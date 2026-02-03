#pragma once
// Will contain GLTF loading logic

#include <vk_types.h>
#include <unordered_map>
#include <filesystem>

#include "vk_descriptors.h"
#include "fastgltf/types.hpp"

struct GLTFMaterial
{
    MaterialInstance data;
};

struct GeoSurface 
{
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset 
{
    std::string name;

    std::vector<GeoSurface> surfaces; // submeshes of this specific mesh
    GPUMeshBuffers meshBuffers;
};

//forward declaration
class VulkanEngine;

// NOTE: LEGACY
// std optional allows our vector to be errored / null. 
std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes_Legacy(VulkanEngine* aEngine, const std::filesystem::path& aFilePath);


struct LoadedGLTF : public IRenderable 
{

    // storage for all the data on a given glTF file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    // TODO- We could be using a singleton instead to avoid storing this pointer if we wanted.
    VulkanEngine* creator;

    ~LoadedGLTF() { ClearAll(); };

    void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) override;

private:

    void ClearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> LoadGLTF(VulkanEngine* engine, std::string_view filePath);

VkFilter extract_filter(fastgltf::Filter aFilter);
VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter aFilter);

std::optional<AllocatedImage> load_image(const VulkanEngine* aEngine, fastgltf::Asset& aAsset, fastgltf::Image& aImage);