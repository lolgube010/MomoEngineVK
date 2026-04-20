#pragma once
// Will contain GLTF loading logic

#include <vk_types.h>
#include <unordered_map>

#include "vk_descriptors.h"
#include "fastgltf/types.hpp"

struct Bounds 
{
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
    float padding = 0;
};

struct GLTFMaterial
{
    MaterialInstance data;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string debugName = "No Material Name";
#endif
};

struct GeoSurface
{
    uint32_t startIndex;
    uint32_t count;
    Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string combinedDebugLabel; // built once at load: "Mesh: X, Material: Y"
#endif
};

struct MeshAsset 
{
    std::string name;
    std::vector<GeoSurface> surfaces; // submeshes of this specific mesh
    GPUMeshBuffers meshBuffers;
};

class VulkanEngine;

struct LoadedGLTF : IRenderable 
{
    // storage for all the data on a given glTF file
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // nodes that don't have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool; // every materialSet for every surface in this file.

    AllocatedBuffer materialDataBuffer;

    ~LoadedGLTF() override { ClearAll(); };

    void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) override;

private:
    void ClearAll();
};

namespace momo_GLTF
{
    std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(std::string_view aFilePath);

    VkFilter extract_filter(fastgltf::Filter aFilter);

    VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter aFilter);

    // TODO:
    // For the textures, we are going to load them using stb_image. Sadly, it does not load KTX or DDS
    // formats which are much better for GPU usage (compressed, direct upload, pregenerated mipmaps).

    // CPU-side decode result + staging buffer ready for a batched GPU upload.
    // The VkImage is allocated but contains no data yet; Immediate_Submit fills it.
    struct PendingTextureUpload
    {
        AllocatedImage image;
        AllocatedBuffer stagingBuffer;
    };
    std::optional<PendingTextureUpload> load_image_stbi(fastgltf::Asset& aAsset, fastgltf::Image& aImage, std::string_view aFilePath);
}