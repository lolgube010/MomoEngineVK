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

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

struct AllocatedBuffer
{
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct Vertex
{
	glm::vec3 pos;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
};

// holds buffers for mesh
struct GPUMeshBuffers
{
	AllocatedBuffer _indexBuffer;
	AllocatedBuffer _vertexBuffer;
	VkDeviceAddress _vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants
{
	glm::mat4 _worldMatrix;
	VkDeviceAddress _vertexBuffer;
};

enum class MaterialPass
{
	MainColor,
	Transparent,
	Other
};

struct MaterialPipeline
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance
{
	MaterialPipeline* pipeline; // non owning
	VkDescriptorSet materialSet;
	MaterialPass passType;
};

struct DrawContext;

// base class for a renderable dynamic object
class IRenderable 
{
    virtual void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) = 0;
};

// implementation of a drawable scene node. 
// the scene node can hold children and will also keep a transform to propagate to them
struct Node : public IRenderable {

    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void RefreshTransform(const glm::mat4& aParentMatrix)
    {
        worldTransform = aParentMatrix * localTransform;
        for (const auto c : children) 
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

struct AllocatedImage
{
    VkImage image; // equivalent to ID3D11Resource/ID3D11Texture2D
    VkImageView imageView; // in vulkan, RTV/SRV/DSV/UAV don't exist, instead this generic one for all of them
    VmaAllocation allocation; // tracks memory
    VkExtent3D imageExtent; // stores width height depth
    VkFormat imageFormat; // stores format of img, like DXGI_FORMAT_R8G8B8_UNORM
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