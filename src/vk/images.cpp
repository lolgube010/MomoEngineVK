#include <vk/images.h>
#include <vk/initializers.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples 
VkImageAspectFlags momo_vkUtil::aspect_flags_from_format(const VkFormat aFormat)
{
    switch (aFormat)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_S8_UINT:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

// given this layout, what stage & access flag goes with it?
std::pair<VkPipelineStageFlags2, VkAccessFlags2> momo_vkUtil::get_mask_info(const VkImageLayout aLayout)
{
    switch (aLayout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT} ;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT} ;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT };
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        return {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, 
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
    // ambiguous case, we don't know if it's a vertex/pixel/compute shader etc. 
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: 
        return {VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT};
    // Semaphores handle availability/visibility for present transitions, not the barrier itself.
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: 
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return {VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE};
    default:
        return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT};
    }
}

// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples 
// Layouts describe how the GPU physically stores image data in memory (tiling, compression, etc.).
// Different operations need different arrangements — rasterization wants tiled data for cache locality,
// compute and present engines need different layouts. The barrier tells the driver:
//   (1) wait for prior work (src masks), (2) re-arrange the memory, (3) unblock the next stage (dst masks).
//
// The barrier covers two independent concerns:
//   Sync   — src/dstStageMask + src/dstAccessMask: what was last using this image, what will use it next.
//             ALL_COMMANDS is maximally broad — it stalls the full pipeline but is always correct.
//             Tighter masks (e.g. COMPUTE_SHADER src → COLOR_ATTACHMENT_OUTPUT dst) let unrelated work overlap.
//   Aspect — which image planes the barrier covers: COLOR, DEPTH, STENCIL, or DEPTH|STENCIL.
//             Comes from the VkFormat (via aspect_flags_from_format), NOT from the layouts.
void momo_vkUtil::transition_image(const VkCommandBuffer aCmd, const VkImage aImg, const VkImageLayout aCurrentLayout, const VkImageLayout aNewLayout, const VkFormat aFormat)
{
	VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr};
    
    auto [currentPipeLineStage, currentAccessFlags] = get_mask_info(aCurrentLayout);
    imageBarrier.srcStageMask = currentPipeLineStage;
    imageBarrier.srcAccessMask = currentAccessFlags;
    
    auto [newPipeLineStage, newAccessFlags] = get_mask_info(aNewLayout);
    imageBarrier.dstStageMask = newPipeLineStage;
    imageBarrier.dstAccessMask = newAccessFlags;

	imageBarrier.oldLayout = aCurrentLayout;
	imageBarrier.newLayout = aNewLayout;
	imageBarrier.subresourceRange = momo_vkInit::image_subresource_range(aspect_flags_from_format(aFormat));
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

	blitRegion.srcOffsets[1].x = static_cast<int32_t>(aSrcSize.width);
    blitRegion.srcOffsets[1].y = static_cast<int32_t>(aSrcSize.height);
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstOffsets[1].x = static_cast<int32_t>(aDstSize.width);
	blitRegion.dstOffsets[1].y = static_cast<int32_t>(aDstSize.height);
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

// TODO: There are multiple options for generating the mipmaps. We also don't have to generate them at load time, and could use formats like KTX or DDS which can have the mipmaps pregenerated. A popular option is to generate them in a compute shader that generates multiple levels at once, and that can improve performance. The way we are going to do mipmaps is with a chain of VkCmdImageBlit calls.
// note: as it works right now we need to transition all mip levels to transfer_dst_optimal beforehand.
void momo_vkUtil::generate_mipmaps(const VkCommandBuffer aCmd, const VkImage aImage, VkExtent2D aImageSize, const VkFormat aFormat)
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

		imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; // used to be all commands
		imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
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
	transition_image(aCmd, aImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, aFormat);
}
