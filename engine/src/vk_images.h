#pragma once
#include <Volk/volk.h>

namespace momo_vkUtil
{
	VkImageAspectFlags aspect_flags_from_format(VkFormat aFormat);

	std::pair<VkPipelineStageFlags2, VkAccessFlags2> get_mask_info(VkImageLayout aLayout);

	void transition_image(VkCommandBuffer aCmd, VkImage aImg, VkImageLayout aCurrentLayout, VkImageLayout aNewLayout, VkFormat aFormat);

	void copy_image_to_image(VkCommandBuffer aCmd, VkImage aSource, VkImage aDestination, VkExtent2D aSrcSize, VkExtent2D aDstSize);

	void generate_mipmaps(VkCommandBuffer aCmd, VkImage aImage, VkExtent2D aImageSize, VkFormat aFormat);
};
