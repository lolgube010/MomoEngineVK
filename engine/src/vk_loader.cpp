#include "stb_image.h"
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes_Legacy(VulkanEngine* aEngine, const std::filesystem::path& aFilePath)
{
    std::cout << "Loading GLTF: " << aFilePath << '\n';

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(aFilePath);

    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

    fastgltf::Asset gltf;
    fastgltf::Parser parser{};

    auto load = parser.loadBinaryGLTF(&data, aFilePath.parent_path(), gltfOptions);
    
	if (load) 
    {
        gltf = std::move(load.get());
    }
    else 
    {
        fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(load.error()));
        return {};
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes;

    // use the same vectors for all meshes so that the memory doesn't reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes) 
    {
        MeshAsset newMesh;

        newMesh.name = mesh.name;

        // clear the mesh arrays each mesh, we don't want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) 
        {
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

            // load vertex positions, this is guaranteed to be in the file. the other info isn't.
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index)
                    {
                        Vertex new_vtx;
                        new_vtx.pos = v;
                        new_vtx.normal = { 1, 0, 0 };
                        new_vtx.color = glm::vec4{ 1.f };
                        new_vtx.uv_x = 0;
                        new_vtx.uv_y = 0;
                        vertices[initial_vtx + index] = new_vtx;
                    });
            }

	        // load vertex normals. data except position isn't guaranteed to exist so we need to check first.
            {
	            auto normals = p.findAttribute("NORMAL");
	            if (normals != p.attributes.end()) 
	            {
	                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->second],
	                    [&](glm::vec3 v, size_t index) {
	                        vertices[initial_vtx + index].normal = v;
	                    });
	            }
            }

            // load UVs
            {
	            auto uv = p.findAttribute("TEXCOORD_0");
	            if (uv != p.attributes.end()) 
	            {
	                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->second],
	                    [&](glm::vec2 v, size_t index) {
	                        vertices[initial_vtx + index].uv_x = v.x;
	                        vertices[initial_vtx + index].uv_y = v.y;
	                    });
	            }
            }

            // load vertex colors
            {
	            auto colors = p.findAttribute("COLOR_0");
	            if (colors != p.attributes.end()) 
	            {
	                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->second],
	                    [&](glm::vec4 v, size_t index) {
	                        vertices[initial_vtx + index].color = v;
	                    });
	            }
            }
            newMesh.surfaces.push_back(newSurface);
        }

        // override the vertex colors with the vertex normals which is useful for debugging
        constexpr bool OverrideColors = false;
        if (OverrideColors) 
        {
            for (Vertex& vtx : vertices) 
            {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }

        // if we ever want to do something with the model data while it still lives on the cpu, THIS is that moment. after this they're gpu only.

        // where we create and fill our buffers.
        newMesh.meshBuffers = aEngine->UploadMesh(indices, vertices);

        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
    }

    return meshes;
}

void LoadedGLTF::Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx)
{
    // create renderables from the scene nodes
    for (auto& n : topNodes) 
    {
        n->Draw(aTopMatrix, aCtx);
    }
}

void LoadedGLTF::ClearAll()
{
    // Important detail with this.You cant delete a LoadedGLTF within the same frame its being used.Those structures are still around.If you want to destroy a LoadedGLTF at runtime, either do a VkQueueWait like we have in the cleanup function, or add it into the per - frame deletion queue and defer it.We are storing the shared_ptrs to hold LoadedGLTF, so it can abuse the lambda capture functionality to do this.

    VkDevice dv = creator->_device;

    descriptorPool.Destroy_Pools(dv);
    creator->Destroy_Buffer(materialDataBuffer);

    for (auto& v : meshes | std::views::values) 
    {
        creator->Destroy_Buffer(v->meshBuffers._indexBuffer);
        creator->Destroy_Buffer(v->meshBuffers._vertexBuffer);
    }

    for (auto& v : images | std::views::values) 
    {

        if (v.image == creator->_errorCheckerboardImage.image) 
        {
            // don't destroy the default images
            continue;
        }
        creator->Destroy_Image(v);
    }

    for (auto& sampler : samplers) 
    {
        vkDestroySampler(dv, sampler, nullptr);
    }

}

std::optional<std::shared_ptr<LoadedGLTF>> LoadGLTF(VulkanEngine* engine, std::string_view filePath)
{
    fmt::print("Loading GLTF: {}\n", filePath);

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene;

    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);

    fastgltf::Asset gltf;

    std::filesystem::path path = filePath;

    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::glTF) 
    {
        auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
        if (load) 
        {
            gltf = std::move(load.get());
        }
        else 
        {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << '\n';
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
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << '\n';
            return {};
        }
    }
    else 
    {
        std::cerr << "Failed to determine glTF container (if file is GLB or GLTF)" << '\n';
        return {};
    }

    // we can stimate the descriptors we will need accurately
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { 
    	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } 
    };

    file.descriptorPool.Init(engine->_device, static_cast<uint32_t>(gltf.materials.size()), sizes);

    // load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers)
    {
        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->_device, &sampl, nullptr, &newSampler);

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
        std::optional<AllocatedImage> img = load_image(engine, gltf, image);

        if (img.has_value()) 
        {
            images.push_back(*img);
            file.images[image.name.c_str()] = *img;
        }
        else 
        {
            // we failed to load, so lets give the slot a default white texture to not
            // completely break loading
            images.push_back(engine->_errorCheckerboardImage);
            std::cout << "gltf failed to load texture " << image.name << '\n';
        }
    }

    // create buffer to hold the material data
    file.materialDataBuffer = engine->Create_Buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int data_index = 0;
    GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(file.materialDataBuffer.info.pMappedData);

    for (fastgltf::Material& mat : gltf.materials) 
    {
        auto newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        file.materials[mat.name.c_str()] = newMat;

        GLTFMetallic_Roughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
        constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
        // write material parameters to buffer
        sceneMaterialConstants[data_index] = constants;

        MaterialPass passType = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) 
        {
            passType = MaterialPass::Transparent;
        }

        GLTFMetallic_Roughness::MaterialResources materialResources;
        // default the material textures
        materialResources.colorImage = engine->_whiteImage;
        materialResources.colorSampler = engine->_defaultSamplerLinear;
        materialResources.metalRoughImage = engine->_whiteImage;
        materialResources.metalRoughSampler = engine->_defaultSamplerLinear;

        // set the uniform buffer for the material data
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);
        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) 
        {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            materialResources.colorImage = images[img];
            materialResources.colorSampler = file.samplers[sampler];
        }
        // build material
        newMat->data = engine->metalRoughMaterial.Write_Material(engine->_device, passType, materialResources, file.descriptorPool);

        data_index++;
    }

    // use the same vectors for all meshes so that the memory doesnt reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes) 
    {
        std::shared_ptr<MeshAsset> newMesh = std::make_shared<MeshAsset>();
        meshes.push_back(newMesh);
        file.meshes[mesh.name.c_str()] = newMesh;
        newMesh->name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) 
        {
            GeoSurface newSurface;
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
                    [&](std::uint32_t idx) {
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
                        newVtx.normal = { 1, 0, 0 };
                        newVtx.color = glm::vec4{ 1.f };
                        newVtx.uv_x = 0;
                        newVtx.uv_y = 0;
                        vertices[initial_vtx + index] = newVtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) 
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) 
            {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) 
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].color = v;
                    });
            }

            if (p.materialIndex.has_value()) 
            {
                newSurface.material = materials[p.materialIndex.value()];
            }
            else 
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

        newMesh->meshBuffers = engine->UploadMesh(indices, vertices);
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
        file.nodes[node.name.c_str()];

        std::visit(fastgltf::visitor{ [&](const fastgltf::Node::TransformMatrix& matrix) {
                                          memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                                      },
                       [&](const fastgltf::Node::TRS& transform) {
                           glm::vec3 tl(transform.translation[0], transform.translation[1],
                               transform.translation[2]);
                           glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                               transform.rotation[2]);
                           glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                           glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                           glm::mat4 rm = glm::toMat4(rot);
                           glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                           newNode->localTransform = tm * rm * sm;
                       } },
            node.transform);
    }

    // run loop again to setup transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++) 
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
            node->RefreshTransform(glm::mat4{ 1.f });
        }
    }
    return scene;

}

VkFilter extract_filter(const fastgltf::Filter aFilter)
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

VkSamplerMipmapMode extract_mipmap_mode(const fastgltf::Filter aFilter)
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

std::optional<AllocatedImage> load_image(const VulkanEngine* aEngine, fastgltf::Asset& aAsset, fastgltf::Image& aImage)
{
    AllocatedImage newImage{};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor{
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading
                // local files.

const std::string path(filePath.uri.path().begin(),
    filePath.uri.path().end()); // Thanks C++.
unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
if (data) {
    VkExtent3D imagesize;
    imagesize.width = width;
    imagesize.height = height;
    imagesize.depth = 1;

    newImage = aEngine->Create_Image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

    stbi_image_free(data);
}
},
[&](fastgltf::sources::Vector& vector) {
    unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
        &width, &height, &nrChannels, 4);
    if (data) {
        VkExtent3D imagesize;
        imagesize.width = width;
        imagesize.height = height;
        imagesize.depth = 1;

        newImage = aEngine->Create_Image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

        stbi_image_free(data);
    }
},
[&](fastgltf::sources::BufferView& view) {
    auto& bufferView = aAsset.bufferViews[view.bufferViewIndex];
    auto& buffer = aAsset.buffers[bufferView.bufferIndex];

    std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
        // specify LoadExternalBuffers, meaning all buffers
        // are already loaded into a vector.
[](auto& arg) {},
[&](fastgltf::sources::Vector& vector) {
    unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
        static_cast<int>(bufferView.byteLength),
        &width, &height, &nrChannels, 4);
    if (data) {
        VkExtent3D imagesize;
        imagesize.width = width;
        imagesize.height = height;
        imagesize.depth = 1;

        newImage = aEngine->Create_Image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_SAMPLED_BIT,false);

        stbi_image_free(data);
    }
} },
buffer.data);
},
        },
        aImage.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
        return {};
    }
    else {
        return newImage;
    }
}
