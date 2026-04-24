#include <vk/engine.h>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk/images.h>
#include <vk/initializers.h>
#include <vk/types.h>
#include <vk/pipelines.h>

#include <VkBootstrap.h>

#include <algorithm>
#include <chrono>
#include <thread>

#define VMA_LEAK_LOG_FORMAT(format, ...) MOMO_VMA_LEAK_LOG(format, __VA_ARGS__)
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <Input.h>
#include <string_utils.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>

// globals
constexpr bool USE_VALIDATION_LAYERS = true;
constexpr auto APP_NAME = "MomoVK";


void GLTFMetallic_Roughness::Build_Pipelines()
{
    auto& engine = VulkanEngine::Get();
	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.Add_Binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	_materialLayout = layoutBuilder.Build(engine._device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, "GLTFMetallic_Roughness Material");

	VkDescriptorSetLayout layouts[] = { engine._gpuSceneDataDescriptorLayout, _materialLayout };

	VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo mesh_layout_info = momo_vkInit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 2;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine._device, &mesh_layout_info, nullptr, &newLayout));
    // pipeline layout is technically shared between transparent & opaque, and we never bother deleting it so it's not a big deal right now
    MOMO_VK_SET_DEBUG_NAME(engine._device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, newLayout, "_Pipeline Layout GLTFMetallic_Roughness Material Opaque and Transparent");

	_opaquePipeline._layout = newLayout;
	_transparentPipeline._layout = newLayout;

	constexpr auto useHLSL = momo_shaderUtil::ShaderLang::GLSL;
    auto meshFragShader = momo_shaderUtil::load_shader("mesh_pbr", momo_shaderUtil::ShaderType::Fragment, useHLSL, engine._device);
    auto meshVertexShader = momo_shaderUtil::load_shader("mesh", momo_shaderUtil::ShaderType::Vertex, useHLSL, engine._device);

	// build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.Set_Shaders(meshVertexShader.value(), meshFragShader.value());
	pipelineBuilder.Set_Input_Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.Set_Polygon_Mode(VK_POLYGON_MODE_FILL);
    // pipelineBuilder.Set_Polygon_Mode(VK_POLYGON_MODE_LINE);
	pipelineBuilder.Set_Cull_Mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.Set_Multisampling_None();
	pipelineBuilder.Disable_Blending();
	pipelineBuilder.Enable_DepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//render format
	pipelineBuilder.Set_Color_Attachment_Format(engine._drawImage._imageFormat);
	pipelineBuilder.Set_Depth_Format(engine._depthImage._imageFormat);

	// use the triangle layout we created
	pipelineBuilder._pipelineLayout = newLayout;

	// finally build the pipeline
    _opaquePipeline._pipeline = pipelineBuilder.Build_Pipeline(engine._device, "GLTFMetallic_Roughness Opaque");
	
    // create the transparent variant, enable additive blending!
	pipelineBuilder.Enable_Blending_Additive();

	pipelineBuilder.Enable_DepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	_transparentPipeline._pipeline = pipelineBuilder.Build_Pipeline(engine._device, "GLTFMetallic_Roughness Transparent");

	vkDestroyShaderModule(engine._device, meshFragShader.value(), nullptr);
	vkDestroyShaderModule(engine._device, meshVertexShader.value(), nullptr);
}

void GLTFMetallic_Roughness::Clear_Resources(const VkDevice aDevice) const
{
	vkDestroyDescriptorSetLayout(aDevice, _materialLayout, nullptr);
	vkDestroyPipelineLayout(aDevice, _transparentPipeline._layout, nullptr);

	vkDestroyPipeline(aDevice, _transparentPipeline._pipeline, nullptr);
	vkDestroyPipeline(aDevice, _opaquePipeline._pipeline, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::Write_Material(const VkDevice aDevice, const MaterialPass aPass, const MaterialResources& aResources, DescriptorAllocatorGrowable& aDescriptorAllocator, const char* aName)
{
	MaterialInstance matData;
	matData._passType = aPass;
	if (aPass == MaterialPass::Transparent) 
	{
		matData._pipeline = &_transparentPipeline;
	}
	else 
	{
		matData._pipeline = &_opaquePipeline;
	}

	matData._materialSet = aDescriptorAllocator.Allocate(aDevice, _materialLayout, aName);


	_writer.Clear();
	_writer.Write_Buffer(0, aResources._dataBuffer, sizeof(MaterialConstants), aResources._dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	// writer.Write_Image(1, aResources.colorImage.imageView, aResources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	// writer.Write_Image(2, aResources.metalRoughImage.imageView, aResources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	_writer.Update_Set(aDevice, matData._materialSet);

	return matData;
}

void MeshNode::Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx)
{
	const glm::mat4 nodeMatrix = aTopMatrix * _worldTransform;

	for (const auto& s : _mesh->_surfaces) // a mesh can have multiple surfaces with different materials.
	{
		RenderObject def;
		def._indexCount = s._count;
		def._firstIndex = s._startIndex;
		def._indexBuffer = _mesh->_meshBuffers._indexBuffer._buffer;
		def._material = &s._material->_data;
		def._bounds = s._bounds;
		def._transform = nodeMatrix;
		def._vertexBufferAddress = _mesh->_meshBuffers._vertexBufferAddress;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
        def._matDebugName = s._material->debugName;
        def._meshDebugName = _mesh->_name;
        def._combinedDebugLabel = s._combinedDebugLabel.c_str();
#endif
		switch (s._material->_data._passType)
		{
		case MaterialPass::MainColor:
			aCtx._opaqueSurfaces.push_back(def);
			break;
		case MaterialPass::Transparent:
			aCtx._transparentSurfaces.push_back(def);
			break;
		case MaterialPass::Other:
			throw;
		}
	}

	// recurse down
	Node::Draw(aTopMatrix, aCtx);
}

TextureID TextureCache::AddTexture(const VkImageView& aImage, const VkSampler aSampler)
{
    const ViewSamplerKey key{._imageView = aImage, ._sampler = aSampler};

    if (const auto it = _lookup.find(key); it != _lookup.end())
    {
        return TextureID{it->second};
    }

    const VkDescriptorImageInfo info{.sampler = aSampler, .imageView = aImage, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    uint32_t idx;
    if (!_freeSlots.empty())
    {
        idx = *_freeSlots.begin();
        _freeSlots.erase(_freeSlots.begin());
        _cache[idx] = info;
    }
    else
    {
        idx = static_cast<uint32_t>(_cache.size());
        _cache.push_back(info);
    }

    _lookup.emplace(key, idx);
    _dirty = true;
    return TextureID{idx};
}

void TextureCache::MarkEngineImage(const VkImageView aView)
{
    _engineDefaultImages.insert(aView);
}

void TextureCache::FreeTextures(const std::span<const TextureID> aIDs, const VkDescriptorImageInfo& aFallback)
{
    for (const auto [Index] : aIDs)
    {
        if (_engineDefaultImages.contains(_cache[Index].imageView))
            continue;
        if (_freeSlots.contains(Index))
            continue;

        _lookup.erase(ViewSamplerKey{._imageView = _cache[Index].imageView, ._sampler = _cache[Index].sampler});
        _cache[Index] = aFallback;
        _freeSlots.insert(Index);
        _dirty = true;
    }
}

VulkanEngine& VulkanEngine::Get()
{
    static VulkanEngine instance;
    return instance;
}

void VulkanEngine::Init()
{
	// We initialize SDL and create a window with it.
	SDL_Init(SDL_INIT_VIDEO);

	constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

	_window = SDL_CreateWindow(
		APP_NAME,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);
	
	_renderDoc.Load();  // NOLINT(readability-static-accessed-through-instance)
	Init_Vulkan();
	Init_Swapchain();
    _renderDoc.Set_Window(_instance, _window); // NOLINT(readability-static-accessed-through-instance)
	Init_Commands();
	Init_Sync_Structures();
	Init_Descriptors();
	Init_Pipelines();
	Init_ImGui();
	Init_Tracy();
	Init_Default_Data();
    Init_Models();
    Input::Instance().Init();

	_isInitialized = true;
}

void VulkanEngine::Draw()
{
    PROFILE_SCOPE_N("Draw")
	{
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "wait for fences");
	    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
	    VK_CHECK(vkWaitForFences(_device, 1, &Get_Current_Frame()._renderFence, true, 1000000000));
	    Get_Current_Frame()._deletionQueue.Flush();
	    Get_Current_Frame()._frameDescriptors.Clear_Pools(_device);
	}
    uint32_t swapchainImageIndex;
    {
	    // request image from the swapchain
        {
            const VkResult res = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, Get_Current_Frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
            if (res == VK_ERROR_OUT_OF_DATE_KHR)
            {
                // Error code: acquire failed, semaphore was NOT signaled — safe to return early.
                _resizeRequested = true;
                return;
            }
            if (res == VK_SUBOPTIMAL_KHR)
            {
                // Success code: acquire succeeded, semaphore IS signaled, image index is valid.
                // Must continue and render this frame — returning early would orphan the semaphore.
                // Recreate swapchain after present instead.
                _resizeRequested = true;
            }
            else
            {
                VK_CHECK(res);
            }
        }
    }
	VK_CHECK(vkResetFences(_device, 1, &Get_Current_Frame()._renderFence));

	// naming it cmd for shorter writing
	const VkCommandBuffer& cmd = Get_Current_Frame()._mainCommandBuffer;

	// now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	{
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "begin command buffer");
	    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	    const VkCommandBufferBeginInfo cmdBeginInfo = momo_vkInit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	    _drawExtent.height = static_cast<uint32_t>(static_cast<float>(std::min(_swapchainExtent.height, _drawImage._imageExtent.height)));
	    _drawExtent.width = static_cast<uint32_t>(static_cast<float>(std::min(_swapchainExtent.width, _drawImage._imageExtent.width)));
	    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
	}
    {
        // PROFILE_GPU zone must be destroyed before vkEndCommandBuffer — keep it in this block
        // so its destructor (which calls vkCmdWriteTimestamp) fires while the buffer is still recording.
        PROFILE_GPU(_tracyVkCtx, cmd, "Render")
        {
            MOMO_VK_SCOPED_CMD_LABEL(cmd, "transition draw img 1");
            // transition our main draw image into general layout so we can write into it.
            // we will overwrite it all so we don't care about what was the older layout
            momo_vkUtil::transition_image(cmd, _drawImage._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, _drawImage._imageFormat);
            momo_vkUtil::transition_image(cmd, _depthImage._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, _depthImage._imageFormat);
            // TEST TO TRIGGER VALIDATION ERROR:
            // momo_vkUtil::Transition_Image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_UNDEFINED);
        }
        {
            MOMO_VK_SCOPED_CMD_LABEL(cmd, "draw main");
            Draw_Main(cmd);
        }
        {
            MOMO_VK_SCOPED_CMD_LABEL(cmd, "transition draw & swapchain img 3");
            //transition the draw image and the swapchain image into their correct transfer layouts
            momo_vkUtil::transition_image(cmd, _drawImage._image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _drawImage._imageFormat);
            momo_vkUtil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _swapchainImageFormat);

            // execute a copy from the draw image into the swapchain
            momo_vkUtil::copy_image_to_image(cmd, _drawImage._image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

            // set swapchain image layout to Attachment Optimal so we can draw it
            momo_vkUtil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, _swapchainImageFormat);
        }
        {
            MOMO_VK_SCOPED_CMD_LABEL(cmd, "Draw imGui Cmd Buffer");
            MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "Draw imGui Graphics Queue");
            // draw imgui into the swapchain image
            Draw_ImGui(cmd, _swapchainImageViews[swapchainImageIndex]);
        }
        {
            MOMO_VK_SCOPED_CMD_LABEL(cmd, "transition swapchain img 4");
            // set swapchain image layout to Present so we can show it on the screen
            momo_vkUtil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, _swapchainImageFormat);
        }
        PROFILE_GPU_COLLECT(_tracyVkCtx, cmd)
    } // PROFILE_GPU destructor fires here — end timestamp written while buffer is still recording
    {
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "end command buffer");
        //finalize the command buffer (we can no longer add commands, but it can now be executed)
        VK_CHECK(vkEndCommandBuffer(cmd));
    }
    {
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "submit command buffer queue");
	    // prepare the submission to the queue. 
	    // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	    // we will signal the _renderSemaphore, to signal that rendering has finished

	    const VkCommandBufferSubmitInfo cmdInfo = momo_vkInit::command_buffer_submit_info(cmd);

	    const VkSemaphoreSubmitInfo waitInfo = momo_vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, Get_Current_Frame()._swapchainSemaphore);
	    //VkSemaphoreSubmitInfo signalInfo = vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, Get_Current_Frame()._renderSemaphore);
        const VkSemaphoreSubmitInfo signalInfo = momo_vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, _readyForPresentSemaphores[swapchainImageIndex]);

	    const VkSubmitInfo2 submit = momo_vkInit::submit_info(&cmdInfo, &signalInfo, &waitInfo);

	    // submit command buffer to the queue and execute it.
	    // _renderFence will now block until the graphic commands finish execution
	    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, Get_Current_Frame()._renderFence));
    }
    {
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "present");
	    // prepare present
	    // this will put the image we just rendered to into the visible window.
	    // we want to wait on the _renderSemaphore for that, 
	    // as its necessary that drawing commands have finished before the image is displayed to the user
        const VkPresentInfoKHR presentInfo = momo_vkInit::present_info(&_swapchain, &_readyForPresentSemaphores[swapchainImageIndex], &swapchainImageIndex);

	    if (const VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo); 
		    presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) 
	    {
		    _resizeRequested = true;
	    }
	    //increase the number of frames drawn
	    _frameNumber++;
    }
}

void VulkanEngine::Run()
{
	bool bQuit = false;

	uint64_t lastTime = SDL_GetPerformanceCounter();
	while (!bQuit)
	{
        const uint64_t currentTime = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(currentTime - lastTime) / static_cast<float>(_stats._frequency);
        lastTime = currentTime;
        dt = std::min(dt, 0.25f);
        _stats._frameTime = dt * 1000.0f;

        Input::Instance().PreUpdate();
		ProcessEvents(bQuit);
		Input::Instance().PostUpdate();

		// do not draw if we are minimized
		if (_freezeRendering)
		{
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (_resizeRequested)
		{
			Resize_Swapchain();
		}

		ImGui_Update();
        Update_Scene();
        {
	        Draw();
            PROFILE_FRAME;
        }
	}
}

void VulkanEngine::Cleanup()
{
	if (_isInitialized)
	{
		vkDeviceWaitIdle(_device);

		_loadedScenes.clear();

		for (auto& frame : _frames)
		{
			vkDestroyCommandPool(_device, frame._commandPool, nullptr);

			//destroy sync objects
			vkDestroyFence(_device, frame._renderFence, nullptr);
			vkDestroySemaphore(_device, frame._swapchainSemaphore, nullptr);
			//vkDestroySemaphore(_device, _frame._renderSemaphore, nullptr);

			frame._deletionQueue.Flush();
		}
#ifdef TRACY_ENABLE
		if (_tracyVkCtx)
		{
			TracyVkDestroy(_tracyVkCtx)
			_tracyVkCtx = nullptr;
		}
#endif
		// for (const auto& mesh : _testMeshes) 
		// {
		// 	Destroy_Buffer(mesh->meshBuffers._indexBuffer);
		// 	Destroy_Buffer(mesh->meshBuffers._vertexBuffer);
		// }

		_metalRoughMaterial.Clear_Resources(_device);
		_mainDeletionQueue.Flush();

		Destroy_Swapchain();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);

		vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debugMessenger);
        _validationCapture.Destroy(_instance);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::Draw_ImGui(const VkCommandBuffer aCmd, const VkImageView aTargetImageView) const
{
	const VkRenderingAttachmentInfo colorAttachment = momo_vkInit::attachment_info(aTargetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingInfo renderInfo = momo_vkInit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(aCmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), aCmd);

	vkCmdEndRendering(aCmd);
}

void VulkanEngine::Draw_Main(VkCommandBuffer aCmd)
{
    {
        MOMO_VK_SCOPED_CMD_LABEL(aCmd, "draw background");
        Draw_Background(aCmd);
    }
    {
        MOMO_VK_SCOPED_CMD_LABEL(aCmd, "transition draw & depth img 2");
        momo_vkUtil::transition_image(aCmd, _drawImage._image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, _drawImage._imageFormat);
        momo_vkUtil::transition_image(aCmd, _depthImage._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, _depthImage._imageFormat);
    }
    {
        MOMO_VK_SCOPED_CMD_LABEL(aCmd, "Draw Geometry CmdBuff");
        MOMO_VK_SCOPED_QUEUE_LABEL(_graphicsQueue, "Draw Geometry Queue");

        PROFILE_SCOPE_N("Draw Geometry")
        Draw_Geometry(aCmd);
    }
}

void VulkanEngine::Immediate_Submit(const std::function<void(VkCommandBuffer aCmd)>& aFunction) const
{
	VK_CHECK(vkResetFences(_device, 1, &_immFence));
	VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

	const VkCommandBuffer cmd = _immCommandBuffer;

	const VkCommandBufferBeginInfo cmdBeginInfo = momo_vkInit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	aFunction(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	const VkCommandBufferSubmitInfo cmdInfo = momo_vkInit::command_buffer_submit_info(cmd);
	const VkSubmitInfo2 submit = momo_vkInit::submit_info(&cmdInfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

	VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

AllocatedImage VulkanEngine::Create_Image(const VkExtent3D aSize, const VkFormat aFormat, const VkImageUsageFlags aUsage, const char* aName, const bool aMipmapped) const
{
	AllocatedImage newImage;
	newImage._imageFormat = aFormat;
	newImage._imageExtent = aSize;

	VkImageCreateInfo img_Info = momo_vkInit::image_create_info(aFormat, aUsage, aSize);
	if (aMipmapped) 
	{
		img_Info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(aSize.width, aSize.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // used to be VMA_MEMORY_USAGE_GPU_ONLY
	allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &img_Info, &allocInfo, &newImage._image, &newImage._allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (aFormat == VK_FORMAT_D32_SFLOAT) 
	{
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build an image-view for the image
	VkImageViewCreateInfo view_Info = momo_vkInit::imageview_create_info(aFormat, newImage._image, aspectFlag);
	view_Info.subresourceRange.levelCount = img_Info.mipLevels;

	VK_CHECK(vkCreateImageView(_device, &view_Info, nullptr, &newImage._imageView));

    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE, newImage._image, "_Image Name: {}", aName);
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE_VIEW, newImage._imageView, "_ImageView Name: {}", aName);
    vmaSetAllocationName(_allocator, newImage._allocation, aName);
	return newImage;
}

AllocatedImage VulkanEngine::Create_Image(const void* aData, const VkExtent3D aSize, const VkFormat aFormat, const VkImageUsageFlags aUsage, const char* aName, const bool aMipmapped) const
{
	const size_t data_Size = static_cast<size_t>(aSize.depth) * aSize.width * aSize.height * 4;
	// was previously VMA_MEMORY_USAGE_CPU_TO_GPU

	const char* uploadBufferName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string temp = fmt::format("Upload, {}", aName);
    uploadBufferName = temp.c_str();
#endif
    const AllocatedBuffer uploadBuffer = Create_Buffer(data_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, uploadBufferName);

	memcpy(uploadBuffer._info.pMappedData, aData, data_Size);

	const AllocatedImage new_Image = Create_Image(aSize, aFormat, aUsage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, aName, aMipmapped);

	Immediate_Submit([&](const VkCommandBuffer aCmd) 
    {
        momo_vkUtil::transition_image(aCmd, new_Image._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, new_Image._imageFormat);

        const VkBufferImageCopy copyRegion = momo_vkInit::buffer_image_copy(aSize, new_Image._imageFormat);

        // copy the buffer into the image
        vkCmdCopyBufferToImage(aCmd, uploadBuffer._buffer, new_Image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (aMipmapped)
        {
            momo_vkUtil::generate_mipmaps(aCmd, new_Image._image, VkExtent2D{.width = new_Image._imageExtent.width, .height = new_Image._imageExtent.height }, new_Image._imageFormat);
        }
        else
        {
            momo_vkUtil::transition_image(aCmd, new_Image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, new_Image._imageFormat);
        }
    });

	Destroy_Buffer(uploadBuffer);
	return new_Image;
}

void VulkanEngine::Destroy_Image(const AllocatedImage& aImg) const
{
	vkDestroyImageView(_device, aImg._imageView, nullptr);
	vmaDestroyImage(_allocator, aImg._image, aImg._allocation);
}

void VulkanEngine::Init_Vulkan()
{
    PROFILE_SCOPE_N("Init_Vulkan")
    if (auto res = volkInitialize(); 
		res != VK_SUCCESS)
    {
        // Handle error: Vulkan loader wasn't found on the system
        fmt::print("Failed to initialize volk!\n");
        return;
    }

	//> init_instance
	vkb::InstanceBuilder builder;

	//make the vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name(APP_NAME)
	                       .request_validation_layers(USE_VALIDATION_LAYERS)
	                       .use_default_debug_messenger()
	                       .require_api_version(1, 3, 0)
	                       .build();

	vkb::Instance vkb_inst = inst_ret.value();

	// grab the instance 
	_instance = vkb_inst.instance;
	_debugMessenger = vkb_inst.debug_messenger;
	//< init_instance

	volkLoadInstance(_instance);

    if (USE_VALIDATION_LAYERS)
    {
        _validationCapture.Init(_instance);
    }

	//> init_device
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	// vk 1.4 features
	// VkPhysicalDeviceVulkan14Features features14{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
	// features14.pushDescriptor // TODO

	// vk 1.3 features
	VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

	// vk 1.2 features
	VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;
    features12.scalarBlockLayout = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    features12.descriptorBindingSampledImageUpdateAfterBind = true;
    features12.runtimeDescriptorArray = true;

	// vk 1.1 features
	// VkPhysicalDeviceVulkan11Features features11{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};

	// vulkan 1.0 features
	VkPhysicalDeviceFeatures features10{};
	features10.shaderInt64 = true; // needed for vertex pulling in hlsl only


	// Use vk-bootstrap to select a gpu. 
	// We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
	vkb::PhysicalDeviceSelector selector{vkb_inst};
	auto phys_ret = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features13)
		.set_required_features_12(features12)
		.set_required_features(features10)
		.set_surface(_surface)
		.select();

	if (!phys_ret)
	{
		throw std::runtime_error("failed to find a suitable GPU: " + phys_ret.error().message());
	}

    vkb::PhysicalDevice& physicalDevice = phys_ret.value();
    if (const std::vector desiredExtensions = {"VK_EXT_memory_budget"}; 
		!physicalDevice.enable_extensions_if_present(desiredExtensions))
    {
        fmt::print("failed to load some extensions!\n");

		// loop through them now so we avoid that potential bottleneck if everything is supported already
        for (const auto & extension : desiredExtensions)
        {
            if (!physicalDevice.enable_extension_if_present(extension))
            {
                fmt::print("failed to load this specific extension!: {}\n", extension);
            }
        }
    }

	//create the final vulkan device (driver) from the physical device (gpu)
	vkb::DeviceBuilder deviceBuilder{physicalDevice};

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a vulkan application
	_device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;
	
	volkLoadDevice(_device);
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_DEVICE, _device, "_Logical Device");
    
	// set instance name too
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_INSTANCE, _instance, "_Instance");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT, _debugMessenger, "_DebugMessenger");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SURFACE_KHR, _surface, "_Surface");

	//< debug info
	{
        VkPhysicalDeviceDriverProperties driverProps{};
        driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

        VkPhysicalDeviceProperties2 deviceProps2{};
        deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProps2.pNext = &driverProps;

        // Assuming 'physicalDevice' is the vkb::PhysicalDevice returned by your vkb::DeviceBuilder
        vkGetPhysicalDeviceProperties2(_chosenGPU, &deviceProps2);

	    const VkPhysicalDeviceProperties& props2 = deviceProps2.properties;
        // fmt::print("\x1b[2J\x1b[H"); // NOTE: THIS CLEARS THE CONSOLE! ANY ERROR MESSAGE BEFORE THIS WILL NOT BE SEEN!
	    fmt::print("--- Physical Device Properties ---\n");
        fmt::print("Selected GPU: {}\n", props2.deviceName);
        fmt::print("Device Type: {}\n", Get_Device_Type_String(props2.deviceType));
        // fmt::print("Vendor ID: {}\n", props2.vendorID);
        // fmt::print("Device ID: {}\n", props2.deviceID);
        fmt::print("VK API Version: {}.{}.{}\n", VK_API_VERSION_MAJOR(props2.apiVersion), VK_API_VERSION_MINOR(props2.apiVersion), VK_API_VERSION_PATCH(props2.apiVersion));
        fmt::print("Driver Name: {}\n", driverProps.driverName);
        fmt::print("Driver Info: {}\n", driverProps.driverInfo);
	    fmt::print("-----------------------------------\n");
	}
    
	// name both devices in renderdoc as well. idk about calling vkGetPhysicalDeviceProperties twice but whatever.
	{
	    // Ask Vulkan how many GPUs are plugged into the motherboard
	    uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);

        // Fetch the actual list of handles
        std::vector<VkPhysicalDevice> allGPUs(deviceCount);
        vkEnumeratePhysicalDevices(_instance, &deviceCount, allGPUs.data());

	    // NOTE: this might break validation layers because naming a device from a different device is a big no no, but whatever. rn it works!
        for (uint32_t i = 0; i < deviceCount; ++i)
        {
            // Get the hardware properties from the driver
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(allGPUs[i], &props);

            MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_PHYSICAL_DEVICE, allGPUs[i], "_Physical Device/GPU {}: {}", i, props.deviceName);
        }
	}
    //> debug info

    //< init device

	//> init_queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_QUEUE, _graphicsQueue, "_Graphics Queue Main");
	//< init_queue

	//> init vma
	// initialize the memory allocator

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _chosenGPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT |	// used for BDA
                          VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;		// used for vram tracking
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
	// allocatorInfo.pDeviceMemoryCallbacks = &_callbacks; // added by momo
    
    VmaVulkanFunctions vulkanFunctions = {};
    vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vulkanFunctions);
    
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

	_mainDeletionQueue.Push_Function([&]
	{
		vmaDestroyAllocator(_allocator);
	});
	//< init vma
}

void VulkanEngine::Init_Swapchain()
{
    PROFILE_SCOPE_N("Init_Swapchain")
	Create_Swapchain(_windowExtent.width, _windowExtent.height);

	//> create image (fullscreen render target/render image)
	//draw image size will match the window
	const VkExtent3D drawImageExtent = {
        .width = _windowExtent.width,
        .height = _windowExtent.height,
        .depth = 1
	};

	// hardcoding the draw format to 16-bit float 
	// this is set as 16 in the guide and gives validation errors unless it is // momo comment
	_drawImage._imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_drawImage._imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const VkImageCreateInfo rimg_info = momo_vkInit::image_create_info(_drawImage._imageFormat, drawImageUsages, drawImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_AUTO; // was VMA_MEMORY_USAGE_GPU_ONLY
	rimg_allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage._image, &_drawImage._allocation, nullptr);
    vmaSetAllocationName(_allocator, _drawImage._allocation, "Draw Image");

	// build an image-view for the draw image to use for rendering
	const VkImageViewCreateInfo rview_info = momo_vkInit::imageview_create_info(_drawImage._imageFormat, _drawImage._image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage._imageView));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE, _drawImage._image, "_Image Main Draw ");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE_VIEW, _drawImage._imageView, "_Image View Main Draw");
	//< create image

	//> create depth
	_depthImage._imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage._imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	const VkImageCreateInfo dimg_info = momo_vkInit::image_create_info(_depthImage._imageFormat, depthImageUsages, drawImageExtent);

	//allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage._image, &_depthImage._allocation, nullptr);
    vmaSetAllocationName(_allocator, _depthImage._allocation, "Depth Image");

	//build an image-view for the draw image to use for rendering
	const VkImageViewCreateInfo dview_info = momo_vkInit::imageview_create_info(_depthImage._imageFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage._imageView));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE, _depthImage._image, "_Image Main Depth");
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE_VIEW, _depthImage._imageView, "_Image View Main Depth");
	//< create depth


	//add to deletion queues
	_mainDeletionQueue.Push_Function([this]
	{
		// main img
		vkDestroyImageView(_device, _drawImage._imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage._image, _drawImage._allocation);
		
		// depth img
		vkDestroyImageView(_device, _depthImage._imageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
	});
}

void VulkanEngine::Init_Commands()
{
    PROFILE_SCOPE_N("Init_Commands")
	// create a command pool for commands submitted to the graphics queue.
	// we also want the pool to allow for resetting of individual command buffers
	const VkCommandPoolCreateInfo commandPoolInfo = momo_vkInit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (auto& frame : _frames)
    {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &frame._commandPool));
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_COMMAND_POOL, frame._commandPool, "_Command Pool Main, FIF: {}", &frame - _frames);

        // allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = momo_vkInit::command_buffer_allocate_info(frame._commandPool, 1);
		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &frame._mainCommandBuffer));

        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_COMMAND_BUFFER, frame._mainCommandBuffer, "_Command Buffer Main, FIF: {}", &frame - _frames);
	}

	// immediate command pool / buffer.
	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_COMMAND_POOL, _immCommandPool, "_Command Pool Immediate");

	// allocate the command buffer for immediate submits
	const VkCommandBufferAllocateInfo cmdAllocInfo = momo_vkInit::command_buffer_allocate_info(_immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_COMMAND_BUFFER, _immCommandBuffer, "_Command Buffer Immediate");

	_mainDeletionQueue.Push_Function([this]
	{
		vkDestroyCommandPool(_device, _immCommandPool, nullptr);
	});
}

void VulkanEngine::Init_Sync_Structures()
{
    PROFILE_SCOPE_N("Init_Sync_Structures")
	// create synchronization structures
	// one fence to control when the gpu has finished rendering the frame, and 2 semaphores to synchronize rendering with swapchain
	// we want the fence to start signalled so we can wait on it on the first frame

	const VkFenceCreateInfo fenceCreateInfo = momo_vkInit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	const VkSemaphoreCreateInfo semaphoreCreateInfo = momo_vkInit::semaphore_create_info();

    for (auto& frame : _frames)
    {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &frame._renderFence));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &frame._swapchainSemaphore));
		//VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &frame._renderSemaphore)); // moved to 2nd for loop
		
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_FENCE, frame._renderFence, "_RenderFence Frame FIF:{}", i);
        
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SEMAPHORE, frame._swapchainSemaphore, "_Semaphore Frame Swapchain, FIF:{}", i);
    }
	
    _readyForPresentSemaphores.resize(_swapchainImageCount);
	
    for (size_t i = 0; i < _swapchainImageCount; ++i)
	{
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_readyForPresentSemaphores[i]));
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SEMAPHORE, _readyForPresentSemaphores[i], "_Semaphore Ready For Present, SwapchainImgCount:{}", i);
        _mainDeletionQueue.Push_Function([this, i]
        {
            vkDestroySemaphore(_device, _readyForPresentSemaphores[i], nullptr);
        });
	}
    
	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_FENCE, _immFence, "_Fence Immediate");
	_mainDeletionQueue.Push_Function([this] { vkDestroyFence(_device, _immFence, nullptr); });
}

void VulkanEngine::Init_Descriptors()
{
    PROFILE_SCOPE_N("Init_Descriptors")
	//create a descriptor pool that will hold 10 sets with 1 image each
	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
	{
		{._type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ._ratio = 1 },
		{._type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ._ratio = 1 },
		{._type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ._ratio = 1 }
	};

	_globalDescriptorAllocator.Init(_device, 10, sizes, "globalDescriptorAllocator");

	// for our compute draw
	{
		DescriptorLayoutBuilder builder;
		builder.Add_Binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.Build(_device, VK_SHADER_STAGE_COMPUTE_BIT, "drawImage");
	}
	// }
	// for our draw image
	{
		DescriptorLayoutBuilder builder;
		builder.Add_Binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.Add_Binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindFlags = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .pNext = nullptr};

        const std::array<VkDescriptorBindingFlags, 2> flagArray{
            0,
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        };

        builder._bindings[1].descriptorCount = 4048;

        bindFlags.bindingCount = 2;
        bindFlags.pBindingFlags = flagArray.data();

        _gpuSceneDataDescriptorLayout = builder.Build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, "gpuSceneData", &bindFlags, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
	}

	_drawImageDescriptors = _globalDescriptorAllocator.Allocate(_device, _drawImageDescriptorLayout, "drawImage");
	{
		DescriptorWriter writer;
		writer.Write_Image(0, _drawImage._imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.Update_Set(_device, _drawImageDescriptors);
	}
	
	//make sure both the descriptor allocator and the new layout get cleaned up properly
	_mainDeletionQueue.Push_Function([&]
	{
		_globalDescriptorAllocator.Destroy_Pools(_device);

		vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
	});

	const char* debugStr = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
	std::string debugStr2 = {};
#endif
    for (unsigned int i = 0; i < FRAME_OVERLAP; i++)   // NOLINT(modernize-loop-convert)
	{
		// create a descriptor pool
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_Sizes =
		{
			{._type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ._ratio = 3 },
			{._type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ._ratio = 4 },
		};

		_frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
        debugStr2 = fmt::format("Frame, FIF: {}", i);
        debugStr = debugStr2.c_str();
#endif
        _frames[i]._frameDescriptors.Init(_device, 1000, frame_Sizes, debugStr, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

#ifdef MOMOVK_ENABLE_DEBUG_NAMES
        debugStr2 = fmt::format("GPUSceneData, FIF: {}", i);
        debugStr = debugStr2.c_str();
#endif
        _frames[i]._sceneDataBuffer = Create_Buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, debugStr);

		_mainDeletionQueue.Push_Function([&, i]
		{
			_frames[i]._frameDescriptors.Destroy_Pools(_device);
            Destroy_Buffer(_frames[i]._sceneDataBuffer);
		});
	}

    // Persistent pool: one global descriptor set per frame slot.
    // UBO binding written once here; texture array patched lazily when texCache._dirty is set.
    {
        constexpr uint32_t kMaxTextures = 4048;
        const std::array<VkDescriptorPoolSize, 2> poolSizes{{
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         FRAME_OVERLAP},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, FRAME_OVERLAP * kMaxTextures},
        }};
        VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags    = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets  = FRAME_OVERLAP;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();
        VK_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_persistentDescPool));

        std::array<uint32_t, FRAME_OVERLAP> varCounts;
        varCounts.fill(kMaxTextures);
        VkDescriptorSetVariableDescriptorCountAllocateInfo varInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .descriptorSetCount = FRAME_OVERLAP,
            .pDescriptorCounts  = varCounts.data(),
        };

        std::array<VkDescriptorSetLayout, FRAME_OVERLAP> layouts;
        layouts.fill(_gpuSceneDataDescriptorLayout);
        VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.pNext              = &varInfo;
        allocInfo.descriptorPool     = _persistentDescPool;
        allocInfo.descriptorSetCount = FRAME_OVERLAP;
        allocInfo.pSetLayouts        = layouts.data();
        VK_CHECK(vkAllocateDescriptorSets(_device, &allocInfo, _persistentGlobalDescriptors.data()));

        // Write UBO binding once per frame slot — buffer handle never changes.
        for (uint32_t i = 0; i < FRAME_OVERLAP; i++)
        {
            DescriptorWriter writer;
            writer.Write_Buffer(0, _frames[i]._sceneDataBuffer._buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            writer.Update_Set(_device, _persistentGlobalDescriptors[i]);
        }

        _mainDeletionQueue.Push_Function([&]
        {
            vkDestroyDescriptorPool(_device, _persistentDescPool, nullptr);
        });
    }
}

void VulkanEngine::Init_Pipelines()
{
    PROFILE_SCOPE_N("Init_Pipelines")
	// compute pipelines
	Init_Background_Pipelines();

	_metalRoughMaterial.Build_Pipelines();
}

void VulkanEngine::Init_Background_Pipelines()
{
    PROFILE_SCOPE_N("Init_Background_Pipelines")
	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(ComputePushConstants);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	
    VkPipelineLayoutCreateInfo computeLayout = momo_vkInit::pipeline_layout_create_info();
	computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;
	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_computePipelineLayout));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, _computePipelineLayout, "_Pipeline Layout Compute Background");

	constexpr auto useHLSL = momo_shaderUtil::ShaderLang::GLSL;
    auto gradientShader = momo_shaderUtil::load_shader("gradient_color", momo_shaderUtil::ShaderType::Compute, useHLSL, _device);
    auto skyShader = momo_shaderUtil::load_shader("sky", momo_shaderUtil::ShaderType::Compute, useHLSL, _device);

	VkPipelineShaderStageCreateInfo stageInfo = momo_vkInit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, gradientShader.value());

	VkComputePipelineCreateInfo computePipelineCreateInfo = momo_vkInit::compute_pipeline_create_info(_computePipelineLayout, stageInfo);

	ComputeEffect gradient{};
	gradient._layout = _computePipelineLayout;
	gradient._name = "gradient";
	gradient._data = {};

	//default colors
	gradient._data._data1 = glm::vec4(1, 0, 0, 1);
	gradient._data._data2 = glm::vec4(0, 0, 1, 1);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient._pipeline));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_PIPELINE, gradient._pipeline, "_Pipeline Compute Gradient");
	_backgroundEffects.push_back(gradient);

	//change the shader module only to create the sky shader
	computePipelineCreateInfo.stage.module = skyShader.value();

	ComputeEffect sky{};
	sky._layout = _computePipelineLayout;
	sky._name = "sky";
	sky._data = {};
	//default sky parameters
	sky._data._data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky._pipeline));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_PIPELINE, sky._pipeline, "_Pipeline Compute Sky");
	
    //add the 2 background effects into the array
	_backgroundEffects.push_back(sky);

	//destroy structures properly
	vkDestroyShaderModule(_device, gradientShader.value(), nullptr);
	vkDestroyShaderModule(_device, skyShader.value(), nullptr);
	_mainDeletionQueue.Push_Function([this, sky, gradient]
	{
		vkDestroyPipelineLayout(_device, _computePipelineLayout, nullptr);
		vkDestroyPipeline(_device, sky._pipeline, nullptr);
		vkDestroyPipeline(_device, gradient._pipeline, nullptr);
	});
}

void VulkanEngine::Init_ImGui()
{
    PROFILE_SCOPE_N("Init_ImGui")
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	const VkDescriptorPoolSize pool_sizes[] = {
		{.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1000},
		{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1000},
		{.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1000},
		{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1000},
		{.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, .descriptorCount = 1000},
		{.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, .descriptorCount = 1000},
		{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1000},
		{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1000},
		{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1000},
		{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .descriptorCount = 1000},
		{.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, .descriptorCount = 1000}
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imGuiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imGuiPool));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, imGuiPool, "_Descriptor Pool imGui");
	// 2: initialize imgui library

	IMGUI_CHECKVERSION();
	// this initializes the core structures of imgui
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

	// Setup Dear ImGui style
	//ImGui::StyleColorsDark();
	ImGui::StyleColorsClassic();

	// this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(_window);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _chosenGPU;
	init_info.Device = _device;
	init_info.Queue = _graphicsQueue;
	init_info.DescriptorPool = imGuiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;

	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
	// - Read 'docs/FONTS.md' for more instructions and details. If you like the default font but want it to scale better, consider using the 'ProggyVector' from the same author!
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//style.FontSizeBase = 20.0f;
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
	//IM_ASSERT(font != nullptr);

	// queue the destruction of imgui created structures
	_mainDeletionQueue.Push_Function([this, imGuiPool]
	{
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imGuiPool, nullptr);
	});
}

void VulkanEngine::Init_Tracy()
{
    PROFILE_SCOPE_N("Init_Tracy")
#ifdef TRACY_ENABLE
	// TracyVkContext requires the command buffer in the *initial* state (reset, not yet begun).
	// Tracy manages vkBeginCommandBuffer / record / vkEndCommandBuffer / submit internally.
	// Wrapping in Immediate_Submit pre-begins the buffer, causing a double-begin crash.
	VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));
	_tracyVkCtx = TracyVkContext(_chosenGPU, _device, _graphicsQueue, _immCommandBuffer)
	TracyVkContextName(_tracyVkCtx, "_Main Graphics Queue", sizeof("_Main Graphics Queue") - 1)
#endif
}

void VulkanEngine::Init_Default_Data()
{
    PROFILE_SCOPE_N("Init_Default_Data")
	// std::array<Vertex, 4> rect_vertices;
	//
	// rect_vertices[0].pos = {0.8, -0.5, 0};
	// rect_vertices[1].pos = {0.5, 0.5, 0};
	// rect_vertices[2].pos = {-0.5, -0.5, 0};
	// rect_vertices[3].pos = {-0.5, 0.5, 0};
	//
	// rect_vertices[0].color = {0, 0, 0, 1};
	// rect_vertices[1].color = {0.5, 0.5, 0.5, 1};
	// rect_vertices[2].color = {1, 0, 0, 1};
	// rect_vertices[3].color = {0, 1, 0, 1};
	//
	// std::array<uint32_t, 6> rect_indices;
	//
	// rect_indices[0] = 0;
	// rect_indices[1] = 1;
	// rect_indices[2] = 2;
	//
	// rect_indices[3] = 2;
	// rect_indices[4] = 1;
	// rect_indices[5] = 3;

	// _rectangle = UploadMesh(rect_indices, rect_vertices);

	//delete the rectangle data on engine shutdown
	// _mainDeletionQueue.Push_Function([&]
	// {
	// 	// Destroy_Buffer(_rectangle._indexBuffer);
	// 	// Destroy_Buffer(_rectangle._vertexBuffer);
	// });
    _stats._frequency = SDL_GetPerformanceFrequency();

	_mainCamera._velocity = glm::vec3(0.f);
	// _mainCamera.position = glm::vec3(30.f, -00.f, -085.f); // for structure
	_mainCamera._position = glm::vec3(0.f, 5.f, 0.f);

	_mainCamera._pitch = 0;
	_mainCamera._yaw = 0;

	const uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage = Create_Image(&white, VkExtent3D{.width = 1, .height = 1, .depth = 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, "Default_White");
	const uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage = Create_Image(&grey, VkExtent3D{.width = 1, .height = 1, .depth = 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, "Default_Grey");
	const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage = Create_Image(&black, VkExtent3D{.width = 1, .height = 1, .depth = 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, "Default_Black");

	const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) 
	{
		for (int y = 0; y < 16; y++) 
		{
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_errorCheckerboardImage = Create_Image(pixels.data(), VkExtent3D{.width = 16, .height = 16, .depth = 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, "Default_ErrorCheckerboard");

    _texCache.MarkEngineImage(_whiteImage._imageView);
    _texCache.MarkEngineImage(_greyImage._imageView);
    _texCache.MarkEngineImage(_blackImage._imageView);
    _texCache.MarkEngineImage(_errorCheckerboardImage._imageView);

	VkSamplerCreateInfo sampler = momo_vkInit::sampler_create_info(VK_FILTER_NEAREST);
	VK_CHECK(vkCreateSampler(_device, &sampler, nullptr, &_defaultSamplerNearest));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SAMPLER, _defaultSamplerNearest, "_Sampler Default Nearest");

	// linear blurs
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	VK_CHECK(vkCreateSampler(_device, &sampler, nullptr, &_defaultSamplerLinear));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SAMPLER, _defaultSamplerLinear, "_Sampler Default Linear");
	

	_mainDeletionQueue.Push_Function([&]
	{
		vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
		vkDestroySampler(_device, _defaultSamplerLinear, nullptr);

		Destroy_Image(_whiteImage);
		Destroy_Image(_greyImage);
		Destroy_Image(_blackImage);
		Destroy_Image(_errorCheckerboardImage);
	});

	//<materials
	GLTFMetallic_Roughness::MaterialResources materialResources;
	// default the material textures
	materialResources._colorImage = _whiteImage;
	materialResources._colorSampler = _defaultSamplerLinear;
	materialResources._metalRoughImage = _whiteImage;
	materialResources._metalRoughSampler = _defaultSamplerLinear;

	// set the uniform buffer for the material data.
	// was previously VMA_MEMORY_USAGE_CPU_TO_GPU
    AllocatedBuffer materialConstants = Create_Buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, "MaterialConstants");

	// cast to struct, ptr on buffer matches the struct data (we assume)
	GLTFMetallic_Roughness::MaterialConstants* sceneUniformData = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(materialConstants._info.pMappedData);
    sceneUniformData->_colorFactors = glm::vec4{1, 1, 1, 1};
    sceneUniformData->_metalRoughFactors = glm::vec4{1, 0.5, 0, 0};

	_mainDeletionQueue.Push_Function([materialConstants, this]
	{
		Destroy_Buffer(materialConstants);
	});

	materialResources._dataBuffer = materialConstants._buffer;
	materialResources._dataBufferOffset = 0;
}

void VulkanEngine::Init_Models()
{
    const std::string structurePath = {R"(..\..\assets\sponza\sponza-png.glb)"};
    // const std::string structurePath = {R"(..\..\assets\sponza\sponza-avif-hi.glb)"}; // unsupported model test
    // const std::string structurePath = {R"(..\..\assets\structure.glb)"};
    const auto structureFile = momo_vkGLTF::load_gltf(structurePath);
    assert(structureFile.has_value());

    _loadedScenes["structure"] = *structureFile;
}

void VulkanEngine::Create_Swapchain(const uint32_t aWidth, const uint32_t aHeight)
{
	vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};

	_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{
		  .format = _swapchainImageFormat,
		  .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		})
		//use vsync present mode
        // https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html#VkPresentModeKHR
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) 
		.set_desired_extent(aWidth, aHeight)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	_swapchainExtent = vkbSwapchain.extent;
	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

	// Set _swapchainImageCount to the amount of swapchain images. 
	// used to initialize the same amount of _readyForPresentSemaphores in init_sync_structures
    VK_CHECK(vkGetSwapchainImagesKHR(_device, _swapchain, &_swapchainImageCount, nullptr));

	MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, _swapchain, "_Swapchain");

    for (size_t i = 0; i < _swapchainImages.size(); ++i)
    {
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE, _swapchainImages[i], "_Image Swapchain {}", i);
        MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE_VIEW, _swapchainImageViews[i], "_Image View Swapchain {}", i);
    }
}

void VulkanEngine::Destroy_Swapchain() const
{
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (const auto& swapchainImageView : _swapchainImageViews)
	{
		vkDestroyImageView(_device, swapchainImageView, nullptr);
	}
}

void VulkanEngine::Draw_Background(const VkCommandBuffer aCmd) const
{
	//make a clear-color from frame number. This will flash with a 120 frame period.
	//VkClearColorValue clearValue;
	//float flash = std::abs(std::sin(_frame_number / 120.f));
	//clearValue = {{0.0f, 0.0f, flash, 1.0f}};

	//VkImageSubresourceRange clearRange = vkInit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	//clear image
	//vkCmdClearColorImage(aCmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	const ComputeEffect& effect = _backgroundEffects[_currentBackgroundEffect];

	// bind the gradient drawing compute pipeline
	vkCmdBindPipeline(aCmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect._pipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(aCmd, _computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect._data);

	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(aCmd, static_cast<uint32_t>(std::ceil(_drawExtent.width / 16.0)), static_cast<uint32_t>(std::ceil(_drawExtent.height / 16.0)), 1);
}

void VulkanEngine::ImGui_Run()
{
    if (ImGui::Begin("settings"))
    {
        _validationCapture.DrawImGui();

        // ── Background ───────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Background"))
        {
            ComputeEffect& selected = _backgroundEffects[_currentBackgroundEffect];
            ImGui::Text("Selected effect: %s", selected._name);
            ImGui::SliderInt("Effect Index", &_currentBackgroundEffect, 0, static_cast<int>(_backgroundEffects.size() - 1));
            ImGui::ColorEdit4("data1", reinterpret_cast<float*>(&selected._data._data1));
            ImGui::ColorEdit4("data2", reinterpret_cast<float*>(&selected._data._data2));
            ImGui::ColorEdit4("data3", reinterpret_cast<float*>(&selected._data._data3));
            ImGui::ColorEdit4("data4", reinterpret_cast<float*>(&selected._data._data4));
        }

        // ── Camera ───────────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Camera"))
        {
            ImGui::SliderFloat("FOV", &_mainCamera._tempCameraFov, 1, 180);
            ImGui::Value("Pitch (rad)", _mainCamera._pitch);
        }

        // ── Lighting ─────────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Lighting"))
        {
            ImGui::ColorEdit4("Sun Color", reinterpret_cast<float*>(&_tempSunColor));
            ImGui::DragFloat4("Sun Direction", reinterpret_cast<float*>(&_tempSunDir), 0.1f);
            ImGui::DragFloat4("Ambient Color", reinterpret_cast<float*>(&_tempAmbientColor), 0, 2.f);
        }

        // ── Stats ────────────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Frame Time: %.3f ms (%.1f FPS)", _stats._frameTime, 1000.0f / _stats._frameTime);
            ImGui::Text("Draw Time:   %.3f ms", _stats._meshDrawTime);
            ImGui::Text("Update Time: %.3f ms", _stats._sceneUpdateTime);
            ImGui::Separator();
            ImGui::Text("Triangles:         %s", momo_stringUtils::format_with_commas(_stats._triCount).c_str());
            ImGui::Text("Total Draws:       %s", momo_stringUtils::format_with_commas(_stats._totalDrawCallCount).c_str());
            ImGui::Text("Opaque Draws:      %s", momo_stringUtils::format_with_commas(_stats._opaqueDrawCallCount).c_str());
            ImGui::Text("Transparent Draws: %s", momo_stringUtils::format_with_commas(_stats._transparentDrawCallCount).c_str());
            ImGui::Separator();
            ImGui::Text("Models Loaded:       %llu", _loadedScenes.size());
            ImGui::Text("Opaque Surfaces:     %llu", _mainDrawContext._opaqueSurfaces.size());
            ImGui::Text("Transparent Surfaces:%llu", _mainDrawContext._transparentSurfaces.size());
        }

        // ── Textures ─────────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Textures"))
        {
            ImGui::Text("Cache Size:       %llu", _texCache._cache.size());
            ImGui::Text("Engine Defaults:  %llu", _texCache._engineDefaultImages.size());
            ImGui::Text("Free Slots:       %llu", _texCache._freeSlots.size());
        }

        // ── Memory ───────────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // VRAM budget — queries driver every frame (fast with VK_EXT_memory_budget)
            {
                const VkPhysicalDeviceMemoryProperties* memProps;
                vmaGetMemoryProperties(_allocator, &memProps);

                VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
                vmaGetHeapBudgets(_allocator, budgets);

                VkDeviceSize totalVramUsage = 0;
                VkDeviceSize totalVramBudget = 0;
                for (uint32_t i = 0; i < memProps->memoryHeapCount; ++i)
                {
                    if (memProps->memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                    {
                        totalVramUsage  += budgets[i].usage;
                        totalVramBudget += budgets[i].budget;
                    }
                }

                const double usageMB  = static_cast<double>(totalVramUsage)  / (1024.0 * 1024.0);
                const double budgetMB = static_cast<double>(totalVramBudget) / (1024.0 * 1024.0);
                ImGui::Text("VRAM: %.2f MB / %.2f MB", usageMB, budgetMB);
                if (totalVramBudget > 0)
                {
                    const float fraction = static_cast<float>(totalVramUsage) / static_cast<float>(totalVramBudget);
                    ImGui::ProgressBar(fraction, ImVec2(-1.f, 0.f));
                }
            }

            ImGui::Separator();

            // VMA internal stats — expensive, cached every 5 seconds
            {
                const auto now = std::chrono::steady_clock::now();
                if (now - _lastVmaStatsTime >= std::chrono::seconds(5))
                {
                    vmaCalculateStatistics(_allocator, &_cachedVmaStats);
                    _lastVmaStatsTime = now;
                }
                const VmaTotalStatistics& stats = _cachedVmaStats;

                const double allocatedMB       = static_cast<double>(stats.total.statistics.allocationBytes) / (1024.0 * 1024.0);
                const double blockMB           = static_cast<double>(stats.total.statistics.blockBytes)      / (1024.0 * 1024.0);
                const double allocationSizeMaxMB = static_cast<double>(stats.total.allocationSizeMax)        / (1024.0 * 1024.0);
                const uint64_t minSize = (stats.total.statistics.allocationCount == 0) ? 0 : stats.total.allocationSizeMin;

                ImGui::Text("VMA Allocated:  %.2f MB", allocatedMB);
                ImGui::Text("VMA Blocks:     %.2f MB", blockMB);
                ImGui::Text("Alloc Count:    %u",      stats.total.statistics.allocationCount);
                ImGui::Text("Block Count:    %u",      stats.total.statistics.blockCount);
                ImGui::Text("Alloc Max:      %.2f MB", allocationSizeMaxMB);
                ImGui::Text("Alloc Min:      %llu B",  minSize);
            }
        }

        // ── RenderDoc ────────────────────────────────────────────────────────
        if (const bool isRenderDocLoaded = _renderDoc.Is_Loaded();
            ImGui::CollapsingHeader("RenderDoc", isRenderDocLoaded ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        {
            if (isRenderDocLoaded) // NOLINT(readability-static-accessed-through-instance)
            {
                if (ImGui::Button("Trigger Capture"))
                    _renderDoc.Trigger_Capture(); // NOLINT(readability-static-accessed-through-instance)
                ImGui::SameLine();
                if (ImGui::Button("Open in RenderDoc"))
                    _renderDoc.Launch_Replay_UI(); // NOLINT(readability-static-accessed-through-instance)
            }
            else
            {
                ImGui::TextDisabled("Not loaded. Enable CMake option or launch via RenderDoc.");
            }
        }

        ImGui::End();
    }
}

void VulkanEngine::ImGui_Update()
{
    PROFILE_SCOPE_N("ImGuiFrame")
    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ////some imgui UI to test
    // ImGui::ShowDemoWindow();

    ImGui_Run();

    // make imgui calculate internal draw structures, doesn't actually render!!! just builds the render data.
    ImGui::Render();
}

void VulkanEngine::Draw_Geometry(const VkCommandBuffer aCmd)
{
	// reset counters
	_stats._totalDrawCallCount = 0;
	_stats._triCount = 0;
    _stats._opaqueDrawCallCount = 0;
    _stats._transparentDrawCallCount = 0;

	auto start = std::chrono::system_clock::now();

	std::vector<uint32_t> opaque_draws;
	opaque_draws.reserve(_mainDrawContext._opaqueSurfaces.size());

	for (uint32_t i = 0; i < _mainDrawContext._opaqueSurfaces.size(); i++) 
	{
		if (Is_Visible(_mainDrawContext._opaqueSurfaces[i], _sceneData._viewProj))
		{
			opaque_draws.push_back(i);
		}
	}

	std::vector<uint32_t> transparent_draws;
	transparent_draws.reserve(_mainDrawContext._transparentSurfaces.size());
	
	for (uint32_t i = 0; i < _mainDrawContext._transparentSurfaces.size(); i++)
	{
		if (Is_Visible(_mainDrawContext._transparentSurfaces[i], _sceneData._viewProj))
		{
			transparent_draws.push_back(i);
		}
	}

	// TODO:
	// Another way of doing this is that we would calculate a sort key, and then our opaque_draws would be something like 20 bits draw index, and 44 bits for sort key / hash.That way would be faster than this as it can be sorted through faster methods.
	// this is also done every frame which I don't know is needed? since when do shaders on objects change.....? not that often right?
	// TODO: multithread? maybe?
	
	// sort the opaque surfaces by material and mesh
	std::ranges::sort(opaque_draws, [&](const auto& anIndexA, const auto& anIndexB) 
	{
		const RenderObject& a = _mainDrawContext._opaqueSurfaces[anIndexA];
		const RenderObject& b = _mainDrawContext._opaqueSurfaces[anIndexB];
		if (a._material == b._material) 
		{
			return a._indexBuffer < b._indexBuffer;
		}
		return a._material < b._material;
	});
	
	// TODO- With the transparent objects, you want to also change the sorting code so that it checks distance from bounds to the camera, so that objects draw more correct. But sorting by depth is incompatible with sorting by pipeline, so you will need to decide what works better for your case.
	std::ranges::sort(transparent_draws, [&](const auto& anIndexA, const auto& anIndexB)
	{
		// pipeline only sorting
		
		// const RenderObject& A = _mainDrawContext.transparentSurfaces[iA];
		// const RenderObject& B = _mainDrawContext.transparentSurfaces[iB];
		// if (A.material == B.material)
		// {
		// 	return A.indexBuffer < B.indexBuffer;
		// }
		// return A.material < B.material;
		
		// depth only sorting (slop)
		
		// const RenderObject& A = _mainDrawContext.transparentSurfaces[iA];
		// const RenderObject& B = _mainDrawContext.transparentSurfaces[iB];
		// // Assume you have a camera position (vec3) and each RenderObject has a bounds center (vec3).
		// const glm::vec3 cameraPos = _sceneData.view[3];
		// float distA = glm::distance(cameraPos, A.bounds.origin);  // Or use sqrDistance for speed.
		// float distB = glm::distance(cameraPos, B.bounds.origin);
		// return distA > distB;  // Farther first (back-to-front).

		// material first, then depth within material group (slop)

		const RenderObject& a = _mainDrawContext._transparentSurfaces[anIndexA];
		const RenderObject& b = _mainDrawContext._transparentSurfaces[anIndexB];
		if (a._material != b._material) 
		{
			return a._material < b._material;  // Batch materials.
		}
		const glm::vec3 cameraPos = _sceneData._view[3];
		const float distSqA = distance2(cameraPos, a._bounds._origin);
		const float distSqB = distance2(cameraPos, b._bounds._origin);
		if (distSqA > distSqB) return true;
		if (distSqA < distSqB) return false;

		// Tertiary: stable tie-breaker
		return a._indexBuffer < b._indexBuffer;	
	});

	//begin a render pass  connected to our draw image
	const VkRenderingAttachmentInfo colorAttachment = momo_vkInit::attachment_info(_drawImage._imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingAttachmentInfo depthAttachment = momo_vkInit::depth_attachment_info(_depthImage._imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	const VkRenderingInfo renderInfo = momo_vkInit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(aCmd, &renderInfo);

	// vkCmdBindPipeline(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

	//set dynamic viewport and scissor
	VkViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = static_cast<float>(_drawExtent.width);
	viewport.height = static_cast<float>(_drawExtent.height);
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(aCmd, 0, 1, &viewport);

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = _drawExtent.width;
	scissor.extent.height = _drawExtent.height;

	vkCmdSetScissor(aCmd, 0, 1, &scissor);

    *static_cast<GPUSceneData*>(Get_Current_Frame()._sceneDataBuffer._info.pMappedData) = _sceneData;

    if (_texCache._dirty && !_texCache._cache.empty())
    {
        const VkWriteDescriptorSet arrayWrite{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding      = 1,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(_texCache._cache.size()),
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = _texCache._cache.data(),
        };
        for (VkDescriptorSet set : _persistentGlobalDescriptors)
        {
            VkWriteDescriptorSet write = arrayWrite;
            write.dstSet = set;
            vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);
        }
        _texCache._dirty = false;
    }

    const VkDescriptorSet globalDescriptor = _persistentGlobalDescriptors[_frameNumber % FRAME_OVERLAP];
	
	//defined outside the draw function, this is the state we will try to skip
	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    {
        auto draw = [&](const RenderObject& aR) 
        {
            if (aR._material != lastMaterial)
            {	
                MOMO_VK_SCOPED_CMD_LABEL(aCmd, "Switching Material");
                lastMaterial = aR._material;
                // rebind pipeline and descriptors if the material changed
                if (aR._material->_pipeline != lastPipeline)
                {
                    lastPipeline = aR._material->_pipeline;

                    vkCmdBindPipeline(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, aR._material->_pipeline->_pipeline);
                    vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, aR._material->_pipeline->_layout, 0, 1, &globalDescriptor, 0, nullptr);
                }
                vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, aR._material->_pipeline->_layout, 1, 1, &aR._material->_materialSet, 0, nullptr);

            }
	    
            if (aR._indexBuffer != lastIndexBuffer)
            {
                lastIndexBuffer = aR._indexBuffer;
                vkCmdBindIndexBuffer(aCmd, aR._indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            }
		
            GPUDrawPushConstants pushConstants;
            pushConstants._worldMatrix = aR._transform;
            pushConstants._vertexBuffer = aR._vertexBufferAddress;
            vkCmdPushConstants(aCmd, aR._material->_pipeline->_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

#ifdef MOMOVK_ENABLE_RENDERDOC
            {
                const char* const passStr = (aR._material->_passType == MaterialPass::Transparent) ? "transparent" : "opaque";
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
                _renderDoc.Annotate_Draw(aCmd, aR._matDebugName.data(), aR._meshDebugName.data(), passStr);
#else
                _renderDoc.Annotate_Draw(aCmd, nullptr, nullptr, passStr);
#endif
            }
#endif
            vkCmdDrawIndexed(aCmd, aR._indexCount, 1, aR._firstIndex, 0, 0);
            _stats._totalDrawCallCount++;
            _stats._triCount += aR._indexCount / 3;
        };
	    {
            MOMO_VK_SCOPED_CMD_LABEL(aCmd, "Draw Opaque");
	        for (auto& r : opaque_draws) 
	        {
                const auto& mesh = _mainDrawContext._opaqueSurfaces[r];
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
                MOMO_VK_SCOPED_CMD_LABEL(aCmd, mesh._combinedDebugLabel);
#endif
                draw(mesh);
                _stats._opaqueDrawCallCount++;
	        }
	    }
	    {
            MOMO_VK_SCOPED_CMD_LABEL(aCmd, "Draw Transparent");
            // momo_vkDebug::VK_SCOPED_CMD_LABEL label(aCmd, "Draw Transparent");
	        for (auto& r : transparent_draws) 
	        {
                const auto& mesh = _mainDrawContext._transparentSurfaces[r];
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
                MOMO_VK_SCOPED_CMD_LABEL(aCmd, mesh._combinedDebugLabel);
#endif
                draw(mesh);
                _stats._transparentDrawCallCount++;
	        }
	    }
    }
    
	vkCmdEndRendering(aCmd);
	
	auto end = std::chrono::system_clock::now();
	//convert to microseconds (integer), and then come back to miliseconds
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	_stats._meshDrawTime = elapsed.count() / 1000.f;
}

AllocatedBuffer VulkanEngine::Create_Buffer(const size_t anAllocSize, const VkBufferUsageFlags aUsage, const VmaMemoryUsage aMemoryUsage, const char* aName) const
{
	// allocate buffer
	VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferInfo.pNext = nullptr;
	bufferInfo.size = anAllocSize;

	bufferInfo.usage = aUsage;

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = aMemoryUsage;
    // VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT was added later to make VMA_MEMORY_USAGE_AUTO work.
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT; 
	AllocatedBuffer newBuffer;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo, &newBuffer._buffer, &newBuffer._allocation,
		&newBuffer._info));

#ifdef MOMOVK_ENABLE_DEBUG_NAMES
	const std::string buffName = fmt::format("_Buffer {}, {}", Get_Buffer_Usage_Flag_String(aUsage), aName);
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_BUFFER, newBuffer._buffer, buffName);
    vmaSetAllocationName(_allocator, newBuffer._allocation, buffName.c_str());
#endif
    return newBuffer;
}

void VulkanEngine::Destroy_Buffer(const AllocatedBuffer& aBuffer) const
{
	vmaDestroyBuffer(_allocator, aBuffer._buffer, aBuffer._allocation);
}

void VulkanEngine::Resize_Swapchain()
{
	vkDeviceWaitIdle(_device);

	Destroy_Swapchain();

	int w, h;
	SDL_GetWindowSize(_window, &w, &h);
	_windowExtent.width = w;
	_windowExtent.height = h;

	Create_Swapchain(_windowExtent.width, _windowExtent.height);

	_resizeRequested = false;
}

void VulkanEngine::Update_Scene()
{
    PROFILE_SCOPE_N("Update_Scene")
	const auto start = std::chrono::system_clock::now();

	_mainDrawContext._opaqueSurfaces.clear();
    _mainDrawContext._transparentSurfaces.clear();

	// _loadedNodes["Suzanne"]->Draw(glm::mat4{ 1.f }, _mainDrawContext);

	// for (int x = -3; x < 3; x++) {
	
		// glm::mat4 scale = glm::scale(glm::vec3{ 0.2f });
		// glm::mat4 translation = glm::translate(glm::vec3{ x, 1, 0 });
	
		// _loadedNodes["Cube"]->Draw(translation * scale, _mainDrawContext);
	// }
	_loadedScenes["structure"]->Draw(glm::mat4{ 1.f }, _mainDrawContext);

	_mainCamera.Update();
	const glm::mat4 view = _mainCamera.GetViewMatrix();
	// camera projection
    const glm::mat4 projection = _mainCamera.GetProjectionMatrix(static_cast<float>(_windowExtent.width), static_cast<float>(_windowExtent.height));

	_sceneData._view = view;
	_sceneData._proj = projection;
	_sceneData._viewProj = projection * view;

	//some default lighting parameters
	_sceneData._ambientColor = _tempAmbientColor;
	_sceneData._sunlightColor = _tempSunColor;
    _sceneData._sunlightDirection = _tempSunDir;

	const auto end = std::chrono::system_clock::now();
	//convert to microseconds (integer), and then come back to miliseconds
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	_stats._sceneUpdateTime = elapsed.count() / 1000.f;
}

void VulkanEngine::ProcessEvents(bool& aQuit)
{
    PROFILE_SCOPE_N("ProcessEvents")
    SDL_Event e;

    // Handle events on queue
    while (SDL_PollEvent(&e) != 0)
    {
        // send SDL event to imgui for handling
        ImGui_ImplSDL2_ProcessEvent(&e);
        Input::Instance().ProcessEvent(e);

		// close the window when user alt-f4s or clicks the X button
        if (e.type == SDL_QUIT)
        {
            aQuit = true;
        }

        if (e.type == SDL_WINDOWEVENT)
        {
            switch (e.window.event)
            {
            case SDL_WINDOWEVENT_MINIMIZED:
                _freezeRendering = true;
                break;
            case SDL_WINDOWEVENT_RESTORED:
                _freezeRendering = false;
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                _resizeRequested = true;
                break;
            default:
                break;
            }
        }
    }
}

bool VulkanEngine::Is_Visible(const RenderObject& aObj, const glm::mat4& aViewProj)
{
	// TODO.
	// This is just one of the multiple possible functions we could be using for frustum culling.The way this works is that we are transforming each of the 8 corners of the mesh - space bounding box into screenspace, using object matrix and view - projection matrix.For those, we find the screen - space box bounds, and we check if that box is inside the clip - space view.This way of calculating bounds is on the slow side compared to other formulas, and can have false - positives where it things objects are visible when they arent. All the functions have different tradeoffs, and this one was selected for code simplicity and parallels with the functions we are doing on the vertex shaders.

	const glm::mat4 matrix = aViewProj * aObj._transform;
 
    const glm::vec4 row0 = {matrix[0][0], matrix[1][0], matrix[2][0], matrix[3][0]};
    const glm::vec4 row1 = {matrix[0][1], matrix[1][1], matrix[2][1], matrix[3][1]};
    const glm::vec4 row2 = {matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2]};
    const glm::vec4 row3 = {matrix[0][3], matrix[1][3], matrix[2][3], matrix[3][3]};
 
    const std::array planes = {
        row3 + row0, // Left
        row3 - row0, // Right
        row3 + row1, // Bottom
        row3 - row1, // Top
        row2, // Near
        row3 - row2 // Far (reversed Z, so far is where Z=0 in clip space)
    };
 
    for (int i = 0; i < 6; i++)
    {
        const float d = glm::dot(glm::vec3(planes[i]), aObj._bounds._origin) + planes[i].w;
        const float r = glm::dot(glm::abs(glm::vec3(planes[i])), aObj._bounds._extents);
 
        if (d < -r)
        {
            return false;
        }
    }
 
    return true;
}

const char* VulkanEngine::Get_Device_Type_String(const VkPhysicalDeviceType aType)
{
    switch (aType)
    {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        return "Other";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU";
    case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM:
    default:
        return "Unknown";
    }
}

std::string VulkanEngine::Get_Buffer_Usage_Flag_String(const VkBufferUsageFlags aUsageFlag)
{
    std::string typeName = {};
    
    // Standard Data Buffers
    if (aUsageFlag & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        typeName += "Index_";
    if (aUsageFlag & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        typeName += "Vertex_";
    if (aUsageFlag & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        typeName += "Uniform_";
    if (aUsageFlag & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        typeName += "Storage_";

    // Transfer Buffers
    if (aUsageFlag & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
        typeName += "TransferSrc(Staging)_";
    if (aUsageFlag & VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        typeName += "TransferDst_";

    // Texel Buffers
    if (aUsageFlag & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)
        typeName += "UniformTexel_";
    if (aUsageFlag & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
        typeName += "StorageTexel_";

    // GPU-Driven Rendering
    if (aUsageFlag & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
        typeName += "Indirect_";

    // Modern / Bindless / Ray Tracing
    if (aUsageFlag & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        typeName += "DeviceAddress_";
    if (aUsageFlag & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR)
        typeName += "AccelStruct_";
    if (aUsageFlag & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR)
        typeName += "SBT_";

    // Clean up the trailing underscore, or handle the case where no known flags were passed
    if (!typeName.empty())
    {
        typeName.pop_back(); // Removes the last '_'
    }
    else
    {
        typeName = "Unknown";
    }
    return typeName;
}

GPUMeshBuffers VulkanEngine::UploadMesh(const std::span<uint32_t> aIndices, const std::span<Vertex> aVertices, const char* aMeshName) const
{
	const size_t vertexBufferSize = aVertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = aIndices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	// create vertex buffer
	// It's not necessary for meshes to use GPU_ONLY vertex buffers, but it's highly recommended unless it's something like a CPU side particle system or other dynamic effects.

	const char* bufferName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string bufferNameString = fmt::format("(Vertex, BDA), {}", aMeshName);
    bufferName = bufferNameString.c_str();
#endif
    newSurface._vertexBuffer = Create_Buffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_AUTO, bufferName); // was VMA_MEMORY_USAGE_GPU_ONLY

	//find the address of the vertex buffer
	const VkBufferDeviceAddressInfo deviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .pNext = nullptr, .buffer = newSurface._vertexBuffer._buffer};
	newSurface._vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    bufferNameString = fmt::format("{}", aMeshName);
    bufferName = bufferNameString.c_str();
#endif
	//create index buffer, was previously VMA_MEMORY_USAGE_CPU_ONLY
    newSurface._indexBuffer = Create_Buffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO, bufferName);

#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    bufferNameString = fmt::format("{}", aMeshName);
    bufferName = bufferNameString.c_str();
#endif
    // staging buffer is 1 buffer for both copies to index and vertex buffers.
    const AllocatedBuffer staging = Create_Buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, bufferName);

	// Buffer is already mapped. You can access its memory.
	memcpy(staging._info.pMappedData, aVertices.data(), vertexBufferSize);
    memcpy(static_cast<char*>(staging._info.pMappedData) + vertexBufferSize, aIndices.data(), indexBufferSize);

	// void* data = staging.allocation->GetMappedData(); // doing this gives us the address so we can write to it. not a copy but just pointing to the staging buffer.
	// // copy vertex buffer
	// memcpy(data, aVertices.data(), vertexBufferSize);
	// // copy index buffer
	// memcpy(static_cast<char*>(data) + vertexBufferSize, aIndices.data(), indexBufferSize);

	Immediate_Submit([&](const VkCommandBuffer aCmd)
    {
        VkBufferCopy vertexCopy;
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(aCmd, staging._buffer, newSurface._vertexBuffer._buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy;
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(aCmd, staging._buffer, newSurface._indexBuffer._buffer, 1, &indexCopy);
    });

	Destroy_Buffer(staging);

	return newSurface;
}
