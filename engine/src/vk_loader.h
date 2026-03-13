#pragma once
// Will contain GLTF loading logic

#include <vk_types.h>
#include <unordered_map>

#include "vk_descriptors.h"
#include "fastgltf/types.hpp"

struct GLTFMaterial
{
    MaterialInstance data;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string debugName = "No Material Name";
#endif
};

struct Bounds 
{
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
    float padding = 0;
};

struct GeoSurface 
{
    uint32_t startIndex;
    uint32_t count;
    Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset 
{
    std::string name;

    std::vector<GeoSurface> surfaces; // submeshes of this specific mesh
    GPUMeshBuffers meshBuffers;
};

class VulkanEngine;

// NOTE: LEGACY
// std optional allows our vector to be errored / null. 
// std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes_Legacy(VulkanEngine* aEngine, const std::filesystem::path& aFilePath);


struct LoadedGLTF : IRenderable 
{
    // storage for all the data on a given glTF file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that don't have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    ~LoadedGLTF() override { ClearAll(); };

    void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) override;

private:
    void ClearAll();
};

namespace momo_GLTF
{
    std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(VulkanEngine* aEngine, std::string_view aFilePath);

    VkFilter extract_filter(fastgltf::Filter aFilter);

    VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter aFilter);

    std::optional<AllocatedImage> load_image(const VulkanEngine* aEngine, fastgltf::Asset& aAsset, fastgltf::Image& aImage, std::string_view aFilePath);
}