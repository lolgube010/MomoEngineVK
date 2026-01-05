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
