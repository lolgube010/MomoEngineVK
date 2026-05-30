#include <vk/engine_rendering.h>
#include <engine_main/engine_scene.h>
#include <vk/DebugDraw.h>
#include "cvars/cvars.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vk/images.h>
#include <vk/initializers.h>
#include <vk/pipelines.h>

#define VMA_LEAK_LOG_FORMAT(format, ...) MOMO_VMA_LEAK_LOG(format, __VA_ARGS__)
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <VkBootstrap.h>

#include <algorithm>
#include <chrono>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>

static momo_cvars::AutoCVar_Int CVAR_Wireframe("r.wireframe", "render geometry as wireframe", 0, momo_cvars::CVarFlags::EditCheckbox);

// ---------------------------------------------------------------------------
// Init / Cleanup
// ---------------------------------------------------------------------------

void EngineRenderer::Init(SDL_Window* aWindow, VkExtent2D aWindowExtent, EngineStats& aStats)
{
    _window       = aWindow;
    _windowExtent = aWindowExtent;
    _pStats       = &aStats;

    _renderDoc.Load();
    Init_Vulkan();
    _renderDoc.Set_Window(_instance, _window);

    Init_Swapchain();
    Init_Commands();
    Init_Sync_Structures();
    Init_Descriptors();
    Init_Pipelines();
    Init_ImGui();
    Init_Tracy();
    Init_Default_Data();
}

void EngineRenderer::Cleanup()
{
    for (auto& frame : _frames)
    {
        vkDestroyCommandPool(_device, frame._commandPool, nullptr);
        vkDestroyFence(_device, frame._renderFence, nullptr);
        vkDestroySemaphore(_device, frame._swapchainSemaphore, nullptr);
        frame._deletionQueue.Flush();
    }

#if TRACY_ENABLE && TRACY_GPU_ENABLE
    if (_tracyVkCtx)
    {
        TracyVkDestroy(_tracyVkCtx)
        _tracyVkCtx = nullptr;
    }
#endif

    _metalRoughMaterial.Clear_Resources(_device);
    DebugDraw::Get().Cleanup(_device, _allocator);
    _imgui.Cleanup(_device);
    _deletionQueue.Flush();
    _gpuResources.Cleanup(_device);
    _swapchain.Cleanup(_device);

    vmaDestroyAllocator(_allocator);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyDevice(_device, nullptr);
    vkb::destroy_debug_utils_messenger(_instance, _debugMessenger);
    _validationCapture.Destroy(_instance);
    vkDestroyInstance(_instance, nullptr);
}

// ---------------------------------------------------------------------------
// Init_Vulkan
// ---------------------------------------------------------------------------

constexpr bool USE_VALIDATION_LAYERS = true;
constexpr auto APP_NAME = "MomoVK";

void EngineRenderer::Init_Vulkan()
{
    PROFILE_SCOPE_N("Init_Vulkan")
    if (auto res = volkInitialize(); res != VK_SUCCESS)
    {
        fmt::print("Failed to initialize volk!\n");
        return;
    }

    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name(APP_NAME)
                           .request_validation_layers(USE_VALIDATION_LAYERS)
                           .use_default_debug_messenger()
                           .require_api_version(1, 3, 0)
                           .build();

    vkb::Instance vkb_inst = inst_ret.value();
    _instance       = vkb_inst.instance;
    _debugMessenger = vkb_inst.debug_messenger;

    volkLoadInstance(_instance);

    if (USE_VALIDATION_LAYERS)
        _validationCapture.Init(_instance);

    SDL_Vulkan_CreateSurface(_window, _instance, nullptr, &_surface);

    VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress                    = true;
    features12.descriptorIndexing                     = true;
    features12.scalarBlockLayout                      = true;
    features12.descriptorBindingPartiallyBound        = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    features12.descriptorBindingSampledImageUpdateAfterBind = true;
    features12.runtimeDescriptorArray                 = true;
    features12.hostQueryReset                         = true;

    VkPhysicalDeviceFeatures features10{};
    features10.shaderInt64       = true;
    features10.fillModeNonSolid  = true;

    vkb::PhysicalDeviceSelector selector{vkb_inst};
    auto phys_ret = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_required_features(features10)
        .set_surface(_surface)
        .select();

    if (!phys_ret)
        throw std::runtime_error("failed to find a suitable GPU: " + phys_ret.error().message());

    vkb::PhysicalDevice& physicalDevice = phys_ret.value();
    if (const std::vector desiredExtensions = {"VK_EXT_memory_budget", "VK_EXT_calibrated_timestamps"};
        !physicalDevice.enable_extensions_if_present(desiredExtensions))
    {
        fmt::print("failed to load some extensions!\n");
        for (const auto& ext : desiredExtensions)
        {
            if (!physicalDevice.enable_extension_if_present(ext))
                fmt::print("failed to load this specific extension!: {}\n", ext);
        }
    }

    vkb::DeviceBuilder deviceBuilder{physicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();

    _device    = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    volkLoadDevice(_device);
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_DEVICE,   _device,   "_Logical Device");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_INSTANCE, _instance, "_Instance");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT, _debugMessenger, "_DebugMessenger");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SURFACE_KHR, _surface, "_Surface");

    {
        VkPhysicalDeviceDriverProperties driverProps{};
        driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        VkPhysicalDeviceProperties2 deviceProps2{};
        deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProps2.pNext = &driverProps;
        vkGetPhysicalDeviceProperties2(_chosenGPU, &deviceProps2);

        const VkPhysicalDeviceProperties& props = deviceProps2.properties;
        fmt::print("--- Physical Device Properties ---\n");
        fmt::print("Selected GPU: {}\n",    props.deviceName);
        fmt::print("Device Type: {}\n",     Get_Device_Type_String(props.deviceType));
        fmt::print("VK API Version: {}.{}.{}\n",
                   VK_API_VERSION_MAJOR(props.apiVersion),
                   VK_API_VERSION_MINOR(props.apiVersion),
                   VK_API_VERSION_PATCH(props.apiVersion));
        fmt::print("Driver Name: {}\n",     driverProps.driverName);
        fmt::print("Driver Info: {}\n",     driverProps.driverInfo);
        fmt::print("-----------------------------------\n");
    }

    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> allGPUs(deviceCount);
        vkEnumeratePhysicalDevices(_instance, &deviceCount, allGPUs.data());
        for (uint32_t i = 0; i < deviceCount; ++i)
        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(allGPUs[i], &props);
            MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_PHYSICAL_DEVICE, allGPUs[i],
                                   "_Physical Device/GPU {}: {}", i, props.deviceName);
        }
    }

    _graphicsQueue       = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_QUEUE, _graphicsQueue, "_Graphics Queue Main");

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice   = _chosenGPU;
    allocatorInfo.device           = _device;
    allocatorInfo.instance         = _instance;
    allocatorInfo.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT |
                                     VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;

    VmaVulkanFunctions vulkanFunctions = {};
    vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vulkanFunctions);
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
}

const char* EngineRenderer::Get_Device_Type_String(const VkPhysicalDeviceType aType)
{
    switch (aType)
    {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:          return "Other";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
    default:                                      return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void EngineRenderer::Draw(const DrawContext& aDrawContext, const GPUSceneData& aSceneData,
                           int& aFrameNumber, bool& aResizeRequested)
{
    PROFILE_SCOPE_N("Draw")
    {
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "wait for fences");
        PROFILE_SCOPE_N("wait for fences")
        VK_CHECK(vkWaitForFences(_device, 1, &GetCurrentFrame(aFrameNumber)._renderFence, true, 1000000000));
        GetCurrentFrame(aFrameNumber)._deletionQueue.Flush();
        GetCurrentFrame(aFrameNumber)._frameDescriptors.Clear_Pools(_device);
    }

    uint32_t swapchainImageIndex;
    {
        PROFILE_SCOPE_N("request image from swapchain")
        const VkResult res = vkAcquireNextImageKHR(_device, _swapchain.Get(), 1000000000,
                                                    GetCurrentFrame(aFrameNumber)._swapchainSemaphore,
                                                    nullptr, &swapchainImageIndex);
        // VK_ERROR_OUT_OF_DATE_KHR: acquire failed, semaphore was NOT signaled. Safe to bail early.
        // VK_SUBOPTIMAL_KHR: acquire succeeded, semaphore IS signaled, image index is valid.
        // Must continue and render this frame; returning early would orphan the semaphore.
        // The swapchain is recreated after present instead.
        if (res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            aResizeRequested = true;
            return;
        }
        if (res == VK_SUBOPTIMAL_KHR)
            aResizeRequested = true;
        else
            VK_CHECK(res);
    }

    VK_CHECK(vkResetFences(_device, 1, &GetCurrentFrame(aFrameNumber)._renderFence));

    const VkCommandBuffer& cmd = GetCurrentFrame(aFrameNumber)._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    {
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "begin command buffer");
        PROFILE_SCOPE_N("begin command buffer")
        const VkCommandBufferBeginInfo cmdBeginInfo = momo_vkInit::command_buffer_begin_info(
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        _drawExtent.height = static_cast<uint32_t>(static_cast<float>(
            std::min(_swapchain.GetExtent().height, _drawImage._imageExtent.height)));
        _drawExtent.width  = static_cast<uint32_t>(static_cast<float>(
            std::min(_swapchain.GetExtent().width, _drawImage._imageExtent.width)));

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
        PROFILE_GPU_COLLECT(_tracyVkCtx, cmd)
    }

    {
        // PROFILE_GPU's destructor writes a timestamp via vkCmdWriteTimestamp,
        // so it must run before vkEndCommandBuffer. Keep it scoped inside this block.
        PROFILE_GPU(_tracyVkCtx, cmd, "Render")
        PROFILE_SCOPE_N("Render")
        {
            PROFILE_SCOPE_N("transition draw img 1")
            PROFILE_GPU(_tracyVkCtx, cmd, "transition draw img 1")
            MOMO_VK_SCOPED_CMD_LABEL(cmd, "transition draw img 1");
            momo_vkUtil::transition_image(cmd, _drawImage._image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, _drawImage._imageFormat);
            momo_vkUtil::transition_image(cmd, _depthImage._image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, _depthImage._imageFormat);
        }
        {
            PROFILE_GPU(_tracyVkCtx, cmd, "draw main")
            MOMO_VK_SCOPED_CMD_LABEL(cmd, "draw main");
            PROFILE_SCOPE_N("draw main")
            Draw_Main(cmd, aDrawContext, aSceneData, aFrameNumber);
        }
        {
            PROFILE_GPU(_tracyVkCtx, cmd, "transition draw & swapchain img 3")
            MOMO_VK_SCOPED_CMD_LABEL(cmd, "transition draw & swapchain img 3");
            momo_vkUtil::transition_image(cmd, _drawImage._image,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _drawImage._imageFormat);
            momo_vkUtil::transition_image(cmd, _swapchain.GetImages()[swapchainImageIndex],
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _swapchain.GetFormat());
            momo_vkUtil::copy_image_to_image(cmd, _drawImage._image,
                _swapchain.GetImages()[swapchainImageIndex], _drawExtent, _swapchain.GetExtent());
            momo_vkUtil::transition_image(cmd, _swapchain.GetImages()[swapchainImageIndex],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, _swapchain.GetFormat());
        }
        {
            PROFILE_GPU(_tracyVkCtx, cmd, "Draw imGui Cmd Buffer")
            MOMO_VK_SCOPED_CMD_LABEL(cmd, "Draw imGui Cmd Buffer");
            MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "Draw imGui Graphics Queue");
            PROFILE_SCOPE_N("draw imgui")
            Draw_ImGui_Cmd(cmd, _swapchain.GetImageViews()[swapchainImageIndex]);
        }
        {
            PROFILE_GPU(_tracyVkCtx, cmd, "transition swapchain img 4")
            MOMO_VK_SCOPED_CMD_LABEL(cmd, "transition swapchain img 4");
            PROFILE_SCOPE_N("transition swapchain img 4")
            momo_vkUtil::transition_image(cmd, _swapchain.GetImages()[swapchainImageIndex],
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, _swapchain.GetFormat());
        }
    }

    {
        PROFILE_SCOPE_N("end command buffer")
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "end command buffer");
        VK_CHECK(vkEndCommandBuffer(cmd));
    }
    {
        PROFILE_SCOPE_N("submit command buffer queue")
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "submit command buffer queue");
        const VkCommandBufferSubmitInfo cmdInfo  = momo_vkInit::command_buffer_submit_info(cmd);
        const VkSemaphoreSubmitInfo waitInfo     = momo_vkInit::semaphore_submit_info(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
            GetCurrentFrame(aFrameNumber)._swapchainSemaphore);
        const VkSemaphoreSubmitInfo signalInfo   = momo_vkInit::semaphore_submit_info(
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, _readyForPresentSemaphores[swapchainImageIndex]);
        const VkSubmitInfo2 submit = momo_vkInit::submit_info(&cmdInfo, &signalInfo, &waitInfo);
        VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, GetCurrentFrame(aFrameNumber)._renderFence));
    }
    {
        PROFILE_SCOPE_N("present")
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "present");
        VkSwapchainKHR swapchainHandle = _swapchain.Get();
        const VkPresentInfoKHR presentInfo = momo_vkInit::present_info(
            &swapchainHandle, &_readyForPresentSemaphores[swapchainImageIndex], &swapchainImageIndex);
        if (const VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
            presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
        {
            aResizeRequested = true;
        }
        aFrameNumber++;
    }
}

void EngineRenderer::Draw_ImGui_Cmd(const VkCommandBuffer aCmd, const VkImageView aTargetImageView) const
{
    _imgui.RenderDrawData(aCmd, aTargetImageView, _swapchain.GetExtent(), _device);
}

void EngineRenderer::Draw_Main(VkCommandBuffer aCmd, const DrawContext& aDrawContext,
                                const GPUSceneData& aSceneData, int aFrameNumber)
{
    {
        PROFILE_GPU(_tracyVkCtx, aCmd, "draw background")
        MOMO_VK_SCOPED_CMD_LABEL(aCmd, "draw background");
        PROFILE_SCOPE_N("draw background")
        Draw_Background(aCmd);
    }
    {
        PROFILE_GPU(_tracyVkCtx, aCmd, "transition draw & depth img 2")
        MOMO_VK_SCOPED_CMD_LABEL(aCmd, "transition draw & depth img 2");
        momo_vkUtil::transition_image(aCmd, _drawImage._image,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, _drawImage._imageFormat);
        momo_vkUtil::transition_image(aCmd, _depthImage._image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, _depthImage._imageFormat);
    }
    {
        MOMO_VK_SCOPED_CMD_LABEL(aCmd, "Draw Geometry CmdBuff");
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "Draw Geometry Queue");
        PROFILE_GPU(_tracyVkCtx, aCmd, "Draw Geometry")
        PROFILE_SCOPE_N("Draw Geometry")
        Draw_Geometry(aCmd, aDrawContext, aSceneData, aFrameNumber);
    }
    {
        MOMO_VK_SCOPED_CMD_LABEL(aCmd, "Debug Draw");
        PROFILE_GPU(_tracyVkCtx, aCmd, "Debug Draw")
        DebugDraw::Get().Draw(aCmd, _drawImage._imageView, _drawExtent, aSceneData._viewProj, aFrameNumber);
    }
}

// ---------------------------------------------------------------------------
// Background pass
// ---------------------------------------------------------------------------

void EngineRenderer::Draw_Background(const VkCommandBuffer aCmd) const
{
    const ComputeEffect& effect = _backgroundEffects[_currentBackgroundEffect];
    vkCmdBindPipeline(aCmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect._pipeline);
    vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout,
                             0, 1, &_drawImageDescriptors, 0, nullptr);
    vkCmdPushConstants(aCmd, _computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(ComputePushConstants), &effect._data);
    vkCmdDispatch(aCmd,
                  static_cast<uint32_t>(std::ceil(_drawExtent.width  / 16.0)),
                  static_cast<uint32_t>(std::ceil(_drawExtent.height / 16.0)), 1);
}

// ---------------------------------------------------------------------------
// ImGui
// ---------------------------------------------------------------------------

void EngineRenderer::ImGui_Update(EngineScene& aScene)
{
    _imgui.Update(*this, aScene);
}

// ---------------------------------------------------------------------------
// Geometry pass
// ---------------------------------------------------------------------------

void EngineRenderer::Draw_Geometry(const VkCommandBuffer aCmd, const DrawContext& aDrawContext,
                                    const GPUSceneData& aSceneData, int aFrameNumber)
{
    _pStats->_totalDrawCallCount       = 0;
    _pStats->_triCount                 = 0;
    _pStats->_opaqueDrawCallCount      = 0;
    _pStats->_transparentDrawCallCount = 0;

    auto start = std::chrono::system_clock::now();

    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(aDrawContext._opaqueSurfaces.size());
    for (uint32_t i = 0; i < aDrawContext._opaqueSurfaces.size(); i++)
        if (Is_Visible(aDrawContext._opaqueSurfaces[i], aSceneData._viewProj))
            opaque_draws.push_back(i);

    std::vector<uint32_t> transparent_draws;
    transparent_draws.reserve(aDrawContext._transparentSurfaces.size());
    for (uint32_t i = 0; i < aDrawContext._transparentSurfaces.size(); i++)
        if (Is_Visible(aDrawContext._transparentSurfaces[i], aSceneData._viewProj))
            transparent_draws.push_back(i);

    std::ranges::sort(opaque_draws, [&](const auto& a, const auto& b)
    {
        const RenderObject& ra = aDrawContext._opaqueSurfaces[a];
        const RenderObject& rb = aDrawContext._opaqueSurfaces[b];
        if (ra._material == rb._material) return ra._indexBuffer < rb._indexBuffer;
        return ra._material < rb._material;
    });

    std::ranges::sort(transparent_draws, [&](const auto& a, const auto& b)
    {
        const RenderObject& ra = aDrawContext._transparentSurfaces[a];
        const RenderObject& rb = aDrawContext._transparentSurfaces[b];
        if (ra._material != rb._material) return ra._material < rb._material;
        const glm::vec3 cameraPos = aSceneData._view[3];
        const float distA = distance2(cameraPos, ra._bounds._origin);
        const float distB = distance2(cameraPos, rb._bounds._origin);
        if (distA > distB) return true;
        if (distA < distB) return false;
        return ra._indexBuffer < rb._indexBuffer;
    });

    const VkRenderingAttachmentInfo colorAttachment = momo_vkInit::attachment_info(
        _drawImage._imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingAttachmentInfo depthAttachment = momo_vkInit::depth_attachment_info(
        _depthImage._imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = momo_vkInit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(aCmd, &renderInfo);

    VkViewport viewport{};
    viewport.width    = static_cast<float>(_drawExtent.width);
    viewport.height   = static_cast<float>(_drawExtent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(aCmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = _drawExtent;
    vkCmdSetScissor(aCmd, 0, 1, &scissor);

    *static_cast<GPUSceneData*>(GetCurrentFrame(aFrameNumber)._sceneDataBuffer._info.pMappedData) = aSceneData;

    if (_texCache._dirty && !_texCache._cache.empty())
    {
        const VkWriteDescriptorSet arrayWrite{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding      = 1,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(_texCache._cache.size()),
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = _texCache._cache.data(),
        };
        for (VkDescriptorSet set : _persistentGlobalDescriptors)
        {
            VkWriteDescriptorSet write = arrayWrite;
            write.dstSet = set;
            vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);
        }
        _texCache._dirty = false;
    }

    const VkDescriptorSet globalDescriptor = _persistentGlobalDescriptors[aFrameNumber % FRAME_OVERLAP];

    const bool wireframe = CVAR_Wireframe.Get() != 0;

    MaterialPipeline* lastPipeline  = nullptr;
    MaterialInstance* lastMaterial  = nullptr;
    VkBuffer          lastIndexBuffer = VK_NULL_HANDLE;

    auto draw = [&](const RenderObject& aR)
    {
        MaterialPipeline* const activePipeline = wireframe
            ? ((aR._material->_passType == MaterialPass::Transparent)
                ? &_metalRoughMaterial._transparentWireframePipeline
                : &_metalRoughMaterial._opaqueWireframePipeline)
            : aR._material->_pipeline;

        if (aR._material != lastMaterial)
        {
            MOMO_VK_SCOPED_CMD_LABEL(aCmd, "Switching Material");
            lastMaterial = aR._material;
            if (activePipeline != lastPipeline)
            {
                lastPipeline = activePipeline;
                vkCmdBindPipeline(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline->_pipeline);
                vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        activePipeline->_layout, 0, 1, &globalDescriptor, 0, nullptr);
            }
            vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    activePipeline->_layout, 1, 1, &aR._material->_materialSet, 0, nullptr);
        }
        if (aR._indexBuffer != lastIndexBuffer)
        {
            lastIndexBuffer = aR._indexBuffer;
            vkCmdBindIndexBuffer(aCmd, aR._indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
        GPUDrawPushConstants pushConstants;
        pushConstants._worldMatrix  = aR._transform;
        pushConstants._vertexBuffer = aR._vertexBufferAddress;
        vkCmdPushConstants(aCmd, activePipeline->_layout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

#ifdef MOMOVK_ENABLE_RENDERDOC
        {
            const char* const passStr = (aR._material->_passType == MaterialPass::Transparent) ? "transparent" : "opaque";
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
            _renderDoc.Annotate_Draw(aCmd, aR._matDebugName.data(), aR._meshDebugName.data(), passStr);
#else
            _renderDoc.Annotate_Draw(aCmd, nullptr, nullptr, passStr);
#endif
        }
#endif
        vkCmdDrawIndexed(aCmd, aR._indexCount, 1, aR._firstIndex, 0, 0);
        _pStats->_totalDrawCallCount++;
        _pStats->_triCount += aR._indexCount / 3;
    };

    {
        MOMO_VK_SCOPED_CMD_LABEL(aCmd, "Draw Opaque");
        PROFILE_GPU(_tracyVkCtx, aCmd, "Draw Opaque")
        for (auto& r : opaque_draws)
        {
            const auto& mesh = aDrawContext._opaqueSurfaces[r];
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
            MOMO_VK_SCOPED_CMD_LABEL(aCmd, mesh._combinedDebugLabel);
#endif
            PROFILE_GPU(_tracyVkCtx, aCmd, "aModel")
            draw(mesh);
            _pStats->_opaqueDrawCallCount++;
        }
    }
    {
        MOMO_VK_SCOPED_CMD_LABEL(aCmd, "Draw Transparent");
        PROFILE_GPU(_tracyVkCtx, aCmd, "Draw Transparent")
        for (auto& r : transparent_draws)
        {
            const auto& mesh = aDrawContext._transparentSurfaces[r];
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
            MOMO_VK_SCOPED_CMD_LABEL(aCmd, mesh._combinedDebugLabel);
#endif
            PROFILE_GPU(_tracyVkCtx, aCmd, "aModel")
            draw(mesh);
            _pStats->_transparentDrawCallCount++;
        }
    }

    vkCmdEndRendering(aCmd);

    auto end = std::chrono::system_clock::now();
    _pStats->_meshDrawTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.f;
}

// ---------------------------------------------------------------------------
// GPU resource helpers
// ---------------------------------------------------------------------------

void EngineRenderer::Immediate_Submit(const std::function<void(VkCommandBuffer aCmd)>& aFunction) const { _gpuResources.Immediate_Submit(aFunction); }
AllocatedImage EngineRenderer::Create_Image(VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, const char* aName, bool aMipmapped) const { return _gpuResources.Create_Image(aSize, aFormat, aUsage, aName, aMipmapped); }
AllocatedImage EngineRenderer::Create_Image(const void* aData, VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, const char* aName, bool aMipmapped) const { return _gpuResources.Create_Image(aData, aSize, aFormat, aUsage, aName, aMipmapped); }
AllocatedBuffer EngineRenderer::Create_Buffer(size_t anAllocSize, VkBufferUsageFlags aUsage, VmaMemoryUsage aMemoryUsage, const char* aName) const { return _gpuResources.Create_Buffer(anAllocSize, aUsage, aMemoryUsage, aName); }
void EngineRenderer::Destroy_Image(const AllocatedImage& aImg) const { _gpuResources.Destroy_Image(aImg); }
void EngineRenderer::Destroy_Buffer(const AllocatedBuffer& aBuffer) const { _gpuResources.Destroy_Buffer(aBuffer); }
GPUMeshBuffers EngineRenderer::UploadMesh(std::span<uint32_t> aIndices, std::span<Vertex> aVertices, const char* aMeshName) const { return _gpuResources.UploadMesh(aIndices, aVertices, aMeshName); }

VkDevice EngineRenderer::GetDevice() const
{ return _device; }

VkSampler EngineRenderer::GetDefaultSamplerLinear() const
{ return _defaultSamplerLinear; }

const AllocatedImage& EngineRenderer::GetErrorCheckerboardImage() const
{ return _errorCheckerboardImage; }

const AllocatedImage& EngineRenderer::GetWhiteImage() const
{ return _whiteImage; }

TextureCache& EngineRenderer::GetTexCache()
{ return _texCache; }

GLTFMetallic_Roughness& EngineRenderer::GetMetalRoughMaterial()
{ return _metalRoughMaterial; }

VkExtent2D EngineRenderer::GetWindowExtent() const
{ return _windowExtent; }

FrameData& EngineRenderer::GetCurrentFrame(const int aFrameNumber)
{ return _frames[aFrameNumber % FRAME_OVERLAP]; }

FrameData& EngineRenderer::GetLastFrame(const int aFrameNumber)
{ return _frames[(aFrameNumber - 1) % FRAME_OVERLAP]; }

// ---------------------------------------------------------------------------
// Init — rendering subsystems
// ---------------------------------------------------------------------------

void EngineRenderer::Init_Swapchain()
{
    PROFILE_SCOPE_N("Init_Swapchain")
    _swapchain.Init(_device, _chosenGPU, _surface, _window, _windowExtent);

    const VkExtent3D drawImageExtent{.width = _windowExtent.width, .height = _windowExtent.height, .depth = 1};

    _drawImage._imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage._imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const VkImageCreateInfo rimg_info = momo_vkInit::image_create_info(_drawImage._imageFormat, drawImageUsages, drawImageExtent);

    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage         = VMA_MEMORY_USAGE_AUTO;
    rimg_allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo,
                   &_drawImage._image, &_drawImage._allocation, nullptr);
    vmaSetAllocationName(_allocator, _drawImage._allocation, "Draw Image");

    const VkImageViewCreateInfo rview_info = momo_vkInit::imageview_create_info(
        _drawImage._imageFormat, _drawImage._image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage._imageView));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE,      _drawImage._image,     "_Image Main Draw");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE_VIEW, _drawImage._imageView, "_Image View Main Draw");

    _depthImage._imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage._imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    const VkImageCreateInfo dimg_info = momo_vkInit::image_create_info(_depthImage._imageFormat, depthImageUsages, drawImageExtent);
    vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo,
                   &_depthImage._image, &_depthImage._allocation, nullptr);
    vmaSetAllocationName(_allocator, _depthImage._allocation, "Depth Image");

    const VkImageViewCreateInfo dview_info = momo_vkInit::imageview_create_info(
        _depthImage._imageFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage._imageView));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE,      _depthImage._image,     "_Image Main Depth");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE_VIEW, _depthImage._imageView, "_Image View Main Depth");

    _deletionQueue.Push_Function([this]
    {
        vkDestroyImageView(_device, _drawImage._imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage._image, _drawImage._allocation);
        vkDestroyImageView(_device, _depthImage._imageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
    });
}

void EngineRenderer::Init_Commands()
{
    PROFILE_SCOPE_N("Init_Commands")
    const VkCommandPoolCreateInfo commandPoolInfo = momo_vkInit::command_pool_create_info(
        _graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (auto& frame : _frames)
    {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &frame._commandPool));
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_COMMAND_POOL, frame._commandPool,
                               "_Command Pool Main, FIF: {}", &frame - _frames);

        VkCommandBufferAllocateInfo cmdAllocInfo = momo_vkInit::command_buffer_allocate_info(frame._commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &frame._mainCommandBuffer));
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_COMMAND_BUFFER, frame._mainCommandBuffer,
                               "_Command Buffer Main, FIF: {}", &frame - _frames);
    }

    _gpuResources.Init(_device, _allocator, _graphicsQueue, _graphicsQueueFamily);
}

void EngineRenderer::Init_Sync_Structures()
{
    PROFILE_SCOPE_N("Init_Sync_Structures")
    const VkFenceCreateInfo     fenceCreateInfo     = momo_vkInit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo semaphoreCreateInfo = momo_vkInit::semaphore_create_info();

    for (auto& frame : _frames)
    {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &frame._renderFence));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &frame._swapchainSemaphore));
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_FENCE,     frame._renderFence,       "_RenderFence Frame FIF:{}", &frame - _frames);
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SEMAPHORE, frame._swapchainSemaphore, "_Semaphore Frame Swapchain, FIF:{}", &frame - _frames);
    }

    _readyForPresentSemaphores.resize(_swapchain.GetImageCount());
    for (size_t i = 0; i < _swapchain.GetImageCount(); ++i)
    {
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_readyForPresentSemaphores[i]));
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SEMAPHORE, _readyForPresentSemaphores[i],
                               "_Semaphore Ready For Present, SwapchainImgCount:{}", i);
    }
    // Single closure walks the live vector at flush time so resize-driven recreation
    // (where the count may change) destroys the current handles, not stale ones.
    _deletionQueue.Push_Function([this]
    {
        for (const auto sem : _readyForPresentSemaphores)
            vkDestroySemaphore(_device, sem, nullptr);
    });
}

void EngineRenderer::Init_Descriptors()
{
    PROFILE_SCOPE_N("Init_Descriptors")
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
    {
        {._type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          ._ratio = 1},
        {._type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ._ratio = 1},
        {._type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         ._ratio = 1}
    };
    _globalDescriptorAllocator.Init(_device, 10, sizes, "globalDescriptorAllocator");

    {
        DescriptorLayoutBuilder builder;
        builder.Add_Binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.Build(_device, VK_SHADER_STAGE_COMPUTE_BIT, "drawImage");
    }
    {
        DescriptorLayoutBuilder builder;
        builder.Add_Binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        builder.Add_Binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindFlags{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};

        const std::array<VkDescriptorBindingFlags, 2> flagArray{
            0,
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        };

        builder._bindings[1].descriptorCount = 4048;
        bindFlags.bindingCount  = 2;
        bindFlags.pBindingFlags = flagArray.data();

        _gpuSceneDataDescriptorLayout = builder.Build(_device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, "gpuSceneData",
            &bindFlags, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    }

    _drawImageDescriptors = _globalDescriptorAllocator.Allocate(_device, _drawImageDescriptorLayout, "drawImage");
    {
        DescriptorWriter writer;
        writer.Write_Image(0, _drawImage._imageView, VK_NULL_HANDLE,
                           VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        writer.Update_Set(_device, _drawImageDescriptors);
    }

    _deletionQueue.Push_Function([this]
    {
        _globalDescriptorAllocator.Destroy_Pools(_device);
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
    });

    const char* debugStr = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string debugStr2;
#endif
    for (unsigned int i = 0; i < FRAME_OVERLAP; i++)
    {
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_Sizes =
        {
            {._type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         ._ratio = 3},
            {._type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ._ratio = 4},
        };
        _frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
        debugStr2 = fmt::format("Frame, FIF: {}", i);
        debugStr  = debugStr2.c_str();
#endif
        _frames[i]._frameDescriptors.Init(_device, 1000, frame_Sizes, debugStr,
                                           VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
        debugStr2 = fmt::format("GPUSceneData, FIF: {}", i);
        debugStr  = debugStr2.c_str();
#endif
        _frames[i]._sceneDataBuffer = Create_Buffer(sizeof(GPUSceneData),
                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                     VMA_MEMORY_USAGE_AUTO, debugStr);

        _deletionQueue.Push_Function([this, i]
        {
            _frames[i]._frameDescriptors.Destroy_Pools(_device);
            Destroy_Buffer(_frames[i]._sceneDataBuffer);
        });
    }

    {
        constexpr uint32_t kMaxTextures = 4048;
        const std::array<VkDescriptorPoolSize, 2> poolSizes{{
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         FRAME_OVERLAP},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, FRAME_OVERLAP * kMaxTextures},
        }};
        VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets       = FRAME_OVERLAP;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();
        VK_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_persistentDescPool));

        std::array<uint32_t, FRAME_OVERLAP> varCounts;
        varCounts.fill(kMaxTextures);
        VkDescriptorSetVariableDescriptorCountAllocateInfo varInfo{
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .descriptorSetCount = FRAME_OVERLAP,
            .pDescriptorCounts  = varCounts.data(),
        };

        std::array<VkDescriptorSetLayout, FRAME_OVERLAP> layouts;
        layouts.fill(_gpuSceneDataDescriptorLayout);
        VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.pNext              = &varInfo;
        allocInfo.descriptorPool     = _persistentDescPool;
        allocInfo.descriptorSetCount = FRAME_OVERLAP;
        allocInfo.pSetLayouts        = layouts.data();
        VK_CHECK(vkAllocateDescriptorSets(_device, &allocInfo, _persistentGlobalDescriptors.data()));

        for (uint32_t i = 0; i < FRAME_OVERLAP; i++)
        {
            DescriptorWriter writer;
            writer.Write_Buffer(0, _frames[i]._sceneDataBuffer._buffer,
                                sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            writer.Update_Set(_device, _persistentGlobalDescriptors[i]);
        }

        _deletionQueue.Push_Function([this] { vkDestroyDescriptorPool(_device, _persistentDescPool, nullptr); });
    }
}

void EngineRenderer::Init_Pipelines()
{
    PROFILE_SCOPE_N("Init_Pipelines")
    Init_Background_Pipelines();
    _metalRoughMaterial.Build_Pipelines(_device, _gpuSceneDataDescriptorLayout,
                                         _drawImage._imageFormat, _depthImage._imageFormat);
}

void EngineRenderer::Init_Background_Pipelines()
{
    PROFILE_SCOPE_N("Init_Background_Pipelines")
    VkPushConstantRange pushConstant{};
    pushConstant.size       = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo computeLayout = momo_vkInit::pipeline_layout_create_info();
    computeLayout.pSetLayouts         = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount      = 1;
    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_computePipelineLayout));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, _computePipelineLayout,
                           "_Pipeline Layout Compute Background");

    constexpr auto shaderLang = momo_shaderUtil::ShaderLang::GLSL;
    auto gradientShader = momo_shaderUtil::load_shader("gradient_color", momo_shaderUtil::ShaderType::Compute, shaderLang, _device);
    auto skyShader      = momo_shaderUtil::load_shader("sky",            momo_shaderUtil::ShaderType::Compute, shaderLang, _device);

    VkPipelineShaderStageCreateInfo stageInfo = momo_vkInit::pipeline_shader_stage_create_info(
        VK_SHADER_STAGE_COMPUTE_BIT, gradientShader.value());
    VkComputePipelineCreateInfo computePipelineCreateInfo = momo_vkInit::compute_pipeline_create_info(
        _computePipelineLayout, stageInfo);

    ComputeEffect gradient{};
    gradient._layout   = _computePipelineLayout;
    gradient._name     = "gradient";
    gradient._data._data1 = glm::vec4(1, 0, 0, 1);
    gradient._data._data2 = glm::vec4(0, 0, 1, 1);
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient._pipeline));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_PIPELINE, gradient._pipeline, "_Pipeline Compute Gradient");
    _backgroundEffects.push_back(gradient);

    computePipelineCreateInfo.stage.module = skyShader.value();
    ComputeEffect sky{};
    sky._layout   = _computePipelineLayout;
    sky._name     = "sky";
    sky._data._data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky._pipeline));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_PIPELINE, sky._pipeline, "_Pipeline Compute Sky");
    _backgroundEffects.push_back(sky);

    vkDestroyShaderModule(_device, gradientShader.value(), nullptr);
    vkDestroyShaderModule(_device, skyShader.value(), nullptr);

    _deletionQueue.Push_Function([this, sky, gradient]
    {
        vkDestroyPipelineLayout(_device, _computePipelineLayout, nullptr);
        vkDestroyPipeline(_device, sky._pipeline, nullptr);
        vkDestroyPipeline(_device, gradient._pipeline, nullptr);
    });
}

void EngineRenderer::Init_ImGui()
{
    _imgui.Init(_instance, _chosenGPU, _device, _graphicsQueueFamily, _graphicsQueue, _swapchain.GetImageCount(), _swapchain.GetFormat(), _window);
}

void EngineRenderer::Init_Default_Data()
{
    PROFILE_SCOPE_N("Init_Default_Data")
    _pStats->_frequency = SDL_GetPerformanceFrequency();

    const uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage = Create_Image(&white, {1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, "Default_White");
    const uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage  = Create_Image(&grey,  {1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, "Default_Grey");
    const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage = Create_Image(&black, {1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, "Default_Black");

    const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16> pixels;
    for (int x = 0; x < 16; x++)
        for (int y = 0; y < 16; y++)
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
    _errorCheckerboardImage = Create_Image(pixels.data(), {16,16,1},
                                            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,
                                            "Default_ErrorCheckerboard");

    _texCache.MarkEngineImage(_whiteImage._imageView);
    _texCache.MarkEngineImage(_greyImage._imageView);
    _texCache.MarkEngineImage(_blackImage._imageView);
    _texCache.MarkEngineImage(_errorCheckerboardImage._imageView);

    VkSamplerCreateInfo sampler = momo_vkInit::sampler_create_info(VK_FILTER_NEAREST);
    VK_CHECK(vkCreateSampler(_device, &sampler, nullptr, &_defaultSamplerNearest));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SAMPLER, _defaultSamplerNearest, "_Sampler Default Nearest");

    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    VK_CHECK(vkCreateSampler(_device, &sampler, nullptr, &_defaultSamplerLinear));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SAMPLER, _defaultSamplerLinear, "_Sampler Default Linear");

    _deletionQueue.Push_Function([this]
    {
        vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
        vkDestroySampler(_device, _defaultSamplerLinear, nullptr);
        Destroy_Image(_whiteImage);
        Destroy_Image(_greyImage);
        Destroy_Image(_blackImage);
        Destroy_Image(_errorCheckerboardImage);
    });

    GLTFMetallic_Roughness::MaterialResources materialResources;
    materialResources._colorImage       = _whiteImage;
    materialResources._colorSampler     = _defaultSamplerLinear;
    materialResources._metalRoughImage  = _whiteImage;
    materialResources._metalRoughSampler = _defaultSamplerLinear;

    AllocatedBuffer materialConstants = Create_Buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants),
                                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                       VMA_MEMORY_USAGE_AUTO, "MaterialConstants");
    auto* sceneUniformData = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(materialConstants._info.pMappedData);
    sceneUniformData->_colorFactors      = glm::vec4{1, 1, 1, 1};
    sceneUniformData->_metalRoughFactors = glm::vec4{1, 0.5, 0, 0};

    _deletionQueue.Push_Function([materialConstants, this] { Destroy_Buffer(materialConstants); });

    materialResources._dataBuffer       = materialConstants._buffer;
    materialResources._dataBufferOffset = 0;

    DebugDraw::Get().Init(_device, _allocator, _drawImage._imageFormat);
}

void EngineRenderer::Init_Tracy()
{
    PROFILE_SCOPE_N("Init_Tracy")
#if TRACY_ENABLE && TRACY_GPU_ENABLE
    // TracyVkContext requires the command buffer in the initial state (reset, not begun).
    // Tracy submits it internally; passing a pre-begun buffer causes a double-begin crash.
    VK_CHECK(vkResetCommandBuffer(_gpuResources.GetImmCommandBuffer(), 0));
    _tracyVkCtx = TracyVkContext(_chosenGPU, _device, _graphicsQueue, _gpuResources.GetImmCommandBuffer())
    TracyVkContextName(_tracyVkCtx, "_Main Graphics Queue", sizeof("_Main Graphics Queue") - 1)
#endif
}

void EngineRenderer::Resize_Draw_Images()
{
    vkDestroyImageView(_device, _drawImage._imageView, nullptr);
    vmaDestroyImage(_allocator, _drawImage._image, _drawImage._allocation);
    vkDestroyImageView(_device, _depthImage._imageView, nullptr);
    vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);

    const VkExtent3D drawImageExtent{.width = _windowExtent.width, .height = _windowExtent.height, .depth = 1};

    _drawImage._imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage._imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VmaAllocationCreateInfo img_allocinfo = {};
    img_allocinfo.usage         = VMA_MEMORY_USAGE_AUTO;
    img_allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    const VkImageCreateInfo rimg_info = momo_vkInit::image_create_info(_drawImage._imageFormat, drawImageUsages, drawImageExtent);
    vmaCreateImage(_allocator, &rimg_info, &img_allocinfo, &_drawImage._image, &_drawImage._allocation, nullptr);
    vmaSetAllocationName(_allocator, _drawImage._allocation, "Draw Image");

    const VkImageViewCreateInfo rview_info = momo_vkInit::imageview_create_info(_drawImage._imageFormat, _drawImage._image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage._imageView));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE,      _drawImage._image,     "_Image Main Draw");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE_VIEW, _drawImage._imageView, "_Image View Main Draw");

    _depthImage._imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage._imageExtent = drawImageExtent;

    const VkImageCreateInfo dimg_info = momo_vkInit::image_create_info(_depthImage._imageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, drawImageExtent);
    vmaCreateImage(_allocator, &dimg_info, &img_allocinfo, &_depthImage._image, &_depthImage._allocation, nullptr);
    vmaSetAllocationName(_allocator, _depthImage._allocation, "Depth Image");

    const VkImageViewCreateInfo dview_info = momo_vkInit::imageview_create_info(_depthImage._imageFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage._imageView));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE,      _depthImage._image,     "_Image Main Depth");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE_VIEW, _depthImage._imageView, "_Image View Main Depth");

    DescriptorWriter writer;
    writer.Write_Image(0, _drawImage._imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.Update_Set(_device, _drawImageDescriptors);
}

void EngineRenderer::Resize_Swapchain(VkExtent2D& aWindowExtent)
{
    vkDeviceWaitIdle(_device);
    const uint32_t oldImageCount = _swapchain.GetImageCount();
    _swapchain.Resize(_device, _chosenGPU, _surface, _window, _windowExtent);

    if (_swapchain.GetImageCount() != oldImageCount)
    {
        for (const auto sem : _readyForPresentSemaphores)
            vkDestroySemaphore(_device, sem, nullptr);
        _readyForPresentSemaphores.clear();
        _readyForPresentSemaphores.resize(_swapchain.GetImageCount());
        const VkSemaphoreCreateInfo info = momo_vkInit::semaphore_create_info();
        for (size_t i = 0; i < _readyForPresentSemaphores.size(); ++i)
        {
            VK_CHECK(vkCreateSemaphore(_device, &info, nullptr, &_readyForPresentSemaphores[i]));
            MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SEMAPHORE, _readyForPresentSemaphores[i],
                                   "_Semaphore Ready For Present, SwapchainImgCount:{}", i);
        }
    }

    Resize_Draw_Images();
    aWindowExtent = _windowExtent;
}

// ---------------------------------------------------------------------------
// Culling & utilities
// ---------------------------------------------------------------------------

bool EngineRenderer::Is_Visible(const RenderObject& aObj, const glm::mat4& aViewProj)
{
    const glm::mat4 matrix = aViewProj * aObj._transform;

    const glm::vec4 row0 = {matrix[0][0], matrix[1][0], matrix[2][0], matrix[3][0]};
    const glm::vec4 row1 = {matrix[0][1], matrix[1][1], matrix[2][1], matrix[3][1]};
    const glm::vec4 row2 = {matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2]};
    const glm::vec4 row3 = {matrix[0][3], matrix[1][3], matrix[2][3], matrix[3][3]};

    const std::array planes = {row3 + row0, row3 - row0, row3 + row1, row3 - row1, row2, row3 - row2};

    for (int i = 0; i < 6; i++)
    {
        const float d = glm::dot(glm::vec3(planes[i]), aObj._bounds._origin) + planes[i].w;
        const float r = glm::dot(glm::abs(glm::vec3(planes[i])), aObj._bounds._extents);
        if (d < -r) return false;
    }
    return true;
}

