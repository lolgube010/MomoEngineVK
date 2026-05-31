#include <vk/DebugDraw.h>

#include <Volk/volk.h>
#include <vk/initializers.h>
#include <vk/pipelines.h>
#include <vk/debug.h>

DebugDraw& DebugDraw::Get()
{
    static DebugDraw instance;
    return instance;
}

void DebugDraw::Init(VkDevice aDevice, VmaAllocator aAllocator, VkFormat aDrawFormat)
{
    _pending.reserve(MAX_VERTICES);

    VkBufferCreateInfo bufInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufInfo.size = MAX_VERTICES * sizeof(DebugDrawVertex);
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    for (int i = 0; i < 2; ++i)
    {
        VmaAllocationInfo vmaInfo{};
        vmaCreateBuffer(aAllocator, &bufInfo, &allocInfo,
                        &_buffers[i]._buffer, &_buffers[i]._allocation, &vmaInfo);
        _buffers[i]._info = vmaInfo;

        const VkBufferDeviceAddressInfo addrInfo{
            .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = _buffers[i]._buffer
        };
        _addresses[i] = vkGetBufferDeviceAddress(aDevice, &addrInfo);

        MOMO_VK_SET_DEBUG_NAME(aDevice, VK_OBJECT_TYPE_BUFFER, _buffers[i]._buffer,
                               "_Buffer Debug Lines {}", i);
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof(glm::mat4) + sizeof(VkDeviceAddress);

    VkPipelineLayoutCreateInfo layoutInfo = Momo_VkInit::pipeline_layout_create_info();
    layoutInfo.pPushConstantRanges    = &pushRange;
    layoutInfo.pushConstantRangeCount = 1;
    VK_CHECK(vkCreatePipelineLayout(aDevice, &layoutInfo, nullptr, &_layout));
    MOMO_VK_SET_DEBUG_NAME(aDevice, VK_OBJECT_TYPE_PIPELINE_LAYOUT, _layout, "_Pipeline Layout Debug Lines");

    auto vert = Momo_ShaderUtil::load_shader("debug_line", Momo_ShaderUtil::ShaderType::Vertex,   Momo_ShaderUtil::ShaderLang::GLSL, aDevice);
    auto frag = Momo_ShaderUtil::load_shader("debug_line", Momo_ShaderUtil::ShaderType::Fragment, Momo_ShaderUtil::ShaderLang::GLSL, aDevice);

    PipelineBuilder pb;
    pb.Set_Shaders(vert.value(), frag.value());
    pb.Set_Input_Topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    pb.Set_Polygon_Mode(VK_POLYGON_MODE_FILL);
    pb.Set_Cull_Mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pb.Set_Multisampling_None();
    pb.Enable_Blending_AlphaBlend();
    pb.Disable_DepthTest();
    pb.Set_Color_Attachment_Format(aDrawFormat);
    pb.Set_Depth_Format(VK_FORMAT_UNDEFINED);
    pb._pipelineLayout = _layout;
    _pipeline = pb.Build_Pipeline(aDevice, "Debug Lines");

    vkDestroyShaderModule(aDevice, vert.value(), nullptr);
    vkDestroyShaderModule(aDevice, frag.value(), nullptr);
}

void DebugDraw::Cleanup(const VkDevice aDevice, const VmaAllocator aAllocator) const
{
    for (auto& buf : _buffers)
    {
        vmaDestroyBuffer(aAllocator, buf._buffer, buf._allocation);
    }
    vkDestroyPipeline(aDevice, _pipeline, nullptr);
    vkDestroyPipelineLayout(aDevice, _layout, nullptr);
}

void DebugDraw::Draw(const VkCommandBuffer aCmd, const VkImageView aTargetView, const VkExtent2D aDrawExtent,
                     const glm::mat4& aViewProj, const int aFrameNumber)
{
    if (_pending.empty())
    {
        return;
    }

    const uint32_t count = std::min(static_cast<uint32_t>(_pending.size()), MAX_VERTICES);
    const int slot = aFrameNumber % 2;
    std::memcpy(_buffers[slot]._info.pMappedData, _pending.data(), count * sizeof(DebugDrawVertex));
    _pending.clear();

    const VkRenderingAttachmentInfo colorAttachment = Momo_VkInit::attachment_info(
        aTargetView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = Momo_VkInit::rendering_info(aDrawExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(aCmd, &renderInfo);

    vkCmdBindPipeline(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

    VkViewport viewport{};
    viewport.width    = static_cast<float>(aDrawExtent.width);
    viewport.height   = static_cast<float>(aDrawExtent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(aCmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = aDrawExtent;
    vkCmdSetScissor(aCmd, 0, 1, &scissor);

    struct PushConstants { glm::mat4 viewProj; VkDeviceAddress vb; };
    const PushConstants pc{.viewProj = aViewProj, .vb = _addresses[slot] };
    vkCmdPushConstants(aCmd, _layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    vkCmdDraw(aCmd, count, 1, 0, 0);
    vkCmdEndRendering(aCmd);
}

void DebugDraw::Line(const glm::vec3 aFrom, const glm::vec3 aTo, const glm::vec4 aColor)
{
    if (_pending.size() + 2 > MAX_VERTICES)
    {
        return;
    }
    const uint32_t c = PackColor(aColor);
    _pending.push_back({._pos = aFrom, ._color = c});
    _pending.push_back({._pos = aTo, ._color = c});
}

void DebugDraw::Box(const glm::vec3 aMin, const glm::vec3 aMax, const glm::vec4 aColor)
{
    const glm::vec3 a = aMin, b = aMax;
    // bottom
    Line({a.x,a.y,a.z}, {b.x,a.y,a.z}, aColor);
    Line({b.x,a.y,a.z}, {b.x,a.y,b.z}, aColor);
    Line({b.x,a.y,b.z}, {a.x,a.y,b.z}, aColor);
    Line({a.x,a.y,b.z}, {a.x,a.y,a.z}, aColor);
    // top
    Line({a.x,b.y,a.z}, {b.x,b.y,a.z}, aColor);
    Line({b.x,b.y,a.z}, {b.x,b.y,b.z}, aColor);
    Line({b.x,b.y,b.z}, {a.x,b.y,b.z}, aColor);
    Line({a.x,b.y,b.z}, {a.x,b.y,a.z}, aColor);
    // verticals
    Line({a.x,a.y,a.z}, {a.x,b.y,a.z}, aColor);
    Line({b.x,a.y,a.z}, {b.x,b.y,a.z}, aColor);
    Line({b.x,a.y,b.z}, {b.x,b.y,b.z}, aColor);
    Line({a.x,a.y,b.z}, {a.x,b.y,b.z}, aColor);
}

void DebugDraw::Arrow(const glm::vec3 aOrigin, const glm::vec3 aDir, const float aLength, const glm::vec4 aColor)
{
    const glm::vec3 tip = aOrigin + aDir * aLength;
    Line(aOrigin, tip, aColor);

    const glm::vec3 ref  = glm::abs(aDir.x) < 0.9f ? glm::vec3(1,0,0) : glm::vec3(0,1,0);
    const glm::vec3 perp = glm::normalize(glm::cross(aDir, ref));
    const glm::vec3 perp2 = glm::cross(aDir, perp);

    const float headLen   = aLength * 0.2f;
    const float headWidth = headLen * 0.5f;
    const glm::vec3 base  = tip - aDir * headLen;
    Line(tip, base + perp  * headWidth, aColor);
    Line(tip, base - perp  * headWidth, aColor);
    Line(tip, base + perp2 * headWidth, aColor);
    Line(tip, base - perp2 * headWidth, aColor);
}

uint32_t DebugDraw::PackColor(const glm::vec4 aColor)
{
    return (static_cast<uint32_t>(aColor.r * 255.f) & 0xFF)
         | (static_cast<uint32_t>(aColor.g * 255.f) & 0xFF) <<  8
         | (static_cast<uint32_t>(aColor.b * 255.f) & 0xFF) << 16
         | (static_cast<uint32_t>(aColor.a * 255.f) & 0xFF) << 24;
}
