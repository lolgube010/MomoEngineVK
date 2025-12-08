#pragma once 
#include <vk_types.h>

// Will contain abstractions for pipelines.

namespace vkUtil
{
	bool LoadShaderModule(const char* aFilePath, VkDevice aDevice, VkShaderModule* aOutShaderModule, VkResult& aOutVkResult);


};