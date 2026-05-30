#pragma once
#include "imgui_utils.h"
struct SDL_Window;

struct ImGui_InitInfo
{
    const VkInstance _instance;
    const VkPhysicalDevice _gpu;
    const VkDevice _device;
    const uint32_t _queueFamily;
    const VkQueue _queue;
    const uint32_t _swapchainImageCount;
    const VkFormat _swapchainFormat;
    SDL_Window* _window;
    VkDescriptorPool _descriptorPool;
};

class EngineRenderer;
class EngineScene;
class GameModule;
struct ImGuiContext;

class EngineImGui
{
public:
    void Init(const ImGui_InitInfo& anInfo);
    static void Cleanup();
    static void Begin_Rendering();
    static void End_Rendering();

    static void Run(EngineRenderer& aRenderer, EngineScene& aScene, GameModule& aGameModule);
    static void RenderDrawData(VkCommandBuffer aCmd, VkImageView aTargetImageView, VkExtent2D aSwapchainExtent, VkDevice aDevice);

    ImGuiContext* _context = nullptr;
};