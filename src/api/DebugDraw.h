#pragma once
#include <vk/gpu_types.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <array>
#include <vector>

class DebugDraw
{
public:
    static DebugDraw& Get();

    void Init(VkDevice aDevice, VmaAllocator aAllocator, VkFormat aDrawFormat);
    void Cleanup(VkDevice aDevice, VmaAllocator aAllocator);
    void Draw(VkCommandBuffer aCmd, VkImageView aTargetView, VkExtent2D aDrawExtent, const glm::mat4& aViewProj, int aFrameNumber);

    void Line(glm::vec3 aFrom, glm::vec3 aTo, glm::vec4 aColor);
    void Box(glm::vec3 aMin, glm::vec3 aMax, glm::vec4 aColor);
    void Arrow(glm::vec3 aOrigin, glm::vec3 aDir, float aLength, glm::vec4 aColor);

private:
    struct Vertex { glm::vec3 _pos; uint32_t _color; };
    static constexpr uint32_t MAX_VERTICES = 1 << 16;

    std::vector<Vertex>               _pending;
    std::array<AllocatedBuffer, 2>    _buffers{};
    std::array<VkDeviceAddress, 2>    _addresses{};
    VkPipeline                        _pipeline{};
    VkPipelineLayout                  _layout{};

    static uint32_t PackColor(glm::vec4 aColor);
    DebugDraw() = default;
};
