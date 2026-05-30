#include <vk/loader.h>
#include <vk/loader_stbi.h>

#include <engine_main/engine.h>
#include <vk/images.h>
#include <vk/initializers.h>
#include <vk/render_types.h>
#include <fmt/std.h>
#include <execution>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

// ---------------------------------------------------------------------------
// MeshNode
// ---------------------------------------------------------------------------

void MeshNode::Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx)
{
    const glm::mat4 nodeMatrix = aTopMatrix * _worldTransform;

    for (const auto& s : _mesh->_surfaces)
    {
        RenderObject def;
        def._indexCount           = s._count;
        def._firstIndex           = s._startIndex;
        def._indexBuffer          = _mesh->_meshBuffers._indexBuffer._buffer;
        def._material             = &s._material->_data;
        def._bounds               = s._bounds;
        def._transform            = nodeMatrix;
        def._vertexBufferAddress  = _mesh->_meshBuffers._vertexBufferAddress;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
        def._matDebugName         = s._material->debugName;
        def._meshDebugName        = _mesh->_name;
        def._combinedDebugLabel   = s._combinedDebugLabel.c_str();
#endif
        switch (s._material->_data._passType)
        {
        case MaterialPass::MainColor:
            aCtx._opaqueSurfaces.push_back(def);
            break;
        case MaterialPass::Transparent:
            aCtx._transparentSurfaces.push_back(def);
            break;
        case MaterialPass::Other:
            throw;
        }
    }

    Node::Draw(aTopMatrix, aCtx);
}

// ---------------------------------------------------------------------------
// LoadedGLTF
// ---------------------------------------------------------------------------

void LoadedGLTF::Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx)
{
    for (const auto& n : _topNodes)
        n->Draw(aTopMatrix, aCtx);
}

void LoadedGLTF::ClearAll()
{
    auto& creator = VulkanEngine::Get();
    const VkDevice dv = creator.GetDevice();

    const VkDescriptorImageInfo fallback{
        .sampler     = creator.GetDefaultSamplerLinear(),
        .imageView   = creator.GetErrorCheckerboardImage()._imageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    creator.GetTexCache().FreeTextures(_textureIDs, fallback);

    _descriptorPool.Destroy_Pools(dv);
    creator.Destroy_Buffer(_materialDataBuffer);

    for (const auto& v : _meshes)
    {
        creator.Destroy_Buffer(v->_meshBuffers._indexBuffer);
        creator.Destroy_Buffer(v->_meshBuffers._vertexBuffer);
    }

    for (auto& v : _images)
    {
        if (v._image == creator.GetErrorCheckerboardImage()._image)
            continue;
        creator.Destroy_Image(v);
    }

    for (const auto& sampler : _samplers)
        vkDestroySampler(dv, sampler, nullptr);
}

// ---------------------------------------------------------------------------
// load_gltf
// ---------------------------------------------------------------------------

std::optional<std::shared_ptr<LoadedGLTF>> momo_vkGLTF::load_gltf(std::string_view aFilePath)
{
    PROFILE_SCOPE_N("load_gltf")
    auto& engine = VulkanEngine::Get();
    fmt::print("Loading GLTF: {}\n", aFilePath);

    auto scene = std::make_shared<LoadedGLTF>();
    LoadedGLTF& file = *scene;

    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember |
                                  fastgltf::Options::AllowDouble |
                                  fastgltf::Options::LoadExternalBuffers |
                                  fastgltf::Options::LoadExternalImages;

    std::filesystem::path path = aFilePath;

    fastgltf::Asset gltf;
    {
        PROFILE_SCOPE_N("fastgltf parse")
        auto dataResult = fastgltf::MappedGltfFile::FromPath(path);
        if (!dataResult)
        {
            fmt::print(stderr, "Failed to map glTF file '{}': {} ({})\n", aFilePath,
                       fastgltf::getErrorName(dataResult.error()),
                       fastgltf::getErrorMessage(dataResult.error()));
            return {};
        }

        fastgltf::MappedGltfFile& data = dataResult.get();
        constexpr auto categories = fastgltf::Category::OnlyRenderable;

        if (const auto type = fastgltf::determineGltfFileType(data);
            type == fastgltf::GltfType::glTF)
        {
            auto load = parser.loadGltf(data, path.parent_path(), gltfOptions, categories);
            if (load) gltf = std::move(load.get());
            else
            {
                fmt::print(stderr, "Failed to load glTF '{}': {} ({})\n", aFilePath,
                           fastgltf::getErrorName(load.error()), fastgltf::getErrorMessage(load.error()));
                return {};
            }
        }
        else if (type == fastgltf::GltfType::GLB)
        {
            auto load = parser.loadGltfBinary(data, path.parent_path(), gltfOptions, categories);
            if (load) gltf = std::move(load.get());
            else
            {
                fmt::print(stderr, "Failed to load glTF '{}': {} ({})\n", aFilePath,
                           fastgltf::getErrorName(load.error()), fastgltf::getErrorMessage(load.error()));
                return {};
            }
        }
        else
        {
            fmt::print(stderr, "Failed to determine glTF container type for '{}'\n", aFilePath);
            return {};
        }
    }

    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {._type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ._ratio = 3},
        {._type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         ._ratio = 3},
        {._type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         ._ratio = 1}
    };

    const char* debugName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string debugNameString = fmt::format("glTF Material, Path: {}", aFilePath);
    debugName = debugNameString.c_str();
#endif
    file._descriptorPool.Init(engine.GetDevice(), static_cast<uint32_t>(gltf.materials.size()),
                               sizes, debugName);

    for (fastgltf::Sampler& sampler : gltf.samplers)
    {
        VkFilter magFilter      = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        VkFilter minFilter      = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        VkSamplerMipmapMode mm  = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        VkSamplerCreateInfo sci = momo_vkInit::sampler_create_info(magFilter, minFilter, VK_LOD_CLAMP_NONE, 0, mm);

        VkSampler newSampler;
        VK_CHECK(vkCreateSampler(engine.GetDevice(), &sci, nullptr, &newSampler));
        MOMO_VK_SET_DEBUG_NAME(engine.GetDevice(), VK_OBJECT_TYPE_SAMPLER, newSampler,
                               "_Sampler glTF, Name: {}, Path: {}", sampler.name, aFilePath);
        file._samplers.push_back(newSampler);
    }

    std::vector<std::shared_ptr<MeshAsset>>   meshes;
    std::vector<std::shared_ptr<Node>>         nodes;
    std::vector<AllocatedImage>                images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // Phase 1 — CPU: parallel decode
    std::vector<std::optional<PendingTextureUpload>> decoded(gltf.images.size());
    {PROFILE_SCOPE_N("load textures")
    std::transform(std::execution::par,
        gltf.images.begin(), gltf.images.end(), decoded.begin(),
        [&](fastgltf::Image& aImage) { return load_image_stbi(gltf, aImage, aFilePath); });
    }

    std::vector<PendingTextureUpload> pendingUploads;
    pendingUploads.reserve(gltf.images.size());
    for (size_t i = 0; i < decoded.size(); ++i)
    {
        if (decoded[i].has_value())
        {
            decoded[i]->_image._name = gltf.images[i].name;
            images.push_back(decoded[i]->_image);
            file._images.push_back(decoded[i]->_image);
            pendingUploads.push_back(std::move(*decoded[i]));
        }
        else
        {
            images.push_back(engine.GetErrorCheckerboardImage());
            fmt::print("gltf failed to load texture named: {}, filepath: {}\n",
                       gltf.images[i].name, aFilePath);
        }
    }

    // Phase 2 — GPU: batch upload
    { PROFILE_SCOPE_N("upload textures")
        engine.Immediate_Submit([&](const VkCommandBuffer aCmd)
        {
            for (const auto& p : pendingUploads)
            {
                momo_vkUtil::transition_image(aCmd, p._image._image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    p._image._imageFormat);
                VkBufferImageCopy copyRegion = momo_vkInit::buffer_image_copy(
                    p._image._imageExtent, p._image._imageFormat);
                vkCmdCopyBufferToImage(aCmd, p._stagingBuffer._buffer, p._image._image,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
                momo_vkUtil::generate_mipmaps(aCmd, p._image._image,
                    VkExtent2D{.width = p._image._imageExtent.width,
                               .height = p._image._imageExtent.height},
                    p._image._imageFormat);
            }
        });
        for (auto& p : pendingUploads)
            engine.Destroy_Buffer(p._stagingBuffer);
    }

#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    debugNameString = fmt::format("Material Data, Path: {}", aFilePath);
    debugName = debugNameString.c_str();
#endif
    { PROFILE_SCOPE_N("load materials")
        GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = nullptr;
        if (!gltf.materials.empty())
        {
            file._materialDataBuffer = engine.Create_Buffer(
                sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, debugName);
            sceneMaterialConstants = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(
                file._materialDataBuffer._info.pMappedData);
        }
        int data_index = 0;

        for (fastgltf::Material& mat : gltf.materials)
        {
            auto newMat = std::make_shared<GLTFMaterial>();
            materials.push_back(newMat);
            file._materials.push_back(newMat);

            GLTFMetallic_Roughness::MaterialConstants constants;
            constants._colorFactors.x = mat.pbrData.baseColorFactor[0];
            constants._colorFactors.y = mat.pbrData.baseColorFactor[1];
            constants._colorFactors.z = mat.pbrData.baseColorFactor[2];
            constants._colorFactors.w = mat.pbrData.baseColorFactor[3];
            constants._metalRoughFactors.x = mat.pbrData.metallicFactor;
            constants._metalRoughFactors.y = mat.pbrData.roughnessFactor;

            MaterialPass passType;
            switch (mat.alphaMode)
            {
            case fastgltf::AlphaMode::Opaque:
                passType = MaterialPass::MainColor; constants._alphaCutOff = 0; break;
            case fastgltf::AlphaMode::Mask:
                passType = MaterialPass::MainColor; constants._alphaCutOff = mat.alphaCutoff; break;
            case fastgltf::AlphaMode::Blend:
                passType = MaterialPass::Transparent; constants._alphaCutOff = mat.alphaCutoff; break;
            }

            GLTFMetallic_Roughness::MaterialResources materialResources;
            materialResources._colorImage      = engine.GetWhiteImage();
            materialResources._colorSampler    = engine.GetDefaultSamplerLinear();
            materialResources._metalRoughImage = engine.GetWhiteImage();
            materialResources._metalRoughSampler = engine.GetDefaultSamplerLinear();
            materialResources._dataBuffer       = file._materialDataBuffer._buffer;
            materialResources._dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);

            if (mat.pbrData.baseColorTexture.has_value())
            {
                const fastgltf::Texture& tex = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex];
                if (tex.imageIndex.has_value())
                    materialResources._colorImage = images[tex.imageIndex.value()];
                materialResources._colorSampler = tex.samplerIndex.has_value()
                    ? file._samplers[tex.samplerIndex.value()] : engine.GetDefaultSamplerLinear();
            }
            if (mat.pbrData.metallicRoughnessTexture.has_value())
            {
                const fastgltf::Texture& tex = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex];
                if (tex.imageIndex.has_value())
                    materialResources._metalRoughImage = images[tex.imageIndex.value()];
                materialResources._metalRoughSampler = tex.samplerIndex.has_value()
                    ? file._samplers[tex.samplerIndex.value()] : engine.GetDefaultSamplerLinear();
            }

            const TextureID colorID      = engine.GetTexCache().AddTexture(materialResources._colorImage._imageView, materialResources._colorSampler);
            const TextureID metalRoughID = engine.GetTexCache().AddTexture(materialResources._metalRoughImage._imageView, materialResources._metalRoughSampler);
            constants._colorTexID      = colorID._index;
            constants._metalRoughTexID = metalRoughID._index;
            file._textureIDs.push_back(colorID);
            file._textureIDs.push_back(metalRoughID);

            sceneMaterialConstants[data_index] = constants;

#ifdef MOMOVK_ENABLE_DEBUG_NAMES
            debugNameString = fmt::format("Material Name: {}, Path: {}", mat.name, aFilePath);
            debugName = debugNameString.c_str();
            newMat->debugName = debugName;
#endif
            newMat->_data = engine.GetMetalRoughMaterial().Write_Material(
                engine.GetDevice(), passType, materialResources, file._descriptorPool, debugName);
            data_index++;
        }
    }

    std::vector<uint32_t> indices;
    std::vector<Vertex>   vertices;

    {PROFILE_SCOPE_N("upload meshes")
    for (fastgltf::Mesh& mesh : gltf.meshes)
    {
        auto newMesh = std::make_shared<MeshAsset>();
        meshes.push_back(newMesh);
        file._meshes.push_back(newMesh);
        newMesh->_name = mesh.name;
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives)
        {
            if (!p.indicesAccessor.has_value())
            {
                fmt::print("Skipping non-indexed primitive in mesh '{}'\n", mesh.name);
                continue;
            }

            GeoSurface newSurface;
            newSurface._startIndex = static_cast<uint32_t>(indices.size());
            newSurface._count      = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

            size_t initial_vtx = vertices.size();

            {
                fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);
                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
                    [&](std::uint32_t idx) { indices.push_back(idx + static_cast<uint32_t>(initial_vtx)); });
            }

            auto minPos = glm::vec3(FLT_MAX);
            auto maxPos = glm::vec3(-FLT_MAX);
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index)
                    {
                        Vertex newVtx;
                        newVtx._pos    = v;
                        newVtx._normal = {1, 0, 0};
                        newVtx._color  = glm::vec4{1.f};
                        newVtx._uvX    = 0;
                        newVtx._uvY    = 0;
                        vertices[initial_vtx + index] = newVtx;
                        minPos = glm::min(minPos, v);
                        maxPos = glm::max(maxPos, v);
                    });
            }

            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end())
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex],
                    [&](glm::vec3 v, size_t index) { vertices[initial_vtx + index]._normal = v; });

            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end())
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex],
                    [&](glm::vec2 v, size_t index)
                    {
                        vertices[initial_vtx + index]._uvX = v.x;
                        vertices[initial_vtx + index]._uvY = v.y;
                    });

            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end())
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
                    [&](glm::vec4 v, size_t index) { vertices[initial_vtx + index]._color = v; });

            if (p.materialIndex.has_value())
                newSurface._material = materials[p.materialIndex.value()];
            else if (!materials.empty())
                newSurface._material = materials[0];

            newSurface._bounds._origin       = (maxPos + minPos) / 2.f;
            newSurface._bounds._extents      = (maxPos - minPos) / 2.f;
            newSurface._bounds._sphereRadius = glm::length(newSurface._bounds._extents);

#ifdef MOMOVK_ENABLE_DEBUG_NAMES
            newSurface._combinedDebugLabel = newSurface._material
                ? fmt::format("Mesh: {}, {}", mesh.name, newSurface._material->debugName)
                : fmt::format("Mesh: {}", mesh.name);
#endif
            newMesh->_surfaces.push_back(newSurface);
        }

        const char* tempMeshName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
        std::string meshName = fmt::format("Mesh Name: {}, Path: {}", newMesh->_name, aFilePath);
        tempMeshName = meshName.c_str();
#endif
        newMesh->_meshBuffers = engine.UploadMesh(indices, vertices, tempMeshName);
    }
    }

    for (fastgltf::Node& node : gltf.nodes)
    {
        std::shared_ptr<Node> newNode;
        if (node.meshIndex.has_value())
        {
            newNode = std::make_shared<MeshNode>();
            dynamic_cast<MeshNode*>(newNode.get())->_mesh = meshes[*node.meshIndex];
        }
        else
        {
            newNode = std::make_shared<Node>();
        }
        nodes.push_back(newNode);
        file._nodes[node.name.c_str()] = newNode;

        std::visit(fastgltf::visitor{
            [&](const fastgltf::math::fmat4x4& aMatrix)
            {
                memcpy(&newNode->_localTransform, &aMatrix, sizeof(aMatrix));
            },
            [&](const fastgltf::TRS& aTransform)
            {
                const glm::vec3 tl(aTransform.translation[0], aTransform.translation[1], aTransform.translation[2]);
                const glm::quat rot(aTransform.rotation[3], aTransform.rotation[0], aTransform.rotation[1], aTransform.rotation[2]);
                const glm::vec3 sc(aTransform.scale[0], aTransform.scale[1], aTransform.scale[2]);
                newNode->_localTransform = glm::translate(glm::mat4(1.f), tl) * glm::toMat4(rot) * glm::scale(glm::mat4(1.f), sc);
            }
        }, node.transform);
    }

    for (size_t i = 0; i < gltf.nodes.size(); i++)
    {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];
        for (auto& c : node.children)
        {
            sceneNode->_children.push_back(nodes[c]);
            nodes[c]->_parent = sceneNode;
        }
    }

    for (auto& node : nodes)
    {
        if (node->_parent.lock() == nullptr)
        {
            file._topNodes.push_back(node);
            node->RefreshTransform(glm::mat4{1.f});
        }
    }
    return scene;
}

// ---------------------------------------------------------------------------
// Sampler helpers
// ---------------------------------------------------------------------------

VkFilter momo_vkGLTF::extract_filter(const fastgltf::Filter aFilter)
{
    switch (aFilter)
    {
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode momo_vkGLTF::extract_mipmap_mode(const fastgltf::Filter aFilter)
{
    switch (aFilter)
    {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}
