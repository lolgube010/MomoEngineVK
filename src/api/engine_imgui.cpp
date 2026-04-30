#include <api/engine_imgui.h>
#include <vk/engine_rendering.h>
#include <engine_scene.h>
#include <vk/initializers.h>
#include <vk/debug.h>
#include <string_utils.h>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include "cvars.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <api/MomoTracy.h>

void EngineImGui::Init(VkInstance aInstance, VkPhysicalDevice aGPU, VkDevice aDevice, uint32_t aQueueFamily, VkQueue aQueue, uint32_t aImageCount, VkFormat aSwapchainFormat, SDL_Window* aWindow)
{
    PROFILE_SCOPE_N("Init_ImGui")
    const VkDescriptorPoolSize pool_sizes[] = {
        {.type = VK_DESCRIPTOR_TYPE_SAMPLER,                    .descriptorCount = 1000},
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     .descriptorCount = 1000},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              .descriptorCount = 1000},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              .descriptorCount = 1000},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,       .descriptorCount = 1000},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,       .descriptorCount = 1000},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             .descriptorCount = 1000},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             .descriptorCount = 1000},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,     .descriptorCount = 1000},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,     .descriptorCount = 1000},
        {.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,           .descriptorCount = 1000}
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets       = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes    = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(aDevice, &pool_info, nullptr, &_imguiPool));
    MOMO_VK_SET_DEBUG_NAME(aDevice, VK_OBJECT_TYPE_DESCRIPTOR_POOL, _imguiPool, "_Descriptor Pool imGui");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsClassic();

    ImGui_ImplSDL3_InitForVulkan(aWindow);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion      = VK_API_VERSION_1_3;
    init_info.Instance        = aInstance;
    init_info.PhysicalDevice  = aGPU;
    init_info.Device          = aDevice;
    init_info.QueueFamily     = aQueueFamily;
    init_info.Queue           = aQueue;
    init_info.DescriptorPool  = _imguiPool;
    init_info.MinImageCount   = aImageCount;
    init_info.ImageCount      = aImageCount;
    init_info.UseDynamicRendering = true;

    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &aSwapchainFormat;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
}

void EngineImGui::Cleanup(VkDevice aDevice)
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(aDevice, _imguiPool, nullptr);
}

void EngineImGui::Update(EngineRenderer& aRenderer, EngineScene& aScene)
{
    PROFILE_SCOPE_N("ImGuiFrame")
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    Run(aRenderer, aScene);
    ImGui::Render();
}

void EngineImGui::Run(EngineRenderer& aRenderer, EngineScene& aScene)
{
    if (ImGui::Begin("settings"))
    {
        aRenderer._validationCapture.DrawImGui();

        if (ImGui::CollapsingHeader("Background"))
        {
            ComputeEffect& selected = aRenderer._backgroundEffects[aRenderer._currentBackgroundEffect];
            ImGui::Text("Selected effect: %s", selected._name);
            ImGui::SliderInt("Effect Index", &aRenderer._currentBackgroundEffect, 0,
                             static_cast<int>(aRenderer._backgroundEffects.size() - 1));
            ImGui::ColorEdit4("data1", reinterpret_cast<float*>(&selected._data._data1));
            ImGui::ColorEdit4("data2", reinterpret_cast<float*>(&selected._data._data2));
            ImGui::ColorEdit4("data3", reinterpret_cast<float*>(&selected._data._data3));
            ImGui::ColorEdit4("data4", reinterpret_cast<float*>(&selected._data._data4));
        }

        if (ImGui::CollapsingHeader("Camera"))
        {
            ImGui::SliderFloat("FOV",        &aScene._mainCamera._tempCameraFov, 1, 180);
            ImGui::Value("Pitch (rad)",      aScene._mainCamera._pitch);
        }

        if (ImGui::CollapsingHeader("CVars"))
            momo_cvars::CVarSystem::Get()->DrawImGuiEditor();

        if (ImGui::CollapsingHeader("Lighting"))
        {
            ImGui::ColorEdit4("Sun Color",     reinterpret_cast<float*>(&aScene._tempSunColor));
            ImGui::DragFloat4("Sun Direction", reinterpret_cast<float*>(&aScene._tempSunDir), 0.1f);
            ImGui::DragFloat4("Ambient Color", reinterpret_cast<float*>(&aScene._tempAmbientColor), 0, 2.f);
        }

        if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
        {
            EngineStats& stats = *aRenderer._pStats;
            ImGui::Text("Frame Time: %.3f ms (%.1f FPS)", stats._frameTime, 1000.0f / stats._frameTime);
            ImGui::Text("Draw Time:   %.3f ms",  stats._meshDrawTime);
            ImGui::Text("Update Time: %.3f ms",  stats._sceneUpdateTime);
            ImGui::Separator();
            ImGui::Text("Triangles:         %s", momo_stringUtils::format_with_commas(stats._triCount).c_str());
            ImGui::Text("Total Draws:       %s", momo_stringUtils::format_with_commas(stats._totalDrawCallCount).c_str());
            ImGui::Text("Opaque Draws:      %s", momo_stringUtils::format_with_commas(stats._opaqueDrawCallCount).c_str());
            ImGui::Text("Transparent Draws: %s", momo_stringUtils::format_with_commas(stats._transparentDrawCallCount).c_str());
            ImGui::Separator();
            ImGui::Text("Models Loaded:        %llu", aScene._loadedModels.size());
            ImGui::Text("Opaque Surfaces:      %llu", aScene.GetDrawContext()._opaqueSurfaces.size());
            ImGui::Text("Transparent Surfaces: %llu", aScene.GetDrawContext()._transparentSurfaces.size());
        }

        if (ImGui::CollapsingHeader("Textures"))
        {
            ImGui::Text("Cache Size:       %llu", aRenderer._texCache.CacheSize());
            ImGui::Text("Engine Defaults:  %llu", aRenderer._texCache.EngineDefaultCount());
            ImGui::Text("Free Slots:       %llu", aRenderer._texCache.FreeSlotCount());
        }

        if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen))
        {
            {
                const VkPhysicalDeviceMemoryProperties* memProps;
                vmaGetMemoryProperties(aRenderer._allocator, &memProps);
                VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
                vmaGetHeapBudgets(aRenderer._allocator, budgets);

                VkDeviceSize totalVramUsage  = 0;
                VkDeviceSize totalVramBudget = 0;
                for (uint32_t i = 0; i < memProps->memoryHeapCount; ++i)
                {
                    if (memProps->memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                    {
                        totalVramUsage  += budgets[i].usage;
                        totalVramBudget += budgets[i].budget;
                    }
                }
                const double usageMB  = static_cast<double>(totalVramUsage)  / (1024.0 * 1024.0);
                const double budgetMB = static_cast<double>(totalVramBudget) / (1024.0 * 1024.0);
                ImGui::Text("VRAM: %.2f MB / %.2f MB", usageMB, budgetMB);
                if (totalVramBudget > 0)
                {
                    const float fraction = static_cast<float>(totalVramUsage) /
                                           static_cast<float>(totalVramBudget);
                    ImGui::ProgressBar(fraction, ImVec2(-1.f, 0.f));
                }
            }
            ImGui::Separator();
            {
                const auto now = std::chrono::steady_clock::now();
                if (now - aRenderer._lastVmaStatsTime >= std::chrono::seconds(5))
                {
                    vmaCalculateStatistics(aRenderer._allocator, &aRenderer._cachedVmaStats);
                    aRenderer._lastVmaStatsTime = now;
                }
                const VmaTotalStatistics& stats = aRenderer._cachedVmaStats;
                const double allocatedMB   = static_cast<double>(stats.total.statistics.allocationBytes) / (1024.0 * 1024.0);
                const double blockMB       = static_cast<double>(stats.total.statistics.blockBytes)      / (1024.0 * 1024.0);
                const double allocMaxMB    = static_cast<double>(stats.total.allocationSizeMax)          / (1024.0 * 1024.0);
                const uint64_t minSize     = (stats.total.statistics.allocationCount == 0) ? 0 : stats.total.allocationSizeMin;
                ImGui::Text("VMA Allocated:  %.2f MB", allocatedMB);
                ImGui::Text("VMA Blocks:     %.2f MB", blockMB);
                ImGui::Text("Alloc Count:    %u",      stats.total.statistics.allocationCount);
                ImGui::Text("Block Count:    %u",      stats.total.statistics.blockCount);
                ImGui::Text("Alloc Max:      %.2f MB", allocMaxMB);
                ImGui::Text("Alloc Min:      %llu B",  minSize);
            }
        }

        if (const bool isRenderDocLoaded = aRenderer._renderDoc.Is_Loaded();
            ImGui::CollapsingHeader("RenderDoc", isRenderDocLoaded ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        {
            if (isRenderDocLoaded)
            {
                if (ImGui::Button("Trigger Capture"))    aRenderer._renderDoc.Trigger_Capture();
                ImGui::SameLine();
                if (ImGui::Button("Open in RenderDoc"))  aRenderer._renderDoc.Launch_Replay_UI();
            }
            else
            {
                ImGui::TextDisabled("Not loaded. Enable CMake option or launch via RenderDoc.");
            }
        }

        ImGui::End();
    }
}

void EngineImGui::RenderDrawData(VkCommandBuffer aCmd, VkImageView aTargetImageView, VkExtent2D aSwapchainExtent, VkDevice aDevice) const
{
    const VkRenderingAttachmentInfo colorAttachment = momo_vkInit::attachment_info(aTargetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = momo_vkInit::rendering_info(aSwapchainExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(aCmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), aCmd);
    vkCmdEndRendering(aCmd);
}
