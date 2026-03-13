#pragma once
#include <Volk/volk.h>

// This will contain image related vulkan helpers
namespace vkUtil
{
    // For Shader read: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL. For Compute R/W: VK_IMAGE_LAYOUT_GENERAL
	void Transition_Image(VkCommandBuffer aCmd, VkImage aImg, VkImageLayout aCurrentLayout, VkImageLayout aNewLayout);

	void copy_image_to_image(VkCommandBuffer aCmd, VkImage aSource, VkImage aDestination, VkExtent2D aSrcSize, VkExtent2D aDstSize);

	void generate_mipmaps(VkCommandBuffer aCmd, VkImage aImage, VkExtent2D aImageSize);
};
