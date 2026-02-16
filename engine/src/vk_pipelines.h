#pragma once 
#include <vk_types.h>
#include "vk_debug.h"

// Will contain abstractions for pipelines.

class VulkanEngine;

namespace vkUtil
{
	// TODO- simplify usage of, right now is way too spammy with the checking things we do and I don't really like that. todo fix. 
	bool LoadShaderModule(const char* aFilePath, VkDevice aDevice, const Vk_Debug_Info* aVkDebugInfo, VkShaderModule* aOutShaderModule, VkResult& aOutVkResult);
};

class PipelineBuilder
{
public:
	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkPipelineRasterizationStateCreateInfo _rasterizer;

	// We only support rendering to one attachment here, so this is fine. 
	// This can be made into an array of VkPipelineColorBlendAttachmentState if drawing to multiple attachments is needed.
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;
	VkPipelineRenderingCreateInfo _renderInfo;
	VkFormat _colorAttachmentFormat;

	PipelineBuilder() { Clear(); }

	void Clear();

	VkPipeline Build_Pipeline(VkDevice aDevice) const;
	void Set_Shaders(VkShaderModule aVertexShader, VkShaderModule aFragmentShader);
	void Set_Input_Topology(VkPrimitiveTopology aTopology);
	void Set_Polygon_Mode(VkPolygonMode aMode);
	void Set_Cull_Mode(VkCullModeFlags aCullMode, VkFrontFace aFrontFace);
	void Set_Multisampling_None();
	void Disable_Blending();
	void Set_Color_Attachment_Format(VkFormat aFormat);
	void Set_Depth_Format(VkFormat aFormat);
	void Disable_DepthTest();
	void Enable_DepthTest(bool aDepthWriteEnable, VkCompareOp aOp);
	void Enable_Blending_Additive();
	void Enable_Blending_AlphaBlend();

	// slop
	void Enable_Blending_Multiply();
	void Enable_Blending_Screen();
	void Enable_Blending_PremultipliedAlpha();
	void Enable_Blending_Subtractive();
	void Enable_Blending_Invert();
	void Enable_Blending_Min();
	void Enable_Blending_Max();
};

namespace momo_util
{
	enum class ShaderType
	{
		Vertex,
		Fragment,
		Compute,
	};

	std::string GetShaderExtension(ShaderType aType);
	std::string BuildShaderPath(const std::string& aFileName, ShaderType aType, bool aIsHlsl);
    std::optional<VkShaderModule> LoadShader(const std::string& aName, momo_util::ShaderType aType, bool aIsHLSL, VkDevice aDevice, const Vk_Debug_Info* aVkDebugInfo);
}
