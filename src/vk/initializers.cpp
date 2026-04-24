#include <vk/initializers.h>
#include <vk/images.h>

//> init_cmd
VkCommandPoolCreateInfo momo_vkInit::command_pool_create_info(const uint32_t aQueueFamilyIndex, const VkCommandPoolCreateFlags aFlags /*= 0*/)
{
	VkCommandPoolCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.queueFamilyIndex = aQueueFamilyIndex;
	info.flags = aFlags;
	return info;
}


VkCommandBufferAllocateInfo momo_vkInit::command_buffer_allocate_info(const VkCommandPool aPool, const uint32_t aCount /*= 1*/)
{
	VkCommandBufferAllocateInfo info;
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.pNext = nullptr;

	info.commandPool = aPool;
	info.commandBufferCount = aCount;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	return info;
}

//< init_cmd
// 
//> init_cmd_draw
VkCommandBufferBeginInfo momo_vkInit::command_buffer_begin_info(const VkCommandBufferUsageFlags aFlags /*= 0*/)
{
	VkCommandBufferBeginInfo info;
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.pNext = nullptr;

	info.pInheritanceInfo = nullptr;
	info.flags = aFlags;
	return info;
}

//< init_cmd_draw

//> init_sync
VkFenceCreateInfo momo_vkInit::fence_create_info(const VkFenceCreateFlags aFlags /*= 0*/)
{
	VkFenceCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.pNext = nullptr;

	info.flags = aFlags;

	return info;
}

VkSemaphoreCreateInfo momo_vkInit::semaphore_create_info(const VkSemaphoreCreateFlags aFlags /*= 0*/)
{
	VkSemaphoreCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = aFlags;
	return info;
}

//< init_sync

VkCommandBufferSubmitInfo momo_vkInit::command_buffer_submit_info(const VkCommandBuffer aCmd)
{
	VkCommandBufferSubmitInfo info;
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = nullptr;
	info.commandBuffer = aCmd;
	info.deviceMask = 0;

	return info;
}

VkSubmitInfo2 momo_vkInit::submit_info(const VkCommandBufferSubmitInfo* aCmd, const VkSemaphoreSubmitInfo* aSignalSemaphoreInfo, const VkSemaphoreSubmitInfo* aWaitSemaphoreInfo)
{
	VkSubmitInfo2 info = {};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	info.pNext = nullptr;

	info.waitSemaphoreInfoCount = aWaitSemaphoreInfo == nullptr ? 0 : 1;
	info.pWaitSemaphoreInfos = aWaitSemaphoreInfo;

	info.signalSemaphoreInfoCount = aSignalSemaphoreInfo == nullptr ? 0 : 1;
	info.pSignalSemaphoreInfos = aSignalSemaphoreInfo;

	info.commandBufferInfoCount = 1;
	info.pCommandBufferInfos = aCmd;

	return info;
}

VkSemaphoreSubmitInfo momo_vkInit::semaphore_submit_info(const VkPipelineStageFlags2 aStageMask, const VkSemaphore aSemaphore)
{
	VkSemaphoreSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.semaphore = aSemaphore;
	submitInfo.stageMask = aStageMask;
	submitInfo.deviceIndex = 0;
	submitInfo.value = 1;

	return submitInfo;
}


VkPresentInfoKHR momo_vkInit::present_info(const VkSwapchainKHR* aSwapchain, const VkSemaphore* aWaitSemaphore, const uint32_t* aSwapchainImageIndex)
{
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.pNext = nullptr;
    info.pSwapchains = aSwapchain;
	info.swapchainCount = 1; // idk when you'd want more than one
	info.pWaitSemaphores = aWaitSemaphore;
	info.waitSemaphoreCount = 1;
	info.pImageIndices = aSwapchainImageIndex;

	return info;
}

VkRenderingAttachmentInfo momo_vkInit::attachment_info(
	const VkImageView aView, const VkClearValue* aClear, const VkImageLayout aLayout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/)
{
	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.pNext = nullptr;

	colorAttachment.imageView = aView;
	colorAttachment.imageLayout = aLayout;
	colorAttachment.loadOp = aClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	if (aClear)
	{
		colorAttachment.clearValue = *aClear;
	}

	return colorAttachment;
}

VkRenderingAttachmentInfo momo_vkInit::depth_attachment_info(const VkImageView aView, const VkImageLayout aLayout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/)
{
	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.pNext = nullptr;

	depthAttachment.imageView = aView;
	depthAttachment.imageLayout = aLayout;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.clearValue.depthStencil.depth = 0.f;

	return depthAttachment;
}

VkRenderingInfo momo_vkInit::rendering_info(const VkExtent2D aRenderExtent, const VkRenderingAttachmentInfo* aColorAttachment, const VkRenderingAttachmentInfo* aDepthAttachment)
{
	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.pNext = nullptr;

	renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, aRenderExtent};
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = aColorAttachment;
	renderInfo.pDepthAttachment = aDepthAttachment;
	renderInfo.pStencilAttachment = nullptr;

	return renderInfo;
}
VkImageSubresourceRange momo_vkInit::image_subresource_range(const VkImageAspectFlags anAspectMask)
{
	VkImageSubresourceRange subImage;
	subImage.aspectMask = anAspectMask;
	subImage.baseMipLevel = 0;
	subImage.levelCount = VK_REMAINING_MIP_LEVELS;
	subImage.baseArrayLayer = 0;
	subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return subImage;
}


VkDescriptorSetLayoutBinding momo_vkInit::descriptor_set_layout_binding(const VkDescriptorType aType, const VkShaderStageFlags aStageFlags, const uint32_t aBinding)
{
	VkDescriptorSetLayoutBinding setBind;
	setBind.binding = aBinding;
	setBind.descriptorCount = 1;
	setBind.descriptorType = aType;
	setBind.pImmutableSamplers = nullptr;
	setBind.stageFlags = aStageFlags;

	return setBind;
}

VkDescriptorSetLayoutCreateInfo momo_vkInit::descriptor_set_layout_create_info(const VkDescriptorSetLayoutBinding* aBindings, const uint32_t aBindingCount)
{
	VkDescriptorSetLayoutCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;

	info.pBindings = aBindings;
	info.bindingCount = aBindingCount;
	info.flags = 0;

	return info;
}

VkWriteDescriptorSet momo_vkInit::write_descriptor_image(const VkDescriptorType aType, const VkDescriptorSet aDstSet, const VkDescriptorImageInfo* aImageInfo, const uint32_t aBinding)
{
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstBinding = aBinding;
	write.dstSet = aDstSet;
	write.descriptorCount = 1;
	write.descriptorType = aType;
	write.pImageInfo = aImageInfo;

	return write;
}

VkWriteDescriptorSet momo_vkInit::write_descriptor_buffer(const VkDescriptorType aType, const VkDescriptorSet aDstSet, const VkDescriptorBufferInfo* aBufferInfo, const uint32_t aBinding)
{
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstBinding = aBinding;
	write.dstSet = aDstSet;
	write.descriptorCount = 1;
	write.descriptorType = aType;
	write.pBufferInfo = aBufferInfo;

	return write;
}

VkDescriptorBufferInfo momo_vkInit::buffer_info(const VkBuffer aBuffer, const VkDeviceSize aOffset, const VkDeviceSize aRange)
{
	VkDescriptorBufferInfo info;
	info.buffer = aBuffer;
	info.offset = aOffset;
	info.range = aRange;
	return info;
}

//> image_set
VkImageCreateInfo momo_vkInit::image_create_info(const VkFormat aFormat, const VkImageUsageFlags aUsageFlags, const VkExtent3D aExtent)
{
	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;

	info.imageType = VK_IMAGE_TYPE_2D;

	info.format = aFormat;
	info.extent = aExtent;

	info.mipLevels = 1;
	info.arrayLayers = 1;

	//for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
	info.samples = VK_SAMPLE_COUNT_1_BIT;

	//optimal tiling, which means the image is stored on the best gpu format
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = aUsageFlags;

	return info;
}

VkImageViewCreateInfo momo_vkInit::imageview_create_info(const VkFormat aFormat, const VkImage aImage, const VkImageAspectFlags aSpectFlags)
{
	// build a image-view for the depth image to use for rendering
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;

	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.image = aImage;
	info.format = aFormat;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.aspectMask = aSpectFlags;

	return info;
}

//< image_set
VkPipelineLayoutCreateInfo momo_vkInit::pipeline_layout_create_info()
{
	VkPipelineLayoutCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;

	// empty defaults
	info.flags = 0;
	info.setLayoutCount = 0;
	info.pSetLayouts = nullptr;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges = nullptr;
	return info;
}

VkPipelineShaderStageCreateInfo momo_vkInit::pipeline_shader_stage_create_info(const VkShaderStageFlagBits aStage, const VkShaderModule aShaderModule, const char* aEntry)
{
	VkPipelineShaderStageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.pNext = nullptr;

	// shader stage
	info.stage = aStage;
	// module containing the code for this shader stage
	info.module = aShaderModule;
	// the entry point of the shader
	info.pName = aEntry; // this option gives you the ability to have multiple shaders in the same file, having different entry points.
	return info;
}

VkDebugUtilsLabelEXT momo_vkInit::debug_label(const char* aPassName, const glm::vec4 aColor)
{
    VkDebugUtilsLabelEXT labelInfo = {};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pNext = nullptr;
    labelInfo.pLabelName = aPassName;

	labelInfo.color[0] = aColor.r;
	labelInfo.color[1] = aColor.g;
	labelInfo.color[2] = aColor.b;
	labelInfo.color[3] = aColor.a;

    return labelInfo;
}

VkComputePipelineCreateInfo momo_vkInit::compute_pipeline_create_info(const VkPipelineLayout aLayout, const VkPipelineShaderStageCreateInfo& aStageInfo)
{ 
    VkComputePipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.pNext = nullptr;
    info.layout = aLayout;
    info.stage = aStageInfo;
    return info;
}

VkSamplerCreateInfo momo_vkInit::sampler_create_info(const VkFilter aFilter)
{
    VkSamplerCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.magFilter = aFilter; // magnification filter, how a texel gets stretched
    info.minFilter = aFilter; // minification filter, how a texel gets collapsed
    info.pNext = nullptr;
    return info;
}

VkSamplerCreateInfo momo_vkInit::sampler_create_info(const VkFilter aMagFilter, const VkFilter aMinFilter, const float aMaxLod, const float aMinLod, const VkSamplerMipmapMode aMipMapMode)
{
	VkSamplerCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.magFilter = aMagFilter; // magnification filter, how a texel gets stretched
    info.minFilter = aMinFilter; // minification filter, how a texel gets collapsed
    info.pNext = nullptr;
    info.maxLod = aMaxLod;
    info.minLod = aMinLod;
    info.mipmapMode = aMipMapMode;
	// info.mipLodBias
	// info.anisotropyEnable
    // info.borderColor
    // info.compareEnable
    // info.compareOp
	// info.maxAnisotropy
	// info.unnormalizedCoordinates
    return info;
}

VkBufferImageCopy momo_vkInit::buffer_image_copy(const VkExtent3D anImageExtent, const VkFormat anImageFormat)
{
	VkBufferImageCopy info;
    info.bufferImageHeight = 0;
    info.bufferOffset = 0;
    info.bufferRowLength = 0;
    info.imageOffset = VkOffset3D();
    info.imageExtent = anImageExtent;
    
    info.imageSubresource.aspectMask = momo_vkUtil::aspect_flags_from_format(anImageFormat);
    info.imageSubresource.baseArrayLayer = 0;
    info.imageSubresource.layerCount = 1;
    info.imageSubresource.mipLevel = 0;
    return info;
}
