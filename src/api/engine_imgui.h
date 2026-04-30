#pragma once
#include <vk/gpu_types.h>

struct SDL_Window;
class EngineRenderer;
class EngineScene;

class EngineImGui
{
public:
    void Init(VkInstance aInstance, VkPhysicalDevice aGPU, VkDevice aDevice, uint32_t aQueueFamily, VkQueue aQueue, uint32_t aImageCount, VkFormat aSwapchainFormat, SDL_Window* aWindow);
    void Cleanup(VkDevice aDevice);
    void Update(EngineRenderer& aRenderer, EngineScene& aScene);
    void RenderDrawData(VkCommandBuffer aCmd, VkImageView aTargetImageView, VkExtent2D aSwapchainExtent, VkDevice aDevice) const;

private:
    VkDescriptorPool _imguiPool{};

    void Run(EngineRenderer& aRenderer, EngineScene& aScene);
};
