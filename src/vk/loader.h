#pragma once
#include <vk/render_types.h>
#include <unordered_map>
#include <vk/descriptors.h>
#include <fastgltf/types.hpp>

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
    std::string _combinedDebugLabel;
#endif
};

struct MeshAsset
{
    std::string _name;
    std::vector<GeoSurface> _surfaces;
    GPUMeshBuffers _meshBuffers;
};

struct MeshNode : Node
{
    std::shared_ptr<MeshAsset> _mesh;
    void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) override;
};

class VulkanEngine;

struct LoadedGLTF : IRenderable
{
    std::vector<std::shared_ptr<MeshAsset>> _meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> _nodes;
    std::vector<AllocatedImage> _images;
    std::vector<std::shared_ptr<GLTFMaterial>> _materials;

    std::vector<std::shared_ptr<Node>> _topNodes; // usually MeshNode

    std::vector<VkSampler> _samplers;
    std::vector<TextureID> _textureIDs;

    DescriptorAllocatorGrowable _descriptorPool;

    AllocatedBuffer _materialDataBuffer;

    ~LoadedGLTF() override { ClearAll(); };

    void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) override;

private:
    void ClearAll();
};

namespace Momo_VkGLTF
{
    std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(std::string_view aFilePath);

    VkFilter extract_filter(fastgltf::Filter aFilter);

    VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter aFilter);
}
