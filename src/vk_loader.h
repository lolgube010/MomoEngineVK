#pragma once
// Will contain GLTF loading logic

#include <vk_types.h>
#include <unordered_map>

#include "vk_descriptors.h"
#include "fastgltf/types.hpp"

struct Bounds 
{
    glm::vec3 _origin;
    float _sphereRadius;
    glm::vec3 _extents;
    float _padding = 0;
};

struct GLTFMaterial
{
    MaterialInstance _data;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string debugName = "No Material Name";
#endif
};

struct GeoSurface
{
    uint32_t _startIndex;
    uint32_t _count;
    Bounds _bounds;
    std::shared_ptr<GLTFMaterial> _material;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string _combinedDebugLabel; // built once at load: "Mesh: X, Material: Y"
#endif
};

struct MeshAsset 
{
    std::string _name;
    std::vector<GeoSurface> _surfaces; // submeshes of this specific mesh
    GPUMeshBuffers _meshBuffers;
};

class VulkanEngine;

struct LoadedGLTF : IRenderable 
{
    // storage for all the data on a given glTF file
    std::vector<std::shared_ptr<MeshAsset>> _meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> _nodes;
    std::vector<AllocatedImage> _images;
    std::vector<std::shared_ptr<GLTFMaterial>> _materials;

    // nodes that don't have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> _topNodes;

    std::vector<VkSampler> _samplers;
    std::vector<TextureID> _textureIDs; // indices into VulkanEngine::texCache; freed on ClearAll

    DescriptorAllocatorGrowable _descriptorPool; // every materialSet for every surface in this file.

    AllocatedBuffer _materialDataBuffer;

    ~LoadedGLTF() override { ClearAll(); };

    void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) override;

private:
    void ClearAll();
};

namespace momo_vkGLTF
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
        AllocatedImage _image;
        AllocatedBuffer _stagingBuffer;
    };
    std::optional<PendingTextureUpload> load_image_stbi(fastgltf::Asset& aAsset, fastgltf::Image& aImage, std::string_view aFilePath);
}