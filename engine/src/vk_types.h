#pragma once

// The entire codebase will include this header. it will provide widely used default structures and includes.

#include <vector>
#include <functional>
#include <deque>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

struct AllocatedImage
{
    VkImage _image; // equivalent to ID3D11Resource/ID3D11Texture2D
    VkImageView _imageView; // in vulkan, RTV/SRV/DSV/UAV don't exist, instead this generic one for all of them
    VmaAllocation _allocation; // tracks memory, VkDeviceMemory
    VkExtent3D _imageExtent; // stores width height depth
    VkFormat _imageFormat; // stores format of img, like DXGI_FORMAT_R8G8B8_UNORM
    std::string _name;
};

struct AllocatedBuffer
{
	VkBuffer _buffer;
	VmaAllocation _allocation; // VkDeviceMemory
	VmaAllocationInfo _info;
};

struct GPUSceneData
{
    glm::mat4 _view;
    glm::mat4 _proj;
    glm::mat4 _viewProj;
    glm::vec4 _ambientColor;
    glm::vec4 _sunlightDirection; // w for sun power
    glm::vec4 _sunlightColor;
};

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

struct Vertex
{
	glm::vec3 _pos;
	float _uvX;
	glm::vec3 _normal;
	float _uvY;
	glm::vec4 _color;
};

struct GPUMeshBuffers
{
	AllocatedBuffer _indexBuffer;
	AllocatedBuffer _vertexBuffer;
	VkDeviceAddress _vertexBufferAddress;
};

// for our mesh object draws - max size is 128 bytes
struct GPUDrawPushConstants
{
	glm::mat4 _worldMatrix;
	VkDeviceAddress _vertexBuffer;
};

struct DrawContext;

// TODO- MOVE?
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

struct ComputePushConstants // max size is 128 bytes
{
    glm::vec4 _data1 = {};
    glm::vec4 _data2 = {};
    glm::vec4 _data3 = {};
    glm::vec4 _data4 = {};
};

struct ComputeEffect
{
    const char* _name;
    VkPipeline _pipeline;
    VkPipelineLayout _layout;

    ComputePushConstants _data = {};
};

struct DeletionQueue
{
    // TODO: Doing callbacks like this is inefficient at scale, because we are storing whole std::functions for every object we are deleting, which is not going to be optimal.
    // For now, it's going to be fine. But if you need to delete thousands of objects and want them deleted faster, a better implementation would be to store arrays of vulkan handles of various types such as VkImage, VkBuffer, and so on, and then delete those from a loop.

    std::deque<std::function<void()>> _deleters;

    void Push_Function(std::function<void()>&& aFunction) { _deleters.push_back(std::move(aFunction)); }

    void Flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto& deleter : std::ranges::reverse_view(_deleters))
        {
            deleter(); // call functors
        }

        _deleters.clear();
    }
};

#define VK_CHECK(x)                                                          \
    do                                                                       \
    {                                                                        \
        VkResult err = x;                                                    \
        if (err)                                                             \
        {                                                                    \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                         \
        }                                                                    \
    }                                                                        \
    while (0)

struct TextureID
{
    uint32_t _index;
};

