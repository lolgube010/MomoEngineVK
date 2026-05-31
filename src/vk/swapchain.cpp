#include <vk/swapchain.h>
#include <vk/debug.h>
#include <SDL3/SDL.h>
#include <VkBootstrap.h>
#include <Volk/volk.h>

#include <vk/gpu_types.h>

void Swapchain::Init(const VkDevice aDevice, const VkPhysicalDevice aGPU, const VkSurfaceKHR aSurface, SDL_Window* aWindow, const VkExtent2D aWindowExtent)
{
    Create(aDevice, aGPU, aSurface, aWindow, aWindowExtent.width, aWindowExtent.height);
}

void Swapchain::Cleanup(const VkDevice aDevice) const
{
    Destroy(aDevice);
}

void Swapchain::Resize(const VkDevice aDevice, const VkPhysicalDevice aGPU, const VkSurfaceKHR aSurface, SDL_Window* aWindow, VkExtent2D& aWindowExtent)
{
    Destroy(aDevice);
    int w, h;
    SDL_GetWindowSize(aWindow, &w, &h);
    aWindowExtent.width  = static_cast<uint32_t>(w);
    aWindowExtent.height = static_cast<uint32_t>(h);
    Create(aDevice, aGPU, aSurface, aWindow, aWindowExtent.width, aWindowExtent.height);
}

void Swapchain::Create(const VkDevice aDevice, const VkPhysicalDevice aGPU, const VkSurfaceKHR aSurface, SDL_Window* aWindow, const uint32_t aWidth, const uint32_t aHeight)
{
    vkb::SwapchainBuilder swapchainBuilder{aGPU, aDevice, aSurface};

    _imageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{
            .format     = _imageFormat,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        })
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(aWidth, aHeight)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build().value();

    _extent     = vkbSwapchain.extent;
    _swapchain  = vkbSwapchain.swapchain;
    _images     = vkbSwapchain.get_images().value();
    _imageViews = vkbSwapchain.get_image_views().value();

    VK_CHECK(vkGetSwapchainImagesKHR(aDevice, _swapchain, &_imageCount, nullptr));
    MOMO_VK_SET_DEBUG_NAME(aDevice, VK_OBJECT_TYPE_SWAPCHAIN_KHR, _swapchain, "_Swapchain");

    for (size_t i = 0; i < _images.size(); ++i)
    {
        MOMO_VK_SET_DEBUG_NAME(aDevice, VK_OBJECT_TYPE_IMAGE,      _images[i],     "_Image Swapchain {}", i);
        MOMO_VK_SET_DEBUG_NAME(aDevice, VK_OBJECT_TYPE_IMAGE_VIEW, _imageViews[i], "_Image View Swapchain {}", i);
    }
}

void Swapchain::Destroy(const VkDevice aDevice) const
{
    vkDestroySwapchainKHR(aDevice, _swapchain, nullptr);
    for (const auto& view : _imageViews)
    {
        vkDestroyImageView(aDevice, view, nullptr);
    }
}
