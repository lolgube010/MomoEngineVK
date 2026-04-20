#include <vk_images.h>

#include "vk_initializers.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void momo_vkUtil::Transition_Image(const VkCommandBuffer aCmd, const VkImage aImg, const VkImageLayout aCurrentLayout, const VkImageLayout aNewLayout)
{
	VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
	imageBarrier.pNext = nullptr;

	// TODO:
    // In the StageMask, we are doing ALL_COMMANDS.
    // This is inefficient, as it will stall the GPU pipeline a bit.
    // For our needs, it's going to be fine as we are only going to do a few transitions per frame.
    // If you are doing many transitions per frame as part of a post - process chain, you want to avoid doing this, and instead use StageMasks more accurate to what you are doing
    
    // AllCommands stage mask on the barrier means that the barrier will stop the gpu commands completely when it arrives at the barrier. 
    // By using more fine-grained stage masks, it's possible to overlap the GPU pipeline across the barrier a bit. 
    // AccessMask is similar, it controls how the barrier stops different parts of the GPU. 
    // We are going to use VK_ACCESS_2_MEMORY_WRITE_BIT for our source, and add VK_ACCESS_2_MEMORY_READ_BIT to our destination. 
    // Those are generic options that will be fine.
    
    // If you want to read about what would be the optimal way of using pipeline barriers for different use cases, you can find a great reference in here 
    // Khronos Vulkan Documentation: Syncronization examples [https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples]
    // This layout transition is going to work just fine for the whole tutorial, but if you want, you can add more complicated transition functions that are more accurate/lightweight.
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

	// SPECIFIC FIX: Handle Presentation Layouts
	// When transitioning TO present, the destination access must be 0.
	// (Visibility is handled by the semaphore passed to vkQueuePresentKHR)
	if (aNewLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	{
		imageBarrier.dstAccessMask = 0;
	}

	// When transitioning FROM present (e.g. after AcquireNextImage), source access must be 0.
	// (Availability is handled by the semaphore passed to vkAcquireNextImageKHR)
	if (aCurrentLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	{
		imageBarrier.srcAccessMask = 0;
	}

	imageBarrier.oldLayout = aCurrentLayout;
	imageBarrier.newLayout = aNewLayout;

	const bool isDepth = (aCurrentLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
	                   || aCurrentLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	                   || aCurrentLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
	                   || aNewLayout    == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
	                   || aNewLayout    == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	                   || aNewLayout    == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
	const VkImageAspectFlags aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange = momo_vkInit::image_subresource_range(aspectMask);
	imageBarrier.image = aImg;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.pNext = nullptr;

	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(aCmd, &depInfo);
}

void momo_vkUtil::copy_image_to_image(const VkCommandBuffer aCmd, const VkImage aSource, const VkImage aDestination, const VkExtent2D aSrcSize, const VkExtent2D aDstSize)
{
	VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

	blitRegion.srcOffsets[1].x = aSrcSize.width;
	blitRegion.srcOffsets[1].y = aSrcSize.height;
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstOffsets[1].x = aDstSize.width;
	blitRegion.dstOffsets[1].y = aDstSize.height;
	blitRegion.dstOffsets[1].z = 1;

	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;

	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
	blitInfo.dstImage = aDestination;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.srcImage = aSource;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.filter = VK_FILTER_LINEAR;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;

	vkCmdBlitImage2(aCmd, &blitInfo);
}

// There are multiple options for generating the mipmaps. We also don't have to generate them at load time, and could use formats like KTX or DDS which can have the mipmaps pregenerated. A popular option is to generate them in a compute shader that generates multiple levels at once, and that can improve performance. The way we are going to do mipmaps is with a chain of VkCmdImageBlit calls.
void momo_vkUtil::generate_mipmaps(const VkCommandBuffer aCmd, const VkImage aImage, VkExtent2D aImageSize)
{
	const int mipLevels = static_cast<int>(std::floor(std::log2(std::max(aImageSize.width, aImageSize.height)))) + 1;
	for (int mip = 0; mip < mipLevels; mip++) 
	{
		VkExtent2D halfSize = aImageSize;
		halfSize.width /= 2;
		halfSize.height /= 2;

		VkImageMemoryBarrier2 imageBarrier{};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		imageBarrier.pNext = nullptr;

		imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		constexpr VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange = momo_vkInit::image_subresource_range(aspectMask);
		imageBarrier.subresourceRange.levelCount = 1;
		imageBarrier.subresourceRange.baseMipLevel = mip;
		imageBarrier.image = aImage;

		VkDependencyInfo depInfo{  };
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO; 
		depInfo.pNext = nullptr;

		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &imageBarrier;

		vkCmdPipelineBarrier2(aCmd, &depInfo);

		if (mip < mipLevels - 1) 
		{
			VkImageBlit2 blitRegion{};
			blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2; 
			blitRegion.pNext = nullptr;

			blitRegion.srcOffsets[1].x = static_cast<int32_t>(aImageSize.width);
			blitRegion.srcOffsets[1].y = static_cast<int32_t>(aImageSize.height);
			blitRegion.srcOffsets[1].z = 1;

			blitRegion.dstOffsets[1].x = static_cast<int32_t>(halfSize.width);
			blitRegion.dstOffsets[1].y = static_cast<int32_t>(halfSize.height);
			blitRegion.dstOffsets[1].z = 1;

			blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blitRegion.srcSubresource.baseArrayLayer = 0;
			blitRegion.srcSubresource.layerCount = 1;
			blitRegion.srcSubresource.mipLevel = mip;

			blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blitRegion.dstSubresource.baseArrayLayer = 0;
			blitRegion.dstSubresource.layerCount = 1;
			blitRegion.dstSubresource.mipLevel = mip + 1;

			VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
			blitInfo.dstImage = aImage;
			blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			blitInfo.srcImage = aImage;
			blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			blitInfo.filter = VK_FILTER_LINEAR;
			blitInfo.regionCount = 1;
			blitInfo.pRegions = &blitRegion;

			vkCmdBlitImage2(aCmd, &blitInfo);

			aImageSize = halfSize;
		}
	}

	// transition all mip levels into the final read_only layout
	Transition_Image(aCmd, aImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
