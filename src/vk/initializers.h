#pragma once
#include <vk/gpu_types.h>

// This will contain helpers to create vulkan structures

namespace momo_vkInit
{
	//> init_cmd
	VkCommandPoolCreateInfo command_pool_create_info(uint32_t aQueueFamilyIndex, VkCommandPoolCreateFlags aFlags = 0);
	VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool aPool, uint32_t aCount = 1);
	//< init_cmd

	VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags aFlags = 0);
	VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer aCmd);

	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags aFlags = 0);

	VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags aFlags = 0);

	VkSubmitInfo2 submit_info(const VkCommandBufferSubmitInfo* aCmd, const VkSemaphoreSubmitInfo* aSignalSemaphoreInfo, const VkSemaphoreSubmitInfo* aWaitSemaphoreInfo);
    VkPresentInfoKHR present_info(const VkSwapchainKHR* aSwapchain, const VkSemaphore* aWaitSemaphore, const uint32_t* aSwapchainImageIndex);

	VkRenderingAttachmentInfo attachment_info(VkImageView aView, const VkClearValue* aClear, VkImageLayout aLayout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

	VkRenderingAttachmentInfo depth_attachment_info(VkImageView aView, VkImageLayout aLayout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

	VkRenderingInfo rendering_info(VkExtent2D aRenderExtent, const VkRenderingAttachmentInfo* aColorAttachment, const VkRenderingAttachmentInfo* aDepthAttachment);

	VkImageSubresourceRange image_subresource_range(VkImageAspectFlags anAspectMask);

	VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 aStageMask, VkSemaphore aSemaphore);
	VkDescriptorSetLayoutBinding descriptor_set_layout_binding(VkDescriptorType aType, VkShaderStageFlags aStageFlags, uint32_t aBinding);
	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info(const VkDescriptorSetLayoutBinding* aBindings, uint32_t aBindingCount);
	VkWriteDescriptorSet write_descriptor_image(VkDescriptorType aType, VkDescriptorSet aDstSet, const VkDescriptorImageInfo* aImageInfo, uint32_t aBinding);
	VkWriteDescriptorSet write_descriptor_buffer(VkDescriptorType aType, VkDescriptorSet aDstSet, const VkDescriptorBufferInfo* aBufferInfo, uint32_t aBinding);
	VkDescriptorBufferInfo buffer_info(VkBuffer aBuffer, VkDeviceSize aOffset, VkDeviceSize aRange);

	VkImageCreateInfo image_create_info(VkFormat aFormat, VkImageUsageFlags aUsageFlags, VkExtent3D aExtent);
	VkImageViewCreateInfo imageview_create_info(VkFormat aFormat, VkImage aImage, VkImageAspectFlags aSpectFlags);
	VkPipelineLayoutCreateInfo pipeline_layout_create_info();
	VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits aStage, VkShaderModule aShaderModule, const char* aEntry = "main");

	VkDebugUtilsLabelEXT debug_label(const char* aPassName, glm::vec4 aColor = glm::vec4(1.f,1.f,1.f,1.f));

	VkComputePipelineCreateInfo compute_pipeline_create_info(VkPipelineLayout aLayout, const VkPipelineShaderStageCreateInfo& aStageInfo);

	VkSamplerCreateInfo sampler_create_info(VkFilter aFilter);
	VkSamplerCreateInfo sampler_create_info(VkFilter aMagFilter, VkFilter aMinFilter, float aMaxLod, float aMinLod, VkSamplerMipmapMode aMipMapMode);

	VkBufferImageCopy buffer_image_copy(VkExtent3D anImageExtent, VkFormat anImageFormat);
} // namespace vkInit
