#pragma once
#include <vk/gpu_types.h>
#include <vector>

struct SDL_Window;

class Swapchain
{
public:
    void Init(VkDevice aDevice, VkPhysicalDevice aGPU, VkSurfaceKHR aSurface, SDL_Window* aWindow, VkExtent2D aWindowExtent);
    void Cleanup(VkDevice aDevice) const;
    void Resize(VkDevice aDevice, VkPhysicalDevice aGPU, VkSurfaceKHR aSurface, SDL_Window* aWindow, VkExtent2D& aWindowExtent);

    VkSwapchainKHR Get() const                               { return _swapchain; }
    VkFormat GetFormat() const                               { return _imageFormat; }
    VkExtent2D GetExtent() const                             { return _extent; }
    uint32_t GetImageCount() const                           { return _imageCount; }
    const std::vector<VkImage>& GetImages() const            { return _images; }
    const std::vector<VkImageView>& GetImageViews() const    { return _imageViews; }

private:
    VkSwapchainKHR            _swapchain{};
    VkFormat                  _imageFormat{};
    std::vector<VkImage>      _images;
    std::vector<VkImageView>  _imageViews;
    VkExtent2D                _extent{};
    uint32_t                  _imageCount{0};

    void Create(VkDevice aDevice, VkPhysicalDevice aGPU, VkSurfaceKHR aSurface, SDL_Window* aWindow, uint32_t aWidth, uint32_t aHeight);
    void Destroy(VkDevice aDevice) const;
};
