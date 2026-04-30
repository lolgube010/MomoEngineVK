#pragma once
#include <vk/gpu_types.h>

// Will contain abstractions for pipelines.

class VulkanEngine;

class PipelineBuilder
{
public:
	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkPipelineRasterizationStateCreateInfo _rasterizer;

	// TODO - used for deferred 
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

	VkPipeline Build_Pipeline(VkDevice aDevice, const char* aName) const;
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
	
    // When setting blending options in vulkan, we need to fill the formula on both color and alpha. 
    // The parameters work the same on both color and alpha. The formula works like this
    // outColor = srcColor * srcColorBlendFactor <op> dstColor * dstColorBlendFactor;
    void Enable_Blending_Additive();
	void Enable_Blending_AlphaBlend();
	void Enable_Blending_Multiply();
	void Enable_Blending_Screen();
	void Enable_Blending_PremultipliedAlpha();
	void Enable_Blending_Subtractive();
	void Enable_Blending_Invert();
	void Enable_Blending_Min();
	void Enable_Blending_Max();

	void Set_Multisampling_MSAA(VkSampleCountFlagBits aSamples); // standard msaa
    // link alpha of texture to msaa sample grid. used for leafs or chainlink fences. any transparent texture on a flat square
    void Set_Multisampling_AlphaToCoverage(VkSampleCountFlagBits aSamples); 
};

namespace momo_shaderUtil
{

	enum class ShaderType
	{
		Vertex,
		Fragment,
		Compute,
	};

	enum class ShaderLang
    {
		GLSL,
		HLSL,
		Slang
    };

    bool load_shader_module(const char* aFilePath, VkDevice aDevice, VkShaderModule* aOutShaderModule, VkResult& aOutVkResult);
    std::string build_spv_shader_path(const std::string& aFileName, ShaderType aType, ShaderLang aShaderLang);
    std::optional<VkShaderModule> load_shader(const std::string& aName, momo_shaderUtil::ShaderType aType, ShaderLang aShaderLang, VkDevice aDevice);
}
