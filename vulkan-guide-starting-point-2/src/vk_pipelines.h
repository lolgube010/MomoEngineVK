#pragma once 
#include <vk_types.h>

// Will contain abstractions for pipelines.

namespace vkUtil
{
	bool LoadShaderModule(const char* aFilePath, VkDevice aDevice, VkShaderModule* aOutShaderModule, VkResult& aOutVkResult);
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
