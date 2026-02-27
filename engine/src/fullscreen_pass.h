#pragma once

class Fullscreen
{
    // TODO. fullscreen pass class similar to DX11 one we had.


    // TODO. Load static / shared vertex shader.
    // load images..? (send as args?)
    // static pipeline state that we can just switch into whenever this runs.
    // init unique vars for this shader frag path, (render target format? push constants?)
}

//class FullscreenPass
//{
//public:
//    VkPipeline pipeline;
//    VkPipelineLayout pipelineLayout;
//
//    // Call this once during engine init for each effect (Tonemap, FXAA, etc.)
//    void init(VkDevice device, VkShaderModule fullscreenVert, VkShaderModule effectFrag, VkFormat outputFormat, VkDescriptorSetLayout inputTextureLayout)
//    {
//        // 1. Build Pipeline Layout
//        VkPipelineLayoutCreateInfo layoutInfo{};
//        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
//        layoutInfo.setLayoutCount = 1;
//        layoutInfo.pSetLayouts = &inputTextureLayout;
//        // Add push constants here if your effect needs them (e.g., exposure value)
//
//        vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
//
//        // 2. Dynamic Rendering Info (Vulkan 1.3)
//        // This tells the pipeline what format to expect without needing a RenderPass!
//        VkPipelineRenderingCreateInfo renderInfo{};
//        renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
//        renderInfo.colorAttachmentCount = 1;
//        renderInfo.pColorAttachmentFormats = &outputFormat;
//
//        // 3. Build the Pipeline
//        // (Assuming you use vkguide's PipelineBuilder here)
//        PipelineBuilder builder;
//        builder.clear();
//        builder.set_shaders(fullscreenVert, effectFrag);
//        builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
//        builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
//        builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
//        builder.set_multisampling_none();
//        builder.disable_depthtest();
//        builder.set_color_attachment_format(outputFormat);
//        builder.disable_blending();
//        builder.set_pipeline_layout(pipelineLayout);
//
//        // vkguide's builder usually lets you attach pNext structs for Dynamic Rendering
//        pipeline = builder.build_pipeline(device);
//    }
//
//    void cleanup(VkDevice device)
//    {
//        vkDestroyPipeline(device, pipeline, nullptr);
//        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
//    }
//
//    // Call this in your command buffer recording loop
//    void draw(VkCommandBuffer cmd, VkDescriptorSet inputTextureSet, VkExtent2D screenSize)
//    {
//        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
//
//        // Bind the image we want to process
//        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &inputTextureSet, 0, nullptr);
//
//        // Handle dynamic viewport/scissor
//        VkViewport viewport{};
//        viewport.width = (float)screenSize.width;
//        viewport.height = (float)screenSize.height;
//        viewport.minDepth = 0.0f;
//        viewport.maxDepth = 1.0f;
//        vkCmdSetViewport(cmd, 0, 1, &viewport);
//
//        VkRect2D scissor{};
//        scissor.extent = screenSize;
//        vkCmdSetScissor(cmd, 0, 1, &scissor);
//
//        // The magic draw call: 3 vertices, 1 instance, no vertex buffers
//        vkCmdDraw(cmd, 3, 1, 0, 0);
//    }
//};

////// 1. Transition your output image (e.g., swapchain) to COLOR_ATTACHMENT_OPTIMAL
////// ... (Insert image memory barrier here) ...
////
////// 2. Start Dynamic Rendering
////VkRenderingAttachmentInfo colorAttachment{};
////colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
////colorAttachment.imageView = currentSwapchainImageView;
////colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
////colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // We are overwriting the whole screen
////colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
////
////VkRenderingInfo renderInfo{};
////renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
////renderInfo.renderArea.extent = swapchainExtent;
////renderInfo.layerCount = 1;
////renderInfo.colorAttachmentCount = 1;
////renderInfo.pColorAttachments = &colorAttachment;
////
////vkCmdBeginRendering(cmd, &renderInfo);
////
////// 3. Execute your post-process
////myTonemapPass.draw(cmd, computeShaderOutputDescriptorSet, swapchainExtent);
////
////vkCmdEndRendering(cmd);
////
////// 4. Transition swapchain image to PRESENT_SRC_KHR
////// ... (Insert image memory barrier here) ...
//
//class PostProcessSystem
//{
//public:
//    // Shared across ALL fullscreen passes
//    VkDescriptorSetLayout singleTextureSetLayout;
//    VkPipelineLayout sharedPipelineLayout;
//    VkShaderModule fullscreenVertexShader;
//
//    // The individual, baked pipelines (because the fragment shaders differ)
//    VkPipeline tonemapPipeline;
//    VkPipeline fxaaPipeline;
//    VkPipeline vignettePipeline;
//
//    void init(VkDevice device, VkFormat swapchainFormat)
//    {
//        // 1. Build the shared layouts ONCE
//        // ... (create singleTextureSetLayout and sharedPipelineLayout)
//
//        // 2. Load the shared vertex shader ONCE
//        // fullscreenVertexShader = load_shader("fullscreen.vert.spv");
//
//        // 3. Build the individual pipelines using the shared data
//        tonemapPipeline = build_effect_pipeline(device, swapchainFormat, "tonemap.frag.spv");
//        fxaaPipeline = build_effect_pipeline(device, swapchainFormat, "fxaa.frag.spv");
//    }
//
//private:
//    VkPipeline build_effect_pipeline(VkDevice device, VkFormat format, const char* fragPath)
//    {
//        VkShaderModule fragShader = load_shader(fragPath);
//
//        PipelineBuilder builder;
//        // USE THE SHARED VERTEX SHADER
//        builder.set_shaders(fullscreenVertexShader, fragShader);
//        // USE THE SHARED PIPELINE LAYOUT
//        builder.set_pipeline_layout(sharedPipelineLayout);
//
//        // ... set topology, culling, disable depth, etc. (same for all) ...
//
//        VkPipeline newPipeline = builder.build_pipeline(device);
//
//        // You can destroy the frag module after the pipeline is built!
//        vkDestroyShaderModule(device, fragShader, nullptr);
//
//        return newPipeline;
//    }
//};

//
//// Bind the shared inputs
//vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sharedPipelineLayout, 0, 1, &myInputTextureSet, 0, nullptr);
//
//// Draw FXAA
//vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fxaaPipeline);
//vkCmdDraw(cmd, 3, 1, 0, 0);
//
//// Need to draw something else right after? Just bind the new pipeline and draw!
//// vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vignettePipeline);
//// vkCmdDraw(cmd, 3, 1, 0, 0);
