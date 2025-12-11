#pragma once 
#include <vk_types.h>

// Will contain abstractions for pipelines.

namespace vkUtil
{
	bool LoadShaderModule(const char* aFilePath, VkDevice aDevice, VkShaderModule* aOutShaderModule, VkResult& aOutVkResult);
};

namespace momo_util
{
	enum class ShaderType
	{
		Vertex,
		Fragment,
		Compute,
	};

	// Helper to get the extension string from the enum
	std::string GetShaderExtension(ShaderType aType);

	// 2. & 3. The Support Function
	std::string BuildShaderPath(const std::string& aFileName, ShaderType aType, bool aIsHlsl);
}
