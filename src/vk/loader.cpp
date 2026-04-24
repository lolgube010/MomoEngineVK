#include <stb_image.h>
#include <vk/loader.h>

#include <vk/engine.h>
#include <vk/images.h>
#include <vk/initializers.h>
#include <vk/types.h>
#include <fmt/std.h>
#include <execution>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

void LoadedGLTF::Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx)
{
    // create renderables from the scene nodes
    for (const auto& n : _topNodes)
    {
        n->Draw(aTopMatrix, aCtx); // most probably MeshNode::Draw
    }
}

void LoadedGLTF::ClearAll()
{
    // Important detail with this. 
    // You cant delete a LoadedGLTF within the same frame its being used. 
    // Those structures are still around. 
    // If you want to destroy a LoadedGLTF at runtime, either do a VkQueueWait like we have in the cleanup function, or add it into the per - frame deletion queue and defer it. 
    // We are storing the shared_ptrs to hold LoadedGLTF, so it can abuse the lambda capture functionality to do this.
    auto& creator = VulkanEngine::Get();
    const VkDevice dv = creator._device;

    const VkDescriptorImageInfo fallback{
        .sampler     = creator._defaultSamplerLinear,
        .imageView   = creator._errorCheckerboardImage._imageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    creator._texCache.FreeTextures(_textureIDs, fallback);

    _descriptorPool.Destroy_Pools(dv);
    creator.Destroy_Buffer(_materialDataBuffer);

    for (const auto& v : _meshes)
    {
        creator.Destroy_Buffer(v->_meshBuffers._indexBuffer);
        creator.Destroy_Buffer(v->_meshBuffers._vertexBuffer);
    }

    for (auto& v : _images)
    {
        if (v._image == creator._errorCheckerboardImage._image)
        {
            // don't destroy the default images
            continue;
        }
        creator.Destroy_Image(v);
    }

    for (const auto& sampler : _samplers)
    {
        vkDestroySampler(dv, sampler, nullptr);
    }
}

std::optional<std::shared_ptr<LoadedGLTF>> momo_vkGLTF::load_gltf(std::string_view aFilePath)
{
    PROFILE_SCOPE_N("load_gltf")
    auto& engine = VulkanEngine::Get();
    fmt::print("Loading GLTF: {}\n", aFilePath);

    auto scene = std::make_shared<LoadedGLTF>();
    LoadedGLTF& file = *scene;

    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;

    std::filesystem::path path = aFilePath;

    fastgltf::Asset gltf;
    {
        // MappedGltfFile uses OS memory-mapped I/O — avoids copying the file into a heap buffer
        PROFILE_SCOPE_N("fastgltf parse")
        auto dataResult = fastgltf::MappedGltfFile::FromPath(path);
        if (!dataResult)
        {
            fmt::print(stderr, "Failed to map glTF file '{}': {} ({})\n", aFilePath,
                fastgltf::getErrorName(dataResult.error()), fastgltf::getErrorMessage(dataResult.error()));
            return {};
        }

        fastgltf::MappedGltfFile& data = dataResult.get();

        // OnlyRenderable skips parsing animations and skins which we don't use
        constexpr auto categories = fastgltf::Category::OnlyRenderable;

        if (const auto type = fastgltf::determineGltfFileType(data); 
            type == fastgltf::GltfType::glTF)
        {
            auto load = parser.loadGltf(data, path.parent_path(), gltfOptions, categories);
            if (load)
            {
                gltf = std::move(load.get());
            }
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
            if (load)
            {
                gltf = std::move(load.get());
            }
            else
            {
                fmt::print(stderr, "Failed to load glTF '{}': {} ({})\n", aFilePath, fastgltf::getErrorName(load.error()), fastgltf::getErrorMessage(load.error()));
                return {};
            }
        }
        else
        {
            fmt::print(stderr, "Failed to determine glTF container type for '{}'\n", aFilePath);
            return {};
        }
    }

    // we can estimate the descriptors we will need accurately
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {._type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ._ratio = 3},
        {._type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ._ratio = 3},
        {._type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ._ratio = 1}
    };

    const char* debugName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string debugNameString = fmt::format("glTF Material, Path: {}", aFilePath);
    debugName = debugNameString.c_str();
#endif
    file._descriptorPool.Init(engine._device, static_cast<uint32_t>(gltf.materials.size()), sizes, debugName);

    // load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers)
    {
        VkFilter magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        VkFilter minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        VkSamplerMipmapMode mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        VkSamplerCreateInfo samplerCreateInfo = momo_vkInit::sampler_create_info(magFilter, minFilter, VK_LOD_CLAMP_NONE, 0, mipmapMode);

        VkSampler newSampler;
        VK_CHECK(vkCreateSampler(engine._device, &samplerCreateInfo, nullptr, &newSampler));
        MOMO_VK_SET_DEBUG_NAME(engine._device, VK_OBJECT_TYPE_SAMPLER, newSampler, "_Sampler glTF, Name: {}, Path: {}", sampler.name, aFilePath);

        file._samplers.push_back(newSampler);
    }

    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<TextureID> imageIDs;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // Phase 1 — CPU: decode every image and fill a host-visible staging buffer.
    // No GPU submissions here; we batch them all into one Immediate_Submit below.
    //
    // Each image is independent so decoding runs in parallel across all cores.
    // stbi uses no global state for the decode itself (only stbi_failure_reason() is a global
    // string, so error messages on the rare failure path may be garbled — acceptable).
    // VMA Create_Image / Create_Buffer are internally locked and safe to call concurrently.
    std::vector<std::optional<PendingTextureUpload>> decoded(gltf.images.size());

    {PROFILE_SCOPE_N("load textures")
    std::transform(std::execution::par,
        gltf.images.begin(), gltf.images.end(),
        decoded.begin(),
        [&](fastgltf::Image& aImage)
        {
            return load_image_stbi(gltf, aImage, aFilePath);
        });
    } // load textures

    // Collect results serially — maintains index alignment with gltf.images for material lookups.
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
            // imageIDs.push_back( aEngine.texCache.AddTexture(_materialResources.colorImage.imageView, materialResources.colorSampler, );
        }
        else
        {
            // failed to decode — slot gets a fallback so material indices stay aligned
            images.push_back(engine._errorCheckerboardImage);
            fmt::print("gltf failed to load texture named: {}, filepath: {}\n", gltf.images[i].name, aFilePath);
        }
    }

    // Phase 2 — GPU: copy all staging buffers and generate mipmaps in one command buffer.
    // Previously this was one Immediate_Submit per texture (N serial CPU-GPU sync points).
    { PROFILE_SCOPE_N("upload textures")
        engine.Immediate_Submit([&](const VkCommandBuffer aCmd)
        {
            for (const auto& p : pendingUploads)
            {
                momo_vkUtil::transition_image(aCmd, p._image._image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, p._image._imageFormat);

                VkBufferImageCopy copyRegion = momo_vkInit::buffer_image_copy(p._image._imageExtent, p._image._imageFormat);
                vkCmdCopyBufferToImage(aCmd, p._stagingBuffer._buffer, p._image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                // TODO: And in the upload textures GPU batch — replace the generate_mipmaps call with a loop that copies each mip level individually(since they're all in the staging buffer already), and the final layout transition goes straight to SHADER_READ_ONLY_OPTIMAL without going through the blit chain.
                momo_vkUtil::generate_mipmaps(aCmd, p._image._image, VkExtent2D{.width = p._image._imageExtent.width, .height = p._image._imageExtent.height }, p._image._imageFormat);
            }
        });

        for (auto& p : pendingUploads)
        {
            engine.Destroy_Buffer(p._stagingBuffer);
        }
    } // upload textures

    #ifdef MOMOVK_ENABLE_DEBUG_NAMES
        debugNameString = fmt::format("Material Data, Path: {}", aFilePath);
        debugName = debugNameString.c_str();
    #endif
    { PROFILE_SCOPE_N("load materials")
        GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = nullptr;
        if (!gltf.materials.empty())
        {
            // create buffer to hold the material data.
            // was previously VMA_MEMORY_USAGE_CPU_TO_GPU 
            file._materialDataBuffer = engine.Create_Buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, debugName);
            sceneMaterialConstants = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(file._materialDataBuffer._info.pMappedData);
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
            
            auto passType = MaterialPass::MainColor;
            if (mat.alphaMode == fastgltf::AlphaMode::Blend)
            {
                passType = MaterialPass::Transparent;
            }

            GLTFMetallic_Roughness::MaterialResources materialResources;
            
            // default the material textures
            materialResources._colorImage = engine._whiteImage;
            materialResources._colorSampler = engine._defaultSamplerLinear;
            materialResources._metalRoughImage = engine._whiteImage;
            materialResources._metalRoughSampler = engine._defaultSamplerLinear;

            // set the uniform buffer for the material data
            materialResources._dataBuffer = file._materialDataBuffer._buffer;
            materialResources._dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);
           
            // grab textures from gltf file
            if (mat.pbrData.baseColorTexture.has_value())
            {
                const fastgltf::Texture& tex = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex];
                if (tex.imageIndex.has_value())
                {
                    materialResources._colorImage = images[tex.imageIndex.value()];
                }
                materialResources._colorSampler = tex.samplerIndex.has_value() ? file._samplers[tex.samplerIndex.value()] : engine._defaultSamplerLinear;
            }
            if (mat.pbrData.metallicRoughnessTexture.has_value())
            {
                const fastgltf::Texture& tex = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex];
                if (tex.imageIndex.has_value())
                {
                    materialResources._metalRoughImage = images[tex.imageIndex.value()];
                }
                materialResources._metalRoughSampler = tex.samplerIndex.has_value() ? file._samplers[tex.samplerIndex.value()] : engine._defaultSamplerLinear;
            }

            const TextureID colorID = engine._texCache.AddTexture(materialResources._colorImage._imageView, materialResources._colorSampler);
            const TextureID metalRoughID = engine._texCache.AddTexture(materialResources._metalRoughImage._imageView, materialResources._metalRoughSampler);
            constants._colorTexID = colorID._index;
            constants._metalRoughTexID = metalRoughID._index;
            file._textureIDs.push_back(colorID);
            file._textureIDs.push_back(metalRoughID);

            // write material parameters to buffer
            sceneMaterialConstants[data_index] = constants;


            // build material
            #ifdef MOMOVK_ENABLE_DEBUG_NAMES
                debugNameString = fmt::format("Material Name: {}, Path: {}", mat.name, aFilePath);
                debugName = debugNameString.c_str();
                newMat.get()->debugName = debugName;
            #endif
            newMat->_data = engine._metalRoughMaterial.Write_Material(engine._device, passType, materialResources, file._descriptorPool, debugName);
            data_index++;
        }
    } // load materials

    // use the same vectors for all meshes so that the memory doesn't reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    // TODO- meshoptimizer!
    {PROFILE_SCOPE_N("upload meshes")
    for (fastgltf::Mesh& mesh : gltf.meshes)
    {
        auto newMesh = std::make_shared<MeshAsset>();
        meshes.push_back(newMesh);
        file._meshes.push_back(newMesh);
        newMesh->_name = mesh.name;
        
        // clear the mesh arrays each mesh, we don't want to merge them by error
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
            newSurface._count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
                [&](std::uint32_t idx)
                {
                    indices.push_back(idx + static_cast<uint32_t>(initial_vtx));
                });
            }

            // load vertex positions
            auto minPos = glm::vec3(FLT_MAX);
            auto maxPos = glm::vec3(-FLT_MAX);
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                [&](glm::vec3 v, size_t index)
                {
                    Vertex newVtx;
                    newVtx._pos = v;
                    newVtx._normal = {1, 0, 0};
                    newVtx._color = glm::vec4{1.f};
                    newVtx._uvX = 0;
                    newVtx._uvY = 0;
                    vertices[initial_vtx + index] = newVtx;
                    minPos = glm::min(minPos, v);
                    maxPos = glm::max(maxPos, v);
                });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex],
                [&](glm::vec3 v, size_t index)
                {
                    vertices[initial_vtx + index]._normal = v;
                });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex],
                [&](glm::vec2 v, size_t index)
                {
                    vertices[initial_vtx + index]._uvX = v.x;
                    vertices[initial_vtx + index]._uvY = v.y;
                });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
                [&](glm::vec4 v, size_t index)
                {
                    vertices[initial_vtx + index]._color = v;
                });
            }

            // used to debug normals
            // ReSharper disable once CppDeclaratorNeverUsed
            if (constexpr bool overrideColorsWithNormals = false)
            {
                for (Vertex& vtx : vertices)
                {
                    vtx._color = glm::vec4(glm::normalize(vtx._normal) * 0.5f + 0.5f, 1.f);
                }
            }

            if (p.materialIndex.has_value())
            {
                newSurface._material = materials[p.materialIndex.value()];
            }
            else if (!materials.empty())
            {
                newSurface._material = materials[0];
            }


            // calculate origin and extents from the min/max, use extent length for radius
            newSurface._bounds._origin = (maxPos + minPos) / 2.f;
            newSurface._bounds._extents = (maxPos - minPos) / 2.f;
            newSurface._bounds._sphereRadius = glm::length(newSurface._bounds._extents);

#ifdef MOMOVK_ENABLE_DEBUG_NAMES
            newSurface._combinedDebugLabel = newSurface._material
                ? fmt::format("Mesh: {}, {}", mesh.name, newSurface._material->debugName)
                : fmt::format("Mesh: {}", mesh.name);
#endif

            newMesh->_surfaces.push_back(newSurface);
        }

        // TODO: meshopt right here?
        // from legacy ver: if we ever want to do something with the model data while it still lives on the cpu, THIS is that moment. after this they're gpu only.
        const char* tempMeshName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
        std::string meshName = fmt::format("Mesh Name: {}, Path: {}", newMesh->_name, aFilePath);
        tempMeshName = meshName.c_str();
#endif
        newMesh->_meshBuffers = engine.UploadMesh(indices, vertices, tempMeshName);
    }

    } // upload meshes

    // load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes)
    {
        std::shared_ptr<Node> newNode;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
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
        file._nodes[node.name.c_str()] = newNode; // NOTE: not in og vkguide, probably a bug.
        // file.nodes[node.name.c_str()];

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

                const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                const glm::mat4 rm = glm::toMat4(rot);
                const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                newNode->_localTransform = tm * rm * sm;
            }
        },
        node.transform);
    }

    // run loop again to setup transform hierarchy
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

    // find the top nodes, with no parents
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

VkFilter momo_vkGLTF::extract_filter(const fastgltf::Filter aFilter)
{
    switch (aFilter)
    {
    // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

    // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
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

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

}

std::optional<momo_vkGLTF::PendingTextureUpload> momo_vkGLTF::load_image_stbi(fastgltf::Asset& aAsset, fastgltf::Image& aImage, std::string_view aFilePath)
{
    // Decodes one GLTF image to RGBA pixels, allocates the VkImage and a host-visible staging
    // buffer, and copies the pixels in. Does NOT submit any GPU work — call site batches uploads.
    const auto& engine = VulkanEngine::Get();

    // TODO: Replace this — the entire std::visit block and the three variables before it(lines 555 - 613).This is the "what format is the image data in, decode it to raw RGBA pixels" section.With KTX2, the file already contains BCn / ASTC compressed data with all mip levels baked in — there 's no CPU decode step at all. You' d replace the stbi visitor with a ktxTexture2_CreateFromMemory call that gives you the raw compressed bytes directly.
    
    const char* imgName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    const std::string temp = fmt::format("{}, Path: {}", aImage.name, aFilePath);
    imgName = temp.c_str();
#endif

    int width = 0, height = 0, nrChannels = 0;
    unsigned char* pixels = nullptr;

    std::visit(
        fastgltf::visitor{
            [&](const auto& anArg)
            {
                fmt::print(stderr, "load_image: unhandled source type '{}' for image '{}' at filepath '{}'\n", typeid(anArg).name(), aImage.name, aFilePath);
            },
            [&](const fastgltf::sources::URI& aGLTFImageFilePath)
            {
                if (aGLTFImageFilePath.fileByteOffset != 0)
                {
                    fmt::print(stderr, "load_image: non-zero byte offset not supported (image '{}')\n", aImage.name);
                    return;
                }
                if (!aGLTFImageFilePath.uri.isLocalPath())
                {
                    fmt::print(stderr, "load_image: non-local URI not supported: '{}' (image '{}')\n", aGLTFImageFilePath.uri.string(), aImage.name);
                    return;
                }

                const std::string path(aGLTFImageFilePath.uri.path().begin(), aGLTFImageFilePath.uri.path().end());
                pixels = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
               
                if (!pixels)
                {
                    fmt::print(stderr, "load_image: stbi failed to load '{}': {}\n", path, stbi_failure_reason());
                }
            },
            [&](fastgltf::sources::Array& anArray)
            {
                const auto* bytes = reinterpret_cast<const stbi_uc*>(anArray.bytes.data());
                pixels = stbi_load_from_memory(bytes, static_cast<int>(anArray.bytes.size()), &width, &height, &nrChannels, 4);
                if (!pixels)
                {
                    fmt::print(stderr, "load_image: stbi failed to decode embedded array for image '{}': {}\n", aImage.name, stbi_failure_reason());
                }
            },
            [&](const fastgltf::sources::BufferView& aView)
            {
                const auto& bufferView = aAsset.bufferViews[aView.bufferViewIndex];
                auto& buffer = aAsset.buffers[bufferView.bufferIndex];
                std::visit(fastgltf::visitor{
                    [&](auto& anArg)
                    {
                        fmt::print(stderr, "load_image: unhandled buffer source type '{}' for image '{}'\n", typeid(anArg).name(), aImage.name);
                    },
                    [&](fastgltf::sources::Array& anArray)
                    {
                        const auto* bytes = reinterpret_cast<const stbi_uc*>(anArray.bytes.data() + bufferView.byteOffset);
                        pixels = stbi_load_from_memory(bytes, static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
                        if (!pixels)
                        {
                            fmt::print(stderr, "load_image: stbi failed to decode buffer view for image '{}': {}\n", aImage.name, stbi_failure_reason());
                        }
                    },
                    [&](fastgltf::sources::ByteView& aByteView)
                    {
                        fmt::print("BYTEVIEW RAN WHEN LOADING TEXTURE! MAKE SURE TO FIX THIS ASAP!");
                    }
                },buffer.data);
            },
        },
        aImage.data);

    if (!pixels)
        return std::nullopt;

    const VkExtent3D extent = {.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height), .depth = 1};

    // TODO:
    // Also change these lines(618 - 626) — the format, image usage flags, and staging buffer size :
    // Currently: hardcoded UNORM, TRANSFER_SRC for mipmap blitting, only mip 0 size
    // KTX2 replacement: format comes from ktxTex->vkFormat, no TRANSFER_SRC needed, staging buffer holds all mip levels combined. do ktxTex-->dataSize instead of what im dooing now
 
    // Allocate the destination VkImage. TRANSFER_SRC is needed for mipmap blitting.
    AllocatedImage image = engine.Create_Image(extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, imgName, /*mipmapped=*/true);

    const size_t dataSize = static_cast<size_t>(width) * height * 4;
    const AllocatedBuffer staging = engine.Create_Buffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, imgName);
    memcpy(staging._info.pMappedData, pixels, dataSize);
    stbi_image_free(pixels);

    return PendingTextureUpload{._image = std::move(image), ._stagingBuffer = staging};
}
