#include "stb_image.h"
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <fmt/std.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

// std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes_Legacy(VulkanEngine* aEngine, const std::filesystem::path& aFilePath)
// {
//     fmt::print("Loading GLTF: {}\n", aFilePath);
//
//     fastgltf::GltfDataBuffer data;
//     data.loadFromFile(aFilePath);
//
//     constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
//
//     fastgltf::Asset gltf;
//     fastgltf::Parser parser{};
//
//     auto load = parser.loadBinaryGLTF(&data, aFilePath.parent_path(), gltfOptions);
//
//     if (load)
//     {
//         gltf = std::move(load.get());
//     }
//     else
//     {
//         fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(load.error()));
//         return {};
//     }
//
//     std::vector<std::shared_ptr<MeshAsset>> meshes;
//
//     // use the same vectors for all meshes so that the memory doesn't reallocate as often
//     std::vector<uint32_t> indices;
//     std::vector<Vertex> vertices;
//     for (fastgltf::Mesh& mesh : gltf.meshes)
//     {
//         MeshAsset newMesh;
//
//         newMesh.name = mesh.name;
//
//         // clear the mesh arrays each mesh, we don't want to merge them by error
//         indices.clear();
//         vertices.clear();
//
//         for (auto&& p : mesh.primitives)
//         {
//             GeoSurface newSurface;
//             newSurface.startIndex = static_cast<uint32_t>(indices.size());
//             newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);
//
//             size_t initial_vtx = vertices.size();
//
//             // load indexes
//             {
//                 fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
//                 indices.reserve(indices.size() + indexAccessor.count);
//
//                 fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
//                                                          [&](std::uint32_t idx)
//                                                          {
//                                                              indices.push_back(idx + static_cast<uint32_t>(initial_vtx));
//                                                          });
//             }
//
//             // load vertex positions, this is guaranteed to be in the file. the other info isn't.
//             {
//                 fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
//                 vertices.resize(vertices.size() + posAccessor.count);
//
//                 fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
//                                                               [&](glm::vec3 v, size_t index)
//                                                               {
//                                                                   Vertex new_vtx;
//                                                                   new_vtx.pos = v;
//                                                                   new_vtx.normal = {1, 0, 0};
//                                                                   new_vtx.color = glm::vec4{1.f};
//                                                                   new_vtx.uv_x = 0;
//                                                                   new_vtx.uv_y = 0;
//                                                                   vertices[initial_vtx + index] = new_vtx;
//                                                               });
//             }
//
//             // load vertex normals. data except position isn't guaranteed to exist so we need to check first.
//             {
//                 auto normals = p.findAttribute("NORMAL");
//                 if (normals != p.attributes.end())
//                 {
//                     fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->second],
//                                                                   [&](glm::vec3 v, size_t index)
//                                                                   {
//                                                                       vertices[initial_vtx + index].normal = v;
//                                                                   });
//                 }
//             }
//
//             // load UVs
//             {
//                 auto uv = p.findAttribute("TEXCOORD_0");
//                 if (uv != p.attributes.end())
//                 {
//                     fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->second],
//                                                                   [&](glm::vec2 v, size_t index)
//                                                                   {
//                                                                       vertices[initial_vtx + index].uv_x = v.x;
//                                                                       vertices[initial_vtx + index].uv_y = v.y;
//                                                                   });
//                 }
//             }
//
//             // load vertex colors
//             {
//                 auto colors = p.findAttribute("COLOR_0");
//                 if (colors != p.attributes.end())
//                 {
//                     fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->second],
//                                                                   [&](glm::vec4 v, size_t index)
//                                                                   {
//                                                                       vertices[initial_vtx + index].color = v;
//                                                                   });
//                 }
//             }
//             newMesh.surfaces.push_back(newSurface);
//         }
//
//         // override the vertex colors with the vertex normals which is useful for debugging
//         constexpr bool OverrideColors = true;
//         if (OverrideColors)
//         {
//             for (Vertex& vtx : vertices)
//             {
//                 vtx.color = glm::vec4(vtx.normal, 1.f);
//             }
//         }
//
//         // if we ever want to do something with the model data while it still lives on the cpu, THIS is that moment. after this they're gpu only.
//
//         // where we create and fill our buffers.
//         newMesh.meshBuffers = aEngine->UploadMesh(indices, vertices);
//
//         meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
//     }
//
//     return meshes;
// }

void LoadedGLTF::Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx)
{
    // create renderables from the scene nodes
    for (const auto& n : topNodes)
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

    descriptorPool.Destroy_Pools(dv);
    creator.Destroy_Buffer(materialDataBuffer);

    for (const auto& v : meshes)
    {
        creator.Destroy_Buffer(v->meshBuffers._indexBuffer);
        creator.Destroy_Buffer(v->meshBuffers._vertexBuffer);
    }

    for (auto& v : images)
    {
        if (v.image == creator._errorCheckerboardImage.image)
        {
            // don't destroy the default images
            continue;
        }
        creator.Destroy_Image(v);
    }

    for (const auto& sampler : samplers)
    {
        vkDestroySampler(dv, sampler, nullptr);
    }
}

std::optional<std::shared_ptr<LoadedGLTF>> momo_GLTF::load_gltf(std::string_view aFilePath)
{
    auto& aEngine = VulkanEngine::Get();
    fmt::print("Loading GLTF: {}\n", aFilePath);

    auto scene = std::make_shared<LoadedGLTF>();
    LoadedGLTF& file = *scene;

    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(aFilePath);

    fastgltf::Asset gltf;

    std::filesystem::path path = aFilePath;

    if (auto type = fastgltf::determineGltfFileType(&data); 
        type == fastgltf::GltfType::glTF)
    {
        auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            fmt::print(stderr, "Failed to load glTF: {} ({})\n", fastgltf::getErrorName(load.error()), fastgltf::getErrorMessage(load.error()));
            return {};
        }
    }
    else if (type == fastgltf::GltfType::GLB)
    {
        auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            fmt::print(stderr, "Failed to load glTF: {} ({})\n", fastgltf::getErrorName(load.error()), fastgltf::getErrorMessage(load.error()));
            return {};
        }
    }
    else
    {
        fmt::print(stderr, "Failed to determine glTF container (if file is GLB or GLTF)\n");
        return {};
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
    file.descriptorPool.Init(aEngine._device, static_cast<uint32_t>(gltf.materials.size()), sizes, debugName);

    // load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers)
    {
        VkSamplerCreateInfo samplerCreateInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerCreateInfo.minLod = 0;

        samplerCreateInfo.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerCreateInfo.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        samplerCreateInfo.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        VK_CHECK(vkCreateSampler(aEngine._device, &samplerCreateInfo, nullptr, &newSampler));
        MOMO_VK_SET_DEBUG_NAME(aEngine._device, VK_OBJECT_TYPE_SAMPLER, newSampler, "_Sampler glTF, Name: {}, Path: {}", sampler.name, aFilePath);

        file.samplers.push_back(newSampler);
    }

    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // load all textures
    for (fastgltf::Image& image : gltf.images)
    {
        if (std::optional<AllocatedImage> img = load_image(gltf, image, aFilePath);
            img.has_value())
        {
            img->name = image.name;
            images.push_back(*img);
            file.images.push_back(*img);
        }
        else
        {
            // we failed to load, so lets give the slot a default texture to not completely break loading
            images.push_back(aEngine._errorCheckerboardImage);
            fmt::print("gltf failed to load texture {}\n", image.name);
        }
    }

    // create buffer to hold the material data.
    // was previously VMA_MEMORY_USAGE_CPU_TO_GPU 
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    debugNameString = fmt::format("Material Data, Path: {}", aFilePath);
    debugName = debugNameString.c_str();
#endif
    GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = nullptr;
    if (!gltf.materials.empty())
    {
        file.materialDataBuffer = aEngine.Create_Buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, debugName);
        sceneMaterialConstants = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(file.materialDataBuffer.info.pMappedData);
    }
    int data_index = 0;

    for (fastgltf::Material& mat : gltf.materials)
    {
        auto newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        file.materials.push_back(newMat);

        GLTFMetallic_Roughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
        constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
        // write material parameters to buffer
        sceneMaterialConstants[data_index] = constants;

        auto passType = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend)
        {
            passType = MaterialPass::Transparent;
        }

        GLTFMetallic_Roughness::MaterialResources materialResources;
        // default the material textures
        materialResources.colorImage = aEngine._whiteImage;
        materialResources.colorSampler = aEngine._defaultSamplerLinear;
        materialResources.metalRoughImage = aEngine._whiteImage;
        materialResources.metalRoughSampler = aEngine._defaultSamplerLinear;

        // set the uniform buffer for the material data
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);
        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value())
        {
            const fastgltf::Texture& tex = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex];
            if (tex.imageIndex.has_value())
                materialResources.colorImage = images[tex.imageIndex.value()];
            materialResources.colorSampler = tex.samplerIndex.has_value()
                ? file.samplers[tex.samplerIndex.value()]
                : aEngine._defaultSamplerLinear;
        }
        // build material
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
        debugNameString = fmt::format("Material Name: {}, Path: {}", mat.name, aFilePath);
        debugName = debugNameString.c_str();
        newMat.get()->debugName = debugName;
#endif
        newMat->data = aEngine.metalRoughMaterial.Write_Material(aEngine._device, passType, materialResources, file.descriptorPool, debugName);
        data_index++;
    }

    // use the same vectors for all meshes so that the memory doesn't reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    // TODO- meshoptimizer!
    for (fastgltf::Mesh& mesh : gltf.meshes)
    {
        auto newMesh = std::make_shared<MeshAsset>();
        meshes.push_back(newMesh);
        file.meshes.push_back(newMesh);
        newMesh->name = mesh.name;
        
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
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

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
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                              [&](glm::vec3 v, size_t index)
                                                              {
                                                                  Vertex newVtx;
                                                                  newVtx.pos = v;
                                                                  newVtx.normal = {1, 0, 0};
                                                                  newVtx.color = glm::vec4{1.f};
                                                                  newVtx.uv_x = 0;
                                                                  newVtx.uv_y = 0;
                                                                  vertices[initial_vtx + index] = newVtx;
                                                              });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->second],
                                                              [&](glm::vec3 v, size_t index)
                                                              {
                                                                  vertices[initial_vtx + index].normal = v;
                                                              });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->second],
                                                              [&](glm::vec2 v, size_t index)
                                                              {
                                                                  vertices[initial_vtx + index].uv_x = v.x;
                                                                  vertices[initial_vtx + index].uv_y = v.y;
                                                              });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->second],
                                                              [&](glm::vec4 v, size_t index)
                                                              {
                                                                  vertices[initial_vtx + index].color = v;
                                                              });
            }

            constexpr bool OverrideColors = false;
            if (OverrideColors)
            {
                for (Vertex& vtx : vertices)
                {
                    vtx.color = glm::vec4(glm::normalize(vtx.normal), 1.f);
                }
            }

            if (p.materialIndex.has_value())
            {
                newSurface.material = materials[p.materialIndex.value()];
            }
            else if (!materials.empty())
            {
                newSurface.material = materials[0];
            }


            // MOMO TODO- can't we do this while fetching the vertices...?
            //loop the vertices of this surface, find min/max bounds
            glm::vec3 minPos = vertices[initial_vtx].pos;
            glm::vec3 maxPos = vertices[initial_vtx].pos;
            for (size_t i = initial_vtx; i < vertices.size(); i++)
            {
                minPos = glm::min(minPos, vertices[i].pos);
                maxPos = glm::max(maxPos, vertices[i].pos);
            }
            // calculate origin and extents from the min/max, use extent length for radius
            newSurface.bounds.origin = (maxPos + minPos) / 2.f;
            newSurface.bounds.extents = (maxPos - minPos) / 2.f;
            newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);
            
            newMesh->surfaces.push_back(newSurface);
        }

        // TODO: meshopt right here?
        // from legacy ver: if we ever want to do something with the model data while it still lives on the cpu, THIS is that moment. after this they're gpu only.
        const char* tempMeshName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
        std::string meshName = fmt::format("Mesh Name: {}, Path: {}", newMesh->name, aFilePath);
        tempMeshName = meshName.c_str();
#endif
        newMesh->meshBuffers = aEngine.UploadMesh(indices, vertices, tempMeshName);
    }

    // load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes)
    {
        std::shared_ptr<Node> newNode;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value())
        {
            newNode = std::make_shared<MeshNode>();
            dynamic_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
        }
        else
        {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        file.nodes[node.name.c_str()] = newNode; // NOTE: not in og vkguide, probably a bug.
        // file.nodes[node.name.c_str()];

        std::visit(fastgltf::visitor{[&](const fastgltf::Node::TransformMatrix& matrix)
                                     {
                                         memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                                     },
                                     [&](const fastgltf::Node::TRS& transform)
                                     {
                                         const glm::vec3 tl(transform.translation[0], transform.translation[1],
                                                      transform.translation[2]);
                                         const glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                                                       transform.rotation[2]);
                                         const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                                         const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                                         const glm::mat4 rm = glm::toMat4(rot);
                                         const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                                         newNode->localTransform = tm * rm * sm;
                                     }},
                   node.transform);
    }

    // run loop again to setup transform hierarchy
    for (size_t i = 0; i < gltf.nodes.size(); i++)
    {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];

        for (auto& c : node.children)
        {
            sceneNode->children.push_back(nodes[c]);
            nodes[c]->parent = sceneNode;
        }
    }

    // find the top nodes, with no parents
    for (auto& node : nodes)
    {
        if (node->parent.lock() == nullptr)
        {
            file.topNodes.push_back(node);
            node->RefreshTransform(glm::mat4{1.f});
        }
    }
    return scene;

}

VkFilter momo_GLTF::extract_filter(const fastgltf::Filter aFilter)
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

VkSamplerMipmapMode momo_GLTF::extract_mipmap_mode(const fastgltf::Filter aFilter)
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

// TODO:
// For the textures, we are going to load them using stb_image.This is a single - header library to load png, jpeg, and a few others.Sadly, it does not load KTX or DDS formats, which are much better for graphics usages as they can be uploaded almost directly into the GPU and are a compressed format that the GPU reads directly so it saves VRAM.

std::optional<AllocatedImage> momo_GLTF::load_image(fastgltf::Asset& aAsset, fastgltf::Image& aImage, std::string_view aFilePath)
{
    const auto& aEngine = VulkanEngine::Get();

    AllocatedImage newImage{};

    int width, height, nrChannels;

    const char* imgName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    const std::string temp = fmt::format("{}, Path: {}", aImage.name, aFilePath);
    imgName = temp.c_str();
#endif

    std::visit(
        fastgltf::visitor{
            [&](auto& arg)
            {
                fmt::print(stderr, "load_image: unhandled source type '{}' for image '{}'\n",
                    typeid(arg).name(), aImage.name);
            },
            [&](fastgltf::sources::URI& filePath)
            {
                if (filePath.fileByteOffset != 0)
                {
                    fmt::print(stderr, "load_image: non-zero byte offset not supported (image '{}')\n", aImage.name);
                    return;
                }
                if (!filePath.uri.isLocalPath())
                {
                    fmt::print(stderr, "load_image: non-local URI not supported: '{}' (image '{}')\n",
                        filePath.uri.string(), aImage.name);
                    return;
                }

                const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
                unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if (data)
                {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = aEngine.Create_Image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, imgName, true);

                    stbi_image_free(data);
                }
                else
                {
                    fmt::print(stderr, "load_image: stbi failed to load '{}': {}\n", path, stbi_failure_reason());
                }
            },
            [&](fastgltf::sources::Vector& vector)
            {
                unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                                                            &width, &height, &nrChannels, 4);
                if (data)
                {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = aEngine.Create_Image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, imgName, true);

                    stbi_image_free(data);
                }
                else
                {
                    fmt::print(stderr, "load_image: stbi failed to decode embedded vector for image '{}': {}\n",
                        aImage.name, stbi_failure_reason());
                }
            },
            [&](fastgltf::sources::BufferView& view)
            {
                const auto& bufferView = aAsset.bufferViews[view.bufferViewIndex];
                auto& buffer = aAsset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor{
                               [&](auto& arg)
                               {
                                   fmt::print(stderr, "load_image: unhandled buffer source type '{}' for image '{}'\n",
                                       typeid(arg).name(), aImage.name);
                               },
                               [&](fastgltf::sources::Vector& vector)
                               {
                                   unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
                                                                               static_cast<int>(bufferView.byteLength),
                                                                               &width, &height, &nrChannels, 4);
                                   if (data)
                                   {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;

                                       newImage = aEngine.Create_Image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, imgName, true);

                                       stbi_image_free(data);
                                   }
                                   else
                                   {
                                       fmt::print(stderr, "load_image: stbi failed to decode buffer view for image '{}': {}\n",
                                           aImage.name, stbi_failure_reason());
                                   }
                               }},
                    buffer.data);
            },
        },
        aImage.data);

    // if any of the attempts to load the data failed, we haven't written the image, so handle is null
    if (newImage.image == VK_NULL_HANDLE)
    {
        return {};
    }
    return newImage;
}
