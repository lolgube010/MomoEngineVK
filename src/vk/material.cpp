#include <vk/material.h>

#include <Volk/volk.h>
#include <vk/initializers.h>
#include <vk/pipelines.h>
#include <vk/debug.h>

void GLTFMetallic_Roughness::Build_Pipelines(const VkDevice aDevice,
                                              const VkDescriptorSetLayout aSceneDataLayout,
                                              const VkFormat aDrawFormat,
                                              const VkFormat aDepthFormat)
{
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.Add_Binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    _materialLayout = layoutBuilder.Build(aDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, "GLTFMetallic_Roughness Material");

    VkDescriptorSetLayout layouts[] = { aSceneDataLayout, _materialLayout };

    VkPushConstantRange matrixRange{};
    matrixRange.offset     = 0;
    matrixRange.size       = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo mesh_layout_info = Momo_VkInit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount         = 2;
    mesh_layout_info.pSetLayouts            = layouts;
    mesh_layout_info.pPushConstantRanges    = &matrixRange;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(aDevice, &mesh_layout_info, nullptr, &newLayout));
    MOMO_VK_SET_DEBUG_NAME(aDevice, VK_OBJECT_TYPE_PIPELINE_LAYOUT, newLayout, "_Pipeline Layout GLTFMetallic_Roughness Material Opaque and Transparent");

    _opaquePipeline._layout               = newLayout;
    _transparentPipeline._layout          = newLayout;
    _opaqueWireframePipeline._layout      = newLayout;
    _transparentWireframePipeline._layout = newLayout;
    _opaqueWireframePipeline2._layout     = newLayout;
    _opaqueWireframePipeline3._layout     = newLayout;

    constexpr auto shaderLang = Momo_ShaderUtil::ShaderLang::GLSL;
    auto meshFragShader   = Momo_ShaderUtil::load_shader("mesh_pbr", Momo_ShaderUtil::ShaderType::Fragment, shaderLang, aDevice);
    auto meshVertexShader = Momo_ShaderUtil::load_shader("mesh",     Momo_ShaderUtil::ShaderType::Vertex,   shaderLang, aDevice);

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.Set_Shaders(meshVertexShader.value(), meshFragShader.value());
    // All four pipelines below share newLayout. 
    // Clear_Resources only destroys the transparent variant's layout to avoid double-free; the other three reference the same handle.
    pipelineBuilder.Set_Input_Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.Set_Polygon_Mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.Set_Cull_Mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.Set_Multisampling_None();
    pipelineBuilder.Disable_Blending();
    pipelineBuilder.Enable_DepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.Set_Color_Attachment_Format(aDrawFormat);
    pipelineBuilder.Set_Depth_Format(aDepthFormat);
    pipelineBuilder._pipelineLayout = newLayout;

    _opaquePipeline._pipeline = pipelineBuilder.Build_Pipeline(aDevice, "GLTFMetallic_Roughness Opaque");

    pipelineBuilder.Enable_Blending_Additive();
    pipelineBuilder.Enable_DepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    _transparentPipeline._pipeline = pipelineBuilder.Build_Pipeline(aDevice, "GLTFMetallic_Roughness Transparent");

    pipelineBuilder.Set_Polygon_Mode(VK_POLYGON_MODE_LINE);
    pipelineBuilder.Disable_Blending();
    pipelineBuilder.Enable_DepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    _opaqueWireframePipeline._pipeline = pipelineBuilder.Build_Pipeline(aDevice, "GLTFMetallic_Roughness Opaque Wireframe");

    pipelineBuilder.Enable_Blending_Additive();
    pipelineBuilder.Enable_DepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    _transparentWireframePipeline._pipeline = pipelineBuilder.Build_Pipeline(aDevice, "GLTFMetallic_Roughness Transparent Wireframe");

    auto whiteShader = Momo_ShaderUtil::load_shader("white", Momo_ShaderUtil::ShaderType::Fragment, shaderLang, aDevice);
    auto orangeShader = Momo_ShaderUtil::load_shader("orange", Momo_ShaderUtil::ShaderType::Fragment, shaderLang, aDevice);
    pipelineBuilder.Disable_Blending();
    pipelineBuilder.Enable_DepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    pipelineBuilder.Set_Shaders(meshVertexShader.value(), whiteShader.value());
    _opaqueWireframePipeline2._pipeline = pipelineBuilder.Build_Pipeline(aDevice, "GLTFMetallic_Roughness Opaque Wireframe2");

    pipelineBuilder.Set_Shaders(meshVertexShader.value(), orangeShader.value());
    _opaqueWireframePipeline3._pipeline = pipelineBuilder.Build_Pipeline(aDevice, "GLTFMetallic_Roughness Opaque Wireframe3");

    
    vkDestroyShaderModule(aDevice, whiteShader.value(), nullptr);
    vkDestroyShaderModule(aDevice, orangeShader.value(), nullptr);
    
    vkDestroyShaderModule(aDevice, meshFragShader.value(),   nullptr);
    vkDestroyShaderModule(aDevice, meshVertexShader.value(), nullptr);
}

void GLTFMetallic_Roughness::Clear_Resources(const VkDevice aDevice) const
{
    vkDestroyDescriptorSetLayout(aDevice, _materialLayout, nullptr);
    vkDestroyPipelineLayout(aDevice, _transparentPipeline._layout, nullptr);
    vkDestroyPipeline(aDevice, _transparentWireframePipeline._pipeline, nullptr);
    vkDestroyPipeline(aDevice, _opaqueWireframePipeline._pipeline,      nullptr);
    vkDestroyPipeline(aDevice, _opaqueWireframePipeline2._pipeline,      nullptr);
    vkDestroyPipeline(aDevice, _opaqueWireframePipeline3._pipeline,      nullptr);
    vkDestroyPipeline(aDevice, _transparentPipeline._pipeline,          nullptr);
    vkDestroyPipeline(aDevice, _opaquePipeline._pipeline,               nullptr);
}

MaterialInstance GLTFMetallic_Roughness::Write_Material(const VkDevice aDevice,
                                                         const MaterialPass aPass,
                                                         const MaterialResources& aResources,
                                                         DescriptorAllocatorGrowable& aDescriptorAllocator,
                                                         const char* aName)
{
    MaterialInstance matData;
    matData._passType = aPass;
    matData._pipeline = (aPass == MaterialPass::Transparent) ? &_transparentPipeline : &_opaquePipeline;
    matData._materialSet = aDescriptorAllocator.Allocate(aDevice, _materialLayout, aName);

    _writer.Clear();
    _writer.Write_Buffer(0, aResources._dataBuffer, sizeof(MaterialConstants),
                         aResources._dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    _writer.Update_Set(aDevice, matData._materialSet);

    return matData;
}
