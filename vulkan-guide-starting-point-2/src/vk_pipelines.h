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
	inline std::string GetShaderExtension(ShaderType type)
	{
		switch (type)
		{
		case ShaderType::Vertex: return ".vert";
		case ShaderType::Fragment: return ".frag";
		case ShaderType::Compute: return ".comp";
		default: return ".unknown";
		}
	}

	// 2. & 3. The Support Function
	inline std::string BuildShaderPath(const std::string& fileName, ShaderType type, bool isHlsl)
	{
		// Base directory (adjust this to match your project structure)
		const std::string basePath = "../../shaders/";

		// Get standard extension (e.g., ".comp")
		std::string stageExt = GetShaderExtension(type);

		// Build the final string
		// Format: ../../shaders/name.stage[.hlsl].spv
		std::string fullPath = basePath + fileName + stageExt;

		if (isHlsl)
		{
			fullPath += ".hlsl";
		}

		// Assuming you load compiled SPIR-V files
		fullPath += ".spv";

		return fullPath;
	}
}
