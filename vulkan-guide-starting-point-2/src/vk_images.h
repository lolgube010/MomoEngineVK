#pragma once
#include <vulkan/vulkan.h>



// This will contain image related vulkan helpers
namespace vkUtil
{
	void Transition_Image(VkCommandBuffer aCmd, VkImage aImg, VkImageLayout aCurrentLayout, VkImageLayout aNewLayout);

	void copy_image_to_image(VkCommandBuffer aCmd, VkImage aSource, VkImage aDestination, VkExtent2D aSrcSize, VkExtent2D aDstSize);
};
