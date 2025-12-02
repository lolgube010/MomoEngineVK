#pragma once
#include <vulkan/vulkan.h>

// This will contain image related vulkan helpers
namespace vkUtil
{
	void Transition_Image(VkCommandBuffer aCmd, VkImage aImg, VkImageLayout aCurrentLayout, VkImageLayout aNewLayout);
};
