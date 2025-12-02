#include <vk_images.h>

#include "vk_initializers.h"

void vkUtil::Transition_Image(VkCommandBuffer aCmd, VkImage aImg, VkImageLayout aCurrentLayout, VkImageLayout aNewLayout)
{
	VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
	imageBarrier.pNext = nullptr;

	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

	// fix for non best practices
	//if (aCurrentLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR ||
	//aNewLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	//{
	//imageBarrier.srcAccessMask = 0;
	//imageBarrier.dstAccessMask = 0;
	//}

	imageBarrier.oldLayout = aCurrentLayout;
	imageBarrier.newLayout = aNewLayout;

	VkImageAspectFlags aspectMask = (aNewLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange = vkInit::image_subresource_range(aspectMask);
	imageBarrier.image = aImg;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.pNext = nullptr;

	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(aCmd, &depInfo);
}
