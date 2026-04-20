// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

// The entire codebase will include this header. it will provide widely used default structures and includes.

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <ranges>
#include <unordered_set>

#include <Volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vma/vk_mem_alloc.h>

#include <fmt/core.h>
// #include <fmt/std.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

struct AllocatedImage
{
    VkImage image; // equivalent to ID3D11Resource/ID3D11Texture2D
    VkImageView imageView; // in vulkan, RTV/SRV/DSV/UAV don't exist, instead this generic one for all of them
    VmaAllocation allocation; // tracks memory, VkDeviceMemory
    VkExtent3D imageExtent; // stores width height depth
    VkFormat imageFormat; // stores format of img, like DXGI_FORMAT_R8G8B8_UNORM
    std::string name;
};

struct AllocatedBuffer
{
	VkBuffer buffer;
	VmaAllocation allocation; // VkDeviceMemory
	VmaAllocationInfo info;
};

struct GPUSceneData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
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
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance
{
    MaterialPipeline* pipeline; // non owning
    VkDescriptorSet materialSet; // set of multiple bindings, e.g. an image view and a buffer.
    MaterialPass passType;
};

struct Vertex
{
	glm::vec3 pos;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
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
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform; // "my" transform
    glm::mat4 worldTransform; // transform in the world when multiplied by this node's parents.

    void RefreshTransform(const glm::mat4& aParentMatrix)
    {
        worldTransform = aParentMatrix * localTransform;
        for (const auto& c : children) 
        {
            c->RefreshTransform(worldTransform);
        }
    }

    void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) override
    {
        // draw children
        for (const auto& c : children) 
        {
            c->Draw(aTopMatrix, aCtx);
        }
    }
};

struct ComputePushConstants // max size is 128 bytes
{
    glm::vec4 data1 = {};
    glm::vec4 data2 = {};
    glm::vec4 data3 = {};
    glm::vec4 data4 = {};
};

struct ComputeEffect
{
    const char* name;
    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data = {};
};

struct DeletionQueue
{
    // Doing callbacks like this is inefficient at scale, because we are storing whole std::functions for every object we are deleting, which is not going to be optimal.For the amount of objects we will
    // use in this tutorial, it's going to be fine.but if you need to delete thousands of objects and want them deleted faster, a better implementation would be to store arrays of vulkan handles of
    // various types such as VkImage, VkBuffer, and so on.And then delete those from a loop.

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
    uint32_t Index;
};

