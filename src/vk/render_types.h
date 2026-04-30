#pragma once
#include <vk/gpu_types.h>
#include <vk/deletion_queue.h>
#include <memory>
#include <vector>
#include <string_view>

enum class MaterialPass
{
    MainColor,
    Transparent,
    Other
};

// material shaders, input layout, states etc.
struct MaterialPipeline
{
    VkPipeline _pipeline;
    VkPipelineLayout _layout;
};

struct MaterialInstance
{
    MaterialPipeline* _pipeline; // non owning
    VkDescriptorSet _materialSet; // set of multiple bindings, e.g. an image view and a buffer.
    MaterialPass _passType;
};

struct Bounds
{
    glm::vec3 _origin;
    float _sphereRadius;
    glm::vec3 _extents;
    float _padding = 0;
};

struct RenderObject
{
    uint32_t _indexCount;
    uint32_t _firstIndex;
    VkBuffer _indexBuffer;

    MaterialInstance* _material;
    Bounds _bounds;
    glm::mat4 _transform;
    VkDeviceAddress _vertexBufferAddress;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string_view _matDebugName;
    std::string_view _meshDebugName;
    const char* _combinedDebugLabel = nullptr;
#endif
};

struct DrawContext
{
    std::vector<RenderObject> _opaqueSurfaces;
    std::vector<RenderObject> _transparentSurfaces;
};

struct EngineStats
{
    float _frameTime;
    float _sceneUpdateTime;
    float _meshDrawTime;
    uint64_t _frequency;

    uint32_t _triCount;
    int _totalDrawCallCount;
    int _opaqueDrawCallCount;
    int _transparentDrawCallCount;
};

// base class for a renderable dynamic object
class IRenderable
{
public:
    virtual ~IRenderable() = default; // to prevent UB if someone were to delete a derived class through a pointer to this base class.
    virtual void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate to them
struct Node : IRenderable
{
    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> _parent;
    std::vector<std::shared_ptr<Node>> _children;

    glm::mat4 _localTransform; // "my" transform
    glm::mat4 _worldTransform; // transform in the world when multiplied by this node's parents.

    void RefreshTransform(const glm::mat4& aParentMatrix)
    {
        _worldTransform = aParentMatrix * _localTransform;
        for (const auto& c : _children)
        {
            c->RefreshTransform(_worldTransform);
        }
    }

    void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) override
    {
        // draw children
        for (const auto& c : _children)
        {
            c->Draw(aTopMatrix, aCtx);
        }
    }
};
