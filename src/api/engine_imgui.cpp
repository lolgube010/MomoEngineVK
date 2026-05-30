#include <api/engine_imgui.h>
#include <vk/engine_rendering.h>
#include <engine_main/engine_scene.h>
#include <vk/initializers.h>
#include <vk/debug.h>
#include <utils/string_utils.h>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include "cvars/cvars.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <api/MomoTracy.h>
#include <api/imgui_utils.h>

class GameState;

void EngineImGui::Init(const ImGui_InitInfo& anInfo)
{
    PROFILE_SCOPE_N("Init_ImGui")
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsClassic();

    ImGui_ImplSDL3_InitForVulkan(anInfo._window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion        = VK_API_VERSION_1_3;
    init_info.Instance          = anInfo._instance;
    init_info.PhysicalDevice    = anInfo._gpu;
    init_info.Device            = anInfo._device;
    init_info.QueueFamily       = anInfo._queueFamily;
    init_info.Queue             = anInfo._queue;
    init_info.DescriptorPool    = anInfo._descriptorPool;
    init_info.MinImageCount     = anInfo._swapchainImageCount;
    init_info.ImageCount        = anInfo._swapchainImageCount;
    init_info.UseDynamicRendering = true;

    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .pNext = nullptr};
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &anInfo._swapchainFormat;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
}

void EngineImGui::Cleanup()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void EngineImGui::Begin_Rendering()
{
    PROFILE_SCOPE_N("ImGuiFrame")
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void EngineImGui::End_Rendering()
{
    ImGui::Render();
}

void EngineImGui::Run(EngineRenderer& aRenderer, EngineScene& aScene)
{
    if (ImGui::Begin("settings"))
    {
        aRenderer._validationCapture.DrawImGui();

        if (momo_imgui::BeginSection("Engine", momo_imgui::ENGINE_TINT))
        {
        if (momo_imgui::CategoryHeader("CVars", momo_imgui::ENGINE_TINT))
        {
            Momo_Cvars::CVarSystem::Get()->DrawImGuiEditor();
        }

        if (momo_imgui::CategoryHeader("Lighting", momo_imgui::ENGINE_TINT))
        {
            ImGui::ColorEdit4("Sun Color", reinterpret_cast<float*>(&aScene._tempSunColor));
            ImGui::DragFloat4("Sun Direction", reinterpret_cast<float*>(&aScene._tempSunDir), 0.1f);
            ImGui::DragFloat4("Ambient Color", reinterpret_cast<float*>(&aScene._tempAmbientColor), 0, 2.f);
        }

        momo_imgui::EndSection();
        }

        if (momo_imgui::BeginSection("Renderer", momo_imgui::RENDERER_TINT))
        {
        if (momo_imgui::CategoryHeader("Background", momo_imgui::RENDERER_TINT))
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

        if (momo_imgui::CategoryHeader("Stats", momo_imgui::RENDERER_TINT, ImGuiTreeNodeFlags_DefaultOpen))
        {
            EngineStats& stats = *aRenderer._pStats;
            ImGui::Text("Frame Time: %.3f ms (%.1f FPS)", stats._frameTime, 1000.0f / stats._frameTime);
            ImGui::Text("Draw Time:   %.3f ms", stats._meshDrawTime);
            ImGui::Text("Update Time: %.3f ms", stats._sceneUpdateTime);
            ImGui::Separator();
            ImGui::Text("Triangles:         %s", Momo_StringUtils::format_with_commas(stats._triCount).c_str());
            ImGui::Text("Total Draws:       %s", Momo_StringUtils::format_with_commas(stats._totalDrawCallCount).c_str());
            ImGui::Text("Opaque Draws:      %s", Momo_StringUtils::format_with_commas(stats._opaqueDrawCallCount).c_str());
            ImGui::Text("Transparent Draws: %s", Momo_StringUtils::format_with_commas(stats._transparentDrawCallCount).c_str());
            ImGui::Separator();
            ImGui::Text("Models Loaded:        %llu", aScene._loadedModels.size());
            ImGui::Text("Opaque Surfaces:      %llu", aScene.GetDrawContext()._opaqueSurfaces.size());
            ImGui::Text("Transparent Surfaces: %llu", aScene.GetDrawContext()._transparentSurfaces.size());
        }

        if (momo_imgui::CategoryHeader("Textures", momo_imgui::RENDERER_TINT))
        {
            ImGui::Text("Cache Size:       %llu", aRenderer._texCache.CacheSize());
            ImGui::Text("Engine Defaults:  %llu", aRenderer._texCache.EngineDefaultCount());
            ImGui::Text("Free Slots:       %llu", aRenderer._texCache.FreeSlotCount());
        }

        if (momo_imgui::CategoryHeader("Memory", momo_imgui::RENDERER_TINT, ImGuiTreeNodeFlags_DefaultOpen))
        {
            {
                const VkPhysicalDeviceMemoryProperties* memProps;
                vmaGetMemoryProperties(aRenderer._allocator, &memProps);
                VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
                vmaGetHeapBudgets(aRenderer._allocator, budgets);

                VkDeviceSize totalVramUsage = 0;
                VkDeviceSize totalVramBudget = 0;
                for (uint32_t i = 0; i < memProps->memoryHeapCount; ++i)
                {
                    if (memProps->memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                    {
                        totalVramUsage += budgets[i].usage;
                        totalVramBudget += budgets[i].budget;
                    }
                }
                const double usageMB = static_cast<double>(totalVramUsage) / (1024.0 * 1024.0);
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
                if (const auto now = std::chrono::steady_clock::now();
                    now - aRenderer._lastVmaStatsTime >= std::chrono::seconds(5))
                {
                    vmaCalculateStatistics(aRenderer._allocator, &aRenderer._cachedVmaStats);
                    aRenderer._lastVmaStatsTime = now;
                }
                const VmaTotalStatistics& stats = aRenderer._cachedVmaStats;
                const double allocatedMB = static_cast<double>(stats.total.statistics.allocationBytes) / (1024.0 * 1024.0);
                const double blockMB = static_cast<double>(stats.total.statistics.blockBytes) / (1024.0 * 1024.0);
                const double allocMaxMB = static_cast<double>(stats.total.allocationSizeMax) / (1024.0 * 1024.0);
                const uint64_t minSize = (stats.total.statistics.allocationCount == 0) ? 0 : stats.total.allocationSizeMin;
                ImGui::Text("VMA Allocated:  %.2f MB", allocatedMB);
                ImGui::Text("VMA Blocks:     %.2f MB", blockMB);
                ImGui::Text("Alloc Count:    %u", stats.total.statistics.allocationCount);
                ImGui::Text("Block Count:    %u", stats.total.statistics.blockCount);
                ImGui::Text("Alloc Max:      %.2f MB", allocMaxMB);
                ImGui::Text("Alloc Min:      %llu B", minSize);
            }
        }

        if (const bool isRenderDocLoaded = aRenderer._renderDoc.Is_Loaded(); // NOLINT(readability-static-accessed-through-instance)
            momo_imgui::CategoryHeader("RenderDoc", momo_imgui::RENDERER_TINT, isRenderDocLoaded ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        {
            if (isRenderDocLoaded)
            {
                if (ImGui::Button("Trigger Capture"))
                {
                    aRenderer._renderDoc.Trigger_Capture(); // NOLINT(readability-static-accessed-through-instance)
                }
                ImGui::SameLine();
                if (ImGui::Button("Open in RenderDoc"))
                {
                    aRenderer._renderDoc.Launch_Replay_UI(); // NOLINT(readability-static-accessed-through-instance)
                }
            }
            else
            {
                ImGui::TextDisabled("Not loaded. Enable CMake option or launch via RenderDoc.");
            }
        }

        momo_imgui::EndSection();
        }
    }
    ImGui::End();
}

void EngineImGui::RenderDrawData(const VkCommandBuffer aCmd, const VkImageView aTargetImageView, const VkExtent2D aSwapchainExtent, VkDevice aDevice)
{
    const VkRenderingAttachmentInfo colorAttachment = Momo_VkInit::attachment_info(aTargetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = Momo_VkInit::rendering_info(aSwapchainExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(aCmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), aCmd);
    vkCmdEndRendering(aCmd);
}
