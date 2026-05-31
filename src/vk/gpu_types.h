#pragma once
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <string>
// ReSharper disable once CppUnusedIncludeDirective
#include <fmt/format.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <vulkan/vk_enum_string_helper.h>

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

struct Vertex
{
	glm::vec3 _pos;
	float _uvX;
	glm::vec3 _normal;
	float _uvY;
	glm::vec4 _color;
};

struct DebugDrawVertex
{
    glm::vec3 _pos;
    uint32_t _color;
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

struct TextureID
{
    uint32_t _index;
};
