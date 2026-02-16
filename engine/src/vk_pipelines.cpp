#include <vk_pipelines.h>
#include <fstream>
#include <vk_initializers.h>

bool vkUtil::LoadShaderModule(const char* aFilePath, const VkDevice aDevice, const Vk_Debug_Info* aVkDebugInfo, VkShaderModule* aOutShaderModule, VkResult& aOutVkResult)
{
	// open the file. With cursor at the end
	std::ifstream file(aFilePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		return false;
	}
	
	// find what the size of the file is by looking up the location of the cursor because the cursor is at the end, it gives the size directly in bytes
	const size_t fileSize = file.tellg();

	// spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	// put file cursor at beginning
	file.seekg(0);

	// load the entire file into the buffer
	file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

	// now that the file is loaded into the buffer, we can close it
	file.close();

	// create a new shader module, using the buffer we loaded
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	// codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	// check that the creation goes well.
	VkShaderModule shaderModule;
	if ((aOutVkResult = vkCreateShaderModule(aDevice, &createInfo, nullptr, &shaderModule)) != VK_SUCCESS)
	{
		return false;
	}
	*aOutShaderModule = shaderModule;
    aVkDebugInfo->SetDebugInfo(&aDevice, uint64_t(*aOutShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, aFilePath);
	return true;
}

void PipelineBuilder::Clear()
{
	// clear all the structs we need back to 0 with their correct sType

	_inputAssembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

	_rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};

	_colorBlendAttachment = {};

	_multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};

	_pipelineLayout = {};

	_depthStencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

	_renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

	_shaderStages.clear();

}

VkPipeline PipelineBuilder::Build_Pipeline(const VkDevice aDevice) const
{
	// make viewport state from our stored viewport and scissor.
	// at the moment we won't support multiple viewports or scissors
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;

	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// setup dummy color blending. We aren't using transparent objects yet
	// the blending is just "no blend", but we do write to the color attachment
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;

	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &_colorBlendAttachment;

	// completely clear VertexInputStateCreateInfo, as we have no need for it
	constexpr VkPipelineVertexInputStateCreateInfo vertexInputInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

	// build the actual pipeline
	// we now use all the info structs we have been writing into this one to create the pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	// connect the renderInfo to the pNext extension mechanism
	pipelineInfo.pNext = &_renderInfo;

	pipelineInfo.stageCount = static_cast<uint32_t>(_shaderStages.size());
	pipelineInfo.pStages = _shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &_inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &_rasterizer;
	pipelineInfo.pMultisampleState = &_multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDepthStencilState = &_depthStencil;
	pipelineInfo.layout = _pipelineLayout;

	constexpr VkDynamicState state[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dynamicInfo.pDynamicStates = &state[0];
	dynamicInfo.dynamicStateCount = 2;

	pipelineInfo.pDynamicState = &dynamicInfo;

	// it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
	VkPipeline newPipeline;
	if (vkCreateGraphicsPipelines(aDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS)
	{
		fmt::println("failed to create pipeline");
		return VK_NULL_HANDLE; // failed to create graphics pipeline
	}
	return newPipeline;
}

void PipelineBuilder::Set_Shaders(const VkShaderModule aVertexShader, const VkShaderModule aFragmentShader)
{
	_shaderStages.clear();

	_shaderStages.push_back(vkInit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, aVertexShader));

	_shaderStages.push_back(vkInit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, aFragmentShader));
}

void PipelineBuilder::Set_Input_Topology(const VkPrimitiveTopology aTopology)
{
	_inputAssembly.topology = aTopology;
	// we are not going to use primitive restart on the entire tutorial so leave
	// it on false
	_inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::Set_Polygon_Mode(const VkPolygonMode aMode)
{
	_rasterizer.polygonMode = aMode;
	_rasterizer.lineWidth = 1.f;
}

///<param name="aCullMode">Which triangle to discard (front or back facing).</param>
///<param name="aFrontFace">Which side should be the front, winding order. (clockwise, counter-clockwise)</param>
void PipelineBuilder::Set_Cull_Mode(const VkCullModeFlags aCullMode, const VkFrontFace aFrontFace)
{
	_rasterizer.cullMode = aCullMode;
	_rasterizer.frontFace = aFrontFace;
}

void PipelineBuilder::Set_Multisampling_None()
{
	_multisampling.sampleShadingEnable = VK_FALSE;
	// multisampling defaulted to no multisampling (1 sample per pixel)
	_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	_multisampling.minSampleShading = 1.0f;
	_multisampling.pSampleMask = nullptr;
	// no alpha to coverage either
	_multisampling.alphaToCoverageEnable = VK_FALSE;
	_multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::Disable_Blending()
{
	// default write mask
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	// no blending
	_colorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::Set_Color_Attachment_Format(const VkFormat aFormat)
{
	_colorAttachmentFormat = aFormat;
	// connect the format to the renderInfo  structure
	_renderInfo.colorAttachmentCount = 1;
	// On the color attachment, the pipeline needs it by pointer because it wants an array of color attachments. 
	// This is useful for things like deferred rendering where you draw to multiple images at once, 
	// but we don't need this yet so we can default it to just 1 color format.
	_renderInfo.pColorAttachmentFormats = &_colorAttachmentFormat;
}

void PipelineBuilder::Set_Depth_Format(const VkFormat aFormat)
{
	_renderInfo.depthAttachmentFormat = aFormat;
}

void PipelineBuilder::Disable_DepthTest()
{
	_depthStencil.depthTestEnable = VK_FALSE;
	_depthStencil.depthWriteEnable = VK_FALSE;
	_depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
	_depthStencil.depthBoundsTestEnable = VK_FALSE;
	_depthStencil.stencilTestEnable = VK_FALSE;
	_depthStencil.front = {};
	_depthStencil.back = {};
	_depthStencil.minDepthBounds = 0.f;
	_depthStencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::Enable_DepthTest(const bool aDepthWriteEnable, const VkCompareOp aOp)
{
	_depthStencil.depthTestEnable = VK_TRUE;
	_depthStencil.depthWriteEnable = aDepthWriteEnable;
	_depthStencil.depthCompareOp = aOp;
	_depthStencil.depthBoundsTestEnable = VK_FALSE;
	_depthStencil.stencilTestEnable = VK_FALSE;
	_depthStencil.front = {};
	_depthStencil.back = {};
	_depthStencil.minDepthBounds = 0.f;
	_depthStencil.maxDepthBounds = 1.f;
}


// When setting blending options in vulkan, we need to fill the formula on both color and alpha.The parameters work the same on both color and alpha.The formula works like this
// outColor = srcColor * srcColorBlendFactor <op> dstColor * dstColorBlendFactor;
void PipelineBuilder::Enable_Blending_Additive()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

// When setting blending options in vulkan, we need to fill the formula on both color and alpha.The parameters work the same on both color and alpha.The formula works like this
// outColor = srcColor * srcColorBlendFactor <op> dstColor * dstColorBlendFactor;
void PipelineBuilder::Enable_Blending_AlphaBlend()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

}

void PipelineBuilder::Enable_Blending_Multiply()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::Enable_Blending_Screen()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::Enable_Blending_PremultipliedAlpha()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::Enable_Blending_Subtractive()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::Enable_Blending_Invert()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::Enable_Blending_Min()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_MIN;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_MIN;
}

void PipelineBuilder::Enable_Blending_Max()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_MAX;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_MAX;
}

std::string momo_util::GetShaderExtension(const ShaderType aType)
{
	switch (aType)
	{
	case ShaderType::Vertex: return ".vert";
	case ShaderType::Fragment: return ".frag";
	case ShaderType::Compute: return ".comp";
	default: return ".unknown";
	}
}

std::string momo_util::BuildShaderPath(const std::string& aFileName, const ShaderType aType, const bool aIsHlsl)
{
	// Base directory (adjust this to match your project structure)
#ifdef _DEBUG
	const std::string basePath = "../../shaders/bin/debug/";
#else
	const std::string basePath = "../../shaders/bin/release/";
#endif
	// Get standard extension (e.g., ".comp")
	const std::string stageExt = GetShaderExtension(aType);

	// Build the final string
	// Format: ../../shaders/name.stage[.hlsl].spv
	std::string fullPath = basePath + aFileName + stageExt;

	if (aIsHlsl)
	{
		fullPath += ".hlsl";
	}

	fullPath += ".spv";

	return fullPath;
}

std::optional<VkShaderModule> momo_util::LoadShader(const std::string& aName, const momo_util::ShaderType aType, bool aIsHLSL, const VkDevice aDevice, const Vk_Debug_Info* aVkDebugInfo)
{
    const std::string path = BuildShaderPath(aName, aType, aIsHLSL);

    VkShaderModule module;

    if (VkResult errorCode = {}; 
		!vkUtil::LoadShaderModule(path.c_str(), aDevice, aVkDebugInfo, &module, errorCode))
    {
        fmt::print("Error loading shader. Type: {} Name: {} ErrorCode: {}\n", GetShaderExtension(aType), aName, static_cast<int>(errorCode));
        return std::nullopt;
    }

    fmt::print("Shader loaded. Type: {} Name: {}\n", GetShaderExtension(aType), aName);
    return module;
}
