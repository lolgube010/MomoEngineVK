#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_images.h>
#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#include <chrono>
#include <thread>

#define VMA_IMPLEMENTATION

#include "vk_mem_alloc.h"
#include "vk_pipelines.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>

// globals
namespace
{
	VulkanEngine* gl_LoadedEngine = nullptr;
}

constexpr bool bUseValidationLayers = true;
constexpr auto AppName = "MomoVK";

void GLTFMetallic_Roughness::Build_Pipelines(VulkanEngine* aEngine)
{
	VkResult loadShaderResult = {};

	VkShaderModule meshFragShader;
	{
		const std::string shaderPath = momo_util::BuildShaderPath("mesh", momo_util::ShaderType::Fragment, false);
		if (!vkUtil::LoadShaderModule(shaderPath.c_str(), aEngine->_device, &meshFragShader, loadShaderResult))
		{
			fmt::print("Error when building the compute shader {}\n", static_cast<int>(loadShaderResult));
		}
		else
		{
			fmt::print("Fragment shader successfully loaded. PATH: {}\n", shaderPath);
		}
	}

	VkShaderModule meshVertexShader;
	{
		const std::string shaderPath = momo_util::BuildShaderPath("mesh", momo_util::ShaderType::Vertex, false);
		if (!vkUtil::LoadShaderModule(shaderPath.c_str(), aEngine->_device, &meshVertexShader, loadShaderResult))
		{
			fmt::print("Error when building the compute shader {}\n", static_cast<int>(loadShaderResult));
		}
		else
		{
			fmt::print("Vertex shader successfully loaded. PATH: {}\n", shaderPath);
		}
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.Add_Binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.Add_Binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.Add_Binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	materialLayout = layoutBuilder.Build(aEngine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { aEngine->_gpuSceneDataDescriptorLayout, materialLayout };

	VkPipelineLayoutCreateInfo mesh_layout_info = vkInit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 2;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(aEngine->_device, &mesh_layout_info, nullptr, &newLayout));

	opaquePipeline.layout = newLayout;
	transparentPipeline.layout = newLayout;

	// build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.Set_Shaders(meshVertexShader, meshFragShader);
	pipelineBuilder.Set_Input_Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.Set_Polygon_Mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.Set_Cull_Mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.Set_Multisampling_None();
	pipelineBuilder.Disable_Blending();
	pipelineBuilder.Enable_DepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//render format
	pipelineBuilder.Set_Color_Attachment_Format(aEngine->_drawImage.imageFormat);
	pipelineBuilder.Set_Depth_Format(aEngine->_depthImage.imageFormat);

	// use the triangle layout we created
	pipelineBuilder._pipelineLayout = newLayout;

	// finally build the pipeline
	opaquePipeline.pipeline = pipelineBuilder.Build_Pipeline(aEngine->_device);

	// create the transparent variant
	pipelineBuilder.Enable_Blending_Additive();

	pipelineBuilder.Enable_DepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparentPipeline.pipeline = pipelineBuilder.Build_Pipeline(aEngine->_device);

	vkDestroyShaderModule(aEngine->_device, meshFragShader, nullptr);
	vkDestroyShaderModule(aEngine->_device, meshVertexShader, nullptr);
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device) const
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);

	vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
	vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::Write_Material(const VkDevice aDevice, const MaterialPass aPass, const MaterialResources& aResources, DescriptorAllocatorGrowable& aDescriptorAllocator)
{
	MaterialInstance matData;
	matData.passType = aPass;
	if (aPass == MaterialPass::Transparent) 
	{
		matData.pipeline = &transparentPipeline;
	}
	else 
	{
		matData.pipeline = &opaquePipeline;
	}

	matData.materialSet = aDescriptorAllocator.Allocate(aDevice, materialLayout);


	writer.Clear();
	writer.Write_Buffer(0, aResources.dataBuffer, sizeof(MaterialConstants), aResources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.Write_Image(1, aResources.colorImage.imageView, aResources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.Write_Image(2, aResources.metalRoughImage.imageView, aResources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.Update_Set(aDevice, matData.materialSet);

	return matData;
}

void MeshNode::Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx)
{
	const glm::mat4 nodeMatrix = aTopMatrix * worldTransform;

	for (const auto& s : mesh->surfaces) // a mesh can have multiple surfaces with different materials.
	{
		RenderObject def;
		def.indexCount = s.count;
		def.firstIndex = s.startIndex;
		def.indexBuffer = mesh->meshBuffers._indexBuffer.buffer;
		def.material = &s.material->data;
		def.bounds = s.bounds;
		def.transform = nodeMatrix;
		def.vertexBufferAddress = mesh->meshBuffers._vertexBufferAddress;

		switch (s.material->data.passType)
		{
		case MaterialPass::MainColor:
			aCtx.opaqueSurfaces.push_back(def);
			break;
		case MaterialPass::Transparent:
			aCtx.transparentSurfaces.push_back(def);
			break;
		case MaterialPass::Other:
			throw;
		}
	}

	// recurse down
	Node::Draw(aTopMatrix, aCtx);
}

VulkanEngine& VulkanEngine::Get()
{
	return *gl_LoadedEngine;
}

void VulkanEngine::Init()
{
	// only one engine initialization is allowed with the application.
	assert(gl_LoadedEngine == nullptr);
	gl_LoadedEngine = this;

	// We initialize SDL and create a window with it.
	SDL_Init(SDL_INIT_VIDEO);

	constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

	_window = SDL_CreateWindow(
		AppName,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);
	
	Init_Vulkan();
	Init_Swapchain();
	Init_Commands();
	Init_Sync_Structures();
	Init_Descriptors();
	Init_Pipelines();
	Init_Imgui();
	Init_Tracy();
	Init_Default_Data();

	_is_initialized = true;
}

void VulkanEngine::Draw()
{
	Update_Scene();

	//> draw_1
	// wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &Get_Current_Frame()._renderFence, true, 1000000000));
	Get_Current_Frame()._deletionQueue.Flush();
	Get_Current_Frame()._frameDescriptors.Clear_Pools(_device);
	VK_CHECK(vkResetFences(_device, 1, &Get_Current_Frame()._renderFence));
	//< draw_1

	//> draw_2
	// request image from the swapchain
	uint32_t swapchainImageIndex;
	if (const VkResult res = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, Get_Current_Frame()._swapchainSemaphore, nullptr, &swapchainImageIndex); 
		res == VK_ERROR_OUT_OF_DATE_KHR) 
	{
		_resize_requested = true;
		return;
	}
	//< draw_2

	//> draw_3
	// naming it cmd for shorter writing
	const VkCommandBuffer cmd = Get_Current_Frame()._mainCommandBuffer;

	// now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	const VkCommandBufferBeginInfo cmdBeginInfo = vkInit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	_drawExtent.height = static_cast<uint32_t>(static_cast<float>(std::min(_swapchain_extent.height, _drawImage.imageExtent.height)) *
		_renderScale);
	_drawExtent.width = static_cast<uint32_t>(static_cast<float>(std::min(_swapchain_extent.width, _drawImage.imageExtent.width)) *
		_renderScale);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// transition our main draw image into general layout so we can write into it.
	// we will overwrite it all so we don't care about what was the older layout
	vkUtil::Transition_Image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	Draw_Background(cmd);

	vkUtil::Transition_Image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkUtil::Transition_Image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	{
		PROFILE_SCOPE_N("Draw Geometry")
		Draw_Geometry(cmd);
	}

	//transition the draw image and the swapchain image into their correct transfer layouts
	vkUtil::Transition_Image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkUtil::Transition_Image(cmd, _swapchain_images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// execute a copy from the draw image into the swapchain
	vkUtil::copy_image_to_image(cmd, _drawImage.image, _swapchain_images[swapchainImageIndex], _drawExtent, _swapchain_extent);

	// set swapchain image layout to Attachment Optimal so we can draw it
	vkUtil::Transition_Image(cmd, _swapchain_images[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// draw imgui into the swapchain image
	Draw_Imgui(cmd, _swapchain_image_views[swapchainImageIndex]);

	// set swapchain image layout to Present so we can show it on the screen
	vkUtil::Transition_Image(cmd, _swapchain_images[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);


	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//< draw_4

	//> draw_5
	// prepare the submission to the queue. 
	// we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	// we will signal the _renderSemaphore, to signal that rendering has finished

	const VkCommandBufferSubmitInfo cmdInfo = vkInit::command_buffer_submit_info(cmd);

	const VkSemaphoreSubmitInfo waitInfo = vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, Get_Current_Frame()._swapchainSemaphore);
	//VkSemaphoreSubmitInfo signalInfo = vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, Get_Current_Frame()._renderSemaphore);
	const VkSemaphoreSubmitInfo signalInfo = vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, ready_for_present_semaphores[swapchainImageIndex]);

	const VkSubmitInfo2 submit = vkInit::submit_info(&cmdInfo, &signalInfo, &waitInfo);

	// submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, Get_Current_Frame()._renderFence));
	//< draw_5

	//> draw_6
	// prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	//presentInfo.pWaitSemaphores = &Get_Current_Frame()._renderSemaphore;
	presentInfo.pWaitSemaphores = &ready_for_present_semaphores[swapchainImageIndex];
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	if (const VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo); 
		presentResult == VK_ERROR_OUT_OF_DATE_KHR) 
	{
		_resize_requested = true;
	}

	//increase the number of frames drawn
	_frame_number++;
	//< draw_6

}

void VulkanEngine::Run()
{
	SDL_Event e;
	bool bQuit = false;

	// main loop
	while (!bQuit)
	{
		auto start = std::chrono::system_clock::now();

		// Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			// close the window when user alt-f4s or clicks the X button
			if (e.type == SDL_QUIT)
			{
				bQuit = true;
			}

			if (e.type == SDL_WINDOWEVENT)
			{
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
				{
					_stop_rendering = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED)
				{
					_stop_rendering = false;
				}
			}

			// putting other input here cuz i'm lazy
			const auto& key = e.key.keysym.sym;
			if (e.type == SDL_KEYDOWN && key == SDLK_CAPSLOCK && e.key.repeat == 0)
			{
				const auto enabled = SDL_GetRelativeMouseMode();
				fmt::print("caps locked presssed, window is currently: {}\n", static_cast<bool>(enabled));
				SDL_SetRelativeMouseMode(static_cast<SDL_bool>(!enabled));
			
			}

			_mainCamera.ProcessSDLEvent(e);
			//send SDL event to imgui for handling
			ImGui_ImplSDL2_ProcessEvent(&e);
			//process_input(e);
		}

		// do not draw if we are minimized
		if (_stop_rendering)
		{
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (_resize_requested)
		{
			Resize_Swapchain();
		}

		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		////some imgui UI to test
		//ImGui::ShowDemoWindow();

		Imgui_Run();

		//make imgui calculate internal draw structures
		ImGui::Render();

		Draw();
		PROFILE_FRAME;

		auto end = std::chrono::system_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		_stats.frameTime = elapsed.count() / 1000.f;
	}
}

void VulkanEngine::Cleanup()
{
	if (_is_initialized)
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
		for (const auto& ready_For_Present_Semaphore : ready_for_present_semaphores)
		{
			vkDestroySemaphore(_device, ready_For_Present_Semaphore, nullptr);
		}
		if (_tracyVkCtx)
		{
			TracyVkDestroy(_tracyVkCtx);
			_tracyVkCtx = nullptr;
		}
		// for (const auto& mesh : _testMeshes) 
		// {
		// 	Destroy_Buffer(mesh->meshBuffers._indexBuffer);
		// 	Destroy_Buffer(mesh->meshBuffers._vertexBuffer);
		// }

		metalRoughMaterial.clear_resources(_device);
		_mainDeletionQueue.Flush();

		Destroy_Swapchain();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);

		vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
	gl_LoadedEngine = nullptr;
}

void VulkanEngine::Draw_Imgui(const VkCommandBuffer aCmd, const VkImageView aTargetImageView) const
{
	const VkRenderingAttachmentInfo colorAttachment = vkInit::attachment_info(aTargetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingInfo renderInfo = vkInit::rendering_info(_swapchain_extent, &colorAttachment, nullptr);

	vkCmdBeginRendering(aCmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), aCmd);

	vkCmdEndRendering(aCmd);
}

void VulkanEngine::Immediate_Submit(std::function<void(VkCommandBuffer cmd)>&& aFunction) const
{
	VK_CHECK(vkResetFences(_device, 1, &_immFence));
	VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

	const VkCommandBuffer cmd = _immCommandBuffer;

	const VkCommandBufferBeginInfo cmdBeginInfo = vkInit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	aFunction(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	const VkCommandBufferSubmitInfo cmdInfo = vkInit::command_buffer_submit_info(cmd);
	const VkSubmitInfo2 submit = vkInit::submit_info(&cmdInfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

	VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void VulkanEngine::SetDebugInfo(const uint64_t aObjectHandle, const VkObjectType aObjectType, const char* a_pObjectName) const
{
	if (_vkSetDebugUtilsObjectNameEXT)
	{
		VkDebugUtilsObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = aObjectType;
		nameInfo.objectHandle = aObjectHandle;
		nameInfo.pObjectName = a_pObjectName;

		_vkSetDebugUtilsObjectNameEXT(_device, &nameInfo);
	}
}

AllocatedImage VulkanEngine::Create_Image(const VkExtent3D aSize, const VkFormat aFormat, const VkImageUsageFlags aUsage, const bool aMipmapped) const
{
	AllocatedImage newImage;
	newImage.imageFormat = aFormat;
	newImage.imageExtent = aSize;

	VkImageCreateInfo img_Info = vkInit::image_create_info(aFormat, aUsage, aSize);
	if (aMipmapped) 
	{
		img_Info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(aSize.width, aSize.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &img_Info, &allocInfo, &newImage.image, &newImage.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (aFormat == VK_FORMAT_D32_SFLOAT) 
	{
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build an image-view for the image
	VkImageViewCreateInfo view_Info = vkInit::imageview_create_info(aFormat, newImage.image, aspectFlag);
	view_Info.subresourceRange.levelCount = img_Info.mipLevels;

	VK_CHECK(vkCreateImageView(_device, &view_Info, nullptr, &newImage.imageView));

	return newImage;

}

AllocatedImage VulkanEngine::Create_Image(const void* aData, const VkExtent3D aSize, const VkFormat aFormat, const VkImageUsageFlags aUsage, const bool aMipmapped) const
{
	const size_t data_Size = static_cast<size_t>(aSize.depth) * aSize.width * aSize.height * 4;
	const AllocatedBuffer uploadBuffer = Create_Buffer(data_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	memcpy(uploadBuffer.info.pMappedData, aData, data_Size);

	const AllocatedImage new_Image = Create_Image(aSize, aFormat, aUsage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, aMipmapped);

	Immediate_Submit([&](const VkCommandBuffer aCmd) 
	{
		vkUtil::Transition_Image(aCmd, new_Image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = aSize;

		// copy the buffer into the image
		vkCmdCopyBufferToImage(aCmd, uploadBuffer.buffer, new_Image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		if (aMipmapped)
		{
			vkUtil::generate_mipmaps(aCmd, new_Image.image, VkExtent2D{ new_Image.imageExtent.width,new_Image.imageExtent.height });
		}
		else
		{
			vkUtil::Transition_Image(aCmd, new_Image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); 
		}
	});

	Destroy_Buffer(uploadBuffer);
	return new_Image;
}

void VulkanEngine::Destroy_Image(const AllocatedImage& aImg) const
{
	vkDestroyImageView(_device, aImg.imageView, nullptr);
	vmaDestroyImage(_allocator, aImg.image, aImg.allocation);
}

void VulkanEngine::ProcessInput(SDL_Event& anE)
{
	auto& e = anE;
	switch (e.type)
	{
	// ------------------- KEYBOARD -------------------
	case SDL_KEYDOWN:
		if (!e.key.repeat)
		{
			switch (e.key.keysym.sym)
			{
			case SDLK_w: fmt::print("W pressed\n");
				break;
			case SDLK_s: fmt::print("S pressed\n");
				break;
			case SDLK_a: fmt::print("A pressed\n");
				break;
			case SDLK_d: fmt::print("D pressed\n");
				break;
			case SDLK_LEFT: fmt::print("Left arrow\n");
				break;
			case SDLK_RIGHT: fmt::print("Right arrow\n");
				break;
			case SDLK_UP: fmt::print("Up arrow\n");
				break;
			case SDLK_DOWN: fmt::print("Down arrow\n");
				break;
			case SDLK_SPACE: fmt::print("Space pressed\n");
				break;
				// Add more keys as needed
			}
		}
		break;

	case SDL_KEYUP:
		switch (e.key.keysym.sym)
		{
		case SDLK_w: fmt::print("W released\n");
			break;
		case SDLK_s: fmt::print("S released\n");
			break;
		case SDLK_a: fmt::print("A released\n");
			break;
		case SDLK_d: fmt::print("D released\n");
			break;
		case SDLK_LEFT: fmt::print("Left arrow up\n");
			break;
		case SDLK_RIGHT: fmt::print("Right arrow up\n");
			break;
		case SDLK_UP: fmt::print("Up arrow up\n");
			break;
		case SDLK_DOWN: fmt::print("Down arrow up\n");
			break;
		case SDLK_SPACE: fmt::print("Space released\n");
			break;
		}
		break;

	// ------------------- MOUSE MOTION -------------------
	case SDL_MOUSEMOTION:
		fmt::print("Mouse at: ({}, {})\n",
		           e.motion.x,
		           e.motion.y);
		// e.motion.xrel, e.motion.yrel for relative movement
		break;

	// ------------------- MOUSE BUTTONS -------------------
	case SDL_MOUSEBUTTONDOWN:
		if (e.button.button == SDL_BUTTON_LEFT)
		{
			fmt::print("Left click DOWN at ({}, {})\n",
			           e.button.x,
			           e.button.y);
		}
		else if (e.button.button == SDL_BUTTON_RIGHT)
		{
			fmt::print("Right click DOWN at ({}, {})\n",
			           e.button.x,
			           e.button.y);
		}
		else if (e.button.button == SDL_BUTTON_MIDDLE)
		{
			fmt::print("Middle click DOWN\n");
		}
		break;

	case SDL_MOUSEBUTTONUP:
		if (e.button.button == SDL_BUTTON_LEFT)
		{
			fmt::print("Left click UP\n");
		}
		else if (e.button.button == SDL_BUTTON_RIGHT)
		{
			fmt::print("Right click UP\n");
		}
		else if (e.button.button == SDL_BUTTON_MIDDLE)
		{
			fmt::print("Middle click UP\n");
		}
		break;

	// ------------------- MOUSE WHEEL -------------------
	case SDL_MOUSEWHEEL:
		fmt::print("Mouse wheel: x={} y={}\n",
		           e.wheel.x,
		           e.wheel.y);
		break;
	}
}

void VulkanEngine::Init_Vulkan()
{
	//> init_instance
	vkb::InstanceBuilder builder;

	//make the vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name(AppName)
	                       .request_validation_layers(bUseValidationLayers)
	                       .use_default_debug_messenger()
	                       .require_api_version(1, 3, 0)
	                       .build();

	vkb::Instance vkb_inst = inst_ret.value();

	// grab the instance 
	_instance = vkb_inst.instance;
	_debug_messenger = vkb_inst.debug_messenger;
	//< init_instance

	//> init_device
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	// vk 1.4 features
	// VkPhysicalDeviceVulkan14Features features14{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
	
	// vk 1.3 features
	VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

	// vk 1.2 features
	VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

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

	// NOTE FOR FUTURE ME: please check if an extension is actually available. tried adding a debug one only for it to only have drivers on NVIDIA and not AMD. 
	// make sure to check before adding random extensions!!!

	if (!phys_ret)
	{
		throw std::runtime_error("failed to find a suitable GPU: " + phys_ret.error().message());
	}

	const vkb::PhysicalDevice& physicalDevice = phys_ret.value();
	//create the final vulkan device (driver) from the physical device (gpu)
	vkb::DeviceBuilder deviceBuilder{physicalDevice};

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a vulkan application
	_device = vkbDevice.device;
	_chosen_GPU = physicalDevice.physical_device;
	//< init device

	//> init_queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
	//< init_queue

	// momo debug stuff
	g_TotalAllocatedBytes = 0;
	g_TotalFreedBytes = 0;
	g_AllocationCount = 0;
	
	_callbacks.pUserData = nullptr;
	_callbacks.pfnAllocate = MyAllocateCallback;
	_callbacks.pfnFree = MyFreeCallback;

	//> init vma
	// initialize the memory allocator
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _chosen_GPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	// allocatorInfo.pDeviceMemoryCallbacks = &_callbacks; // added by momo
	
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	_mainDeletionQueue.Push_Function([&]
	{
		vmaDestroyAllocator(_allocator);
	});
	//< init vma
	
	// momo debug adventure
	_vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(_instance, "vkSetDebugUtilsObjectNameEXT"));
}

void VulkanEngine::Init_Swapchain()
{
	Create_Swapchain(_windowExtent.width, _windowExtent.height);

	//> create image (fullscreen render target/render image)
	//draw image size will match the window
	const VkExtent3D drawImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	// hardcoding the draw format to 32-bit float 
	// this is set as 16 in the guide and gives validation errors unless it is // momo comment
	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_drawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const VkImageCreateInfo rimg_info = vkInit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

	// build an image-view for the draw image to use for rendering
	const VkImageViewCreateInfo rview_info = vkInit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));
	SetDebugInfo((uint64_t)_drawImage.image, VK_OBJECT_TYPE_IMAGE, "OOGILI BOOGILI ZOOGILI SHMALOOGILI");
	//< create image

	//> create depth
	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	const VkImageCreateInfo dimg_info = vkInit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);

	//allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

	//build an image-view for the draw image to use for rendering
	const VkImageViewCreateInfo dview_info = vkInit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));
	SetDebugInfo((uint64_t)_depthImage.image, VK_OBJECT_TYPE_IMAGE, "(gabagool) main depth");
	//< create depth


	//add to deletion queues
	_mainDeletionQueue.Push_Function([=]
	{
		// main img
		vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
		
		// depth img
		vkDestroyImageView(_device, _depthImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
	});
}

void VulkanEngine::Init_Commands()
{
	// create a command pool for commands submitted to the graphics queue. 
	// we also want the pool to allow for resetting of individual command buffers
	const VkCommandPoolCreateInfo commandPoolInfo = vkInit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (auto& frame : _frames)
	{
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &frame._commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkInit::command_buffer_allocate_info(frame._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &frame._mainCommandBuffer));
	}

	// for imgui
	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

	// allocate the command buffer for immediate submits
	const VkCommandBufferAllocateInfo cmdAllocInfo = vkInit::command_buffer_allocate_info(_immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

	_mainDeletionQueue.Push_Function([=]
	{
		vkDestroyCommandPool(_device, _immCommandPool, nullptr);
	});
}

void VulkanEngine::Init_Sync_Structures()
{
	// create synchronization structures
	// one fence to control when the gpu has finished rendering the frame, and 2 semaphores to synchronize rendering with swapchain
	// we want the fence to start signalled so we can wait on it on the first frame

	const VkFenceCreateInfo fenceCreateInfo = vkInit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	const VkSemaphoreCreateInfo semaphoreCreateInfo = vkInit::semaphore_create_info();

	for (auto& frame : _frames)
	{
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &frame._renderFence));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &frame._swapchainSemaphore));
		//VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &frame._renderSemaphore));
	}
	ready_for_present_semaphores.resize(_swapchain_images.size());
	for (int i = 0; i < _swapchain_images.size(); ++i)
	{
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &ready_for_present_semaphores[i]));
	}

	// for imgui
	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
	_mainDeletionQueue.Push_Function([=] { vkDestroyFence(_device, _immFence, nullptr); });
}

void VulkanEngine::Init_Descriptors()
{
	//create a descriptor pool that will hold 10 sets with 1 image each
	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
	{
		{._type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ._ratio = 1 },
		{._type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ._ratio = 1 },
		{._type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ._ratio = 1 }
	};

	_globalDescriptorAllocator.Init(_device, 10, sizes);

	// for our compute draw
	{
		DescriptorLayoutBuilder builder;
		builder.Add_Binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		_drawImageDescriptorLayout = builder.Build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}
	// for textures
	{
		// TODO:
		// When we do drawing, we want to use the fixed hardware in the GPU for accessing texture data, which needs the sampler.We have the option to either use VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, which packages an image and a sampler to use with that image, or to use 2 descriptors, and separate the two into VK_DESCRIPTOR_TYPE_SAMPLER and VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE.According to gpu vendors, the separated approach can be faster as there is less duplicated data.But its a bit harder to deal with so we won't be doing it for now.Instead, we will use the combined descriptor to make our shaders simpler.

		DescriptorLayoutBuilder builder;
		builder.Add_Binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_singleImageDescriptorLayout = builder.Build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	// for our draw image
	{
		DescriptorLayoutBuilder builder;
		builder.Add_Binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		_gpuSceneDataDescriptorLayout = builder.Build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	_drawImageDescriptors = _globalDescriptorAllocator.Allocate(_device, _drawImageDescriptorLayout);
	{
		DescriptorWriter writer;
		writer.Write_Image(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.Update_Set(_device, _drawImageDescriptors);
		
	}
	
	//make sure both the descriptor allocator and the new layout get cleaned up properly
	_mainDeletionQueue.Push_Function([&]
	{
		_globalDescriptorAllocator.Destroy_Pools(_device);

		vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _singleImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
	});

	for (unsigned int i = 0; i < FRAME_OVERLAP; i++)   // NOLINT(modernize-loop-convert)
	{
		// create a descriptor pool
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_Sizes = 
		{
			{._type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ._ratio = 3 },
			{._type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ._ratio = 3 },
			{._type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ._ratio = 3 },
			{._type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ._ratio = 4 },
		};

		_frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
		_frames[i]._frameDescriptors.Init(_device, 1000, frame_Sizes);

		_mainDeletionQueue.Push_Function([&, i]
		{
			_frames[i]._frameDescriptors.Destroy_Pools(_device);
		});
	}
}

void VulkanEngine::Init_Pipelines()
{
	// compute pipelines
	Init_Background_Pipelines();

	// graphics pipelines
	Init_Mesh_Pipeline();

	metalRoughMaterial.Build_Pipelines(this);
}

void VulkanEngine::Init_Background_Pipelines()
{
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(ComputePushConstants);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

	VkResult loadShaderResult = {};

	VkShaderModule gradientShader;
	{
		const std::string gradientShaderPath = momo_util::BuildShaderPath("gradient_color", momo_util::ShaderType::Compute, false);
		if (!vkUtil::LoadShaderModule(gradientShaderPath.c_str(), _device, &gradientShader, loadShaderResult))
		{
			fmt::print("Error when building the compute shader {}\n", static_cast<int>(loadShaderResult));
		}
		else
		{
			fmt::print("Compute shader successfully loaded. PATH: {}\n", gradientShaderPath);
		}
	}

	VkShaderModule skyShader;
	{
		const std::string skyShaderPath = momo_util::BuildShaderPath("sky", momo_util::ShaderType::Compute, false);
		if (!vkUtil::LoadShaderModule(skyShaderPath.c_str(), _device, &skyShader, loadShaderResult))
		{
			fmt::print("Error when building the compute shader {}\n", static_cast<int>(loadShaderResult));
		}
		else
		{
			fmt::print("Compute shader successfully loaded. PATH: {}\n", skyShaderPath);
		}
	}

	VkPipelineShaderStageCreateInfo stageInfo{};
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.pNext = nullptr;
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = gradientShader;
	stageInfo.pName = "main"; // this option gives you the ability to have multiple shaders in the same file, having different entry points.

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = _gradientPipelineLayout;
	computePipelineCreateInfo.stage = stageInfo;

	ComputeEffect gradient{};
	gradient.layout = _gradientPipelineLayout;
	gradient.name = "gradient";
	gradient.data = {};

	//default colors
	gradient.data.data1 = glm::vec4(1, 0, 0, 1);
	gradient.data.data2 = glm::vec4(0, 0, 1, 1);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));
	backgroundEffects.push_back(gradient);

	//change the shader module only to create the sky shader
	computePipelineCreateInfo.stage.module = skyShader;

	ComputeEffect sky{};
	sky.layout = _gradientPipelineLayout;
	sky.name = "sky";
	sky.data = {};
	//default sky parameters
	sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

	//add the 2 background effects into the array
	backgroundEffects.push_back(sky);

	//destroy structures properly
	vkDestroyShaderModule(_device, gradientShader, nullptr);
	vkDestroyShaderModule(_device, skyShader, nullptr);
	_mainDeletionQueue.Push_Function([=]
	{
		vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
		vkDestroyPipeline(_device, sky.pipeline, nullptr);
		vkDestroyPipeline(_device, gradient.pipeline, nullptr);
	});
}

void VulkanEngine::Init_Imgui()
{
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	const VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

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
	init_info.PhysicalDevice = _chosen_GPU;
	init_info.Device = _device;
	init_info.Queue = _graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchain_image_format;

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
	_mainDeletionQueue.Push_Function([=]
	{
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
	});
}

void VulkanEngine::Init_Tracy()
{
	// init tracy
#ifdef TRACY_ENABLE
	_tracyVkCtx = TracyVkContext(_chosen_GPU, _device, _graphicsQueue, Get_Current_Frame()._mainCommandBuffer)
		TracyVkContextName(_tracyVkCtx, "Main Graphics Queue", sizeof("Main Graphics Queue") - 1)
#endif
}

void VulkanEngine::Init_Default_Data()
{
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

	_mainCamera.velocity = glm::vec3(0.f);
	_mainCamera.position = glm::vec3(30.f, -00.f, -085.f);

	_mainCamera.pitch = 0;
	_mainCamera.yaw = 0;

	// In the file provided, index 0 is a cube, index 1 is a sphere, and index 2 is a blender monkey head. we will be drawing that last one, draw it right after drawing the rectangle from before
	// _testMeshes = LoadGltfMeshes_Legacy(this, R"(..\..\assets\basicmesh.glb)").value(); 
	// _testMeshes = LoadGltfMeshes(this, R"(..\..\assets\structure.glb)").value();
	// _testMeshes = LoadGltfMeshes(this, R"(..\..\assets\thejunkshopsplashscreen2.glb)").value();

	const uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = Create_Image(&white, VkExtent3D{1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
	const uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = Create_Image(&grey, VkExtent3D{1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
	const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	_blackImage = Create_Image(&black, VkExtent3D{1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);


	const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) 
	{
		for (int y = 0; y < 16; y++) 
		{
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_errorCheckerboardImage = Create_Image(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	VkSamplerCreateInfo sampler = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	// nearest gives pixelated look
	sampler.magFilter = VK_FILTER_NEAREST;
	sampler.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(_device, &sampler, nullptr, &_defaultSamplerNearest);

	// linear blurs
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_device, &sampler, nullptr, &_defaultSamplerLinear);
	

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
	//default the material textures
	materialResources.colorImage = _whiteImage;
	materialResources.colorSampler = _defaultSamplerLinear;
	materialResources.metalRoughImage = _whiteImage;
	materialResources.metalRoughSampler = _defaultSamplerLinear;

	//set the uniform buffer for the material data
	AllocatedBuffer materialConstants = Create_Buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//write the buffer
	GLTFMetallic_Roughness::MaterialConstants* sceneUniformData = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(materialConstants.allocation->GetMappedData());
	sceneUniformData->colorFactors = glm::vec4{ 1,1,1,1 };
	sceneUniformData->metal_rough_factors = glm::vec4{ 1,0.5,0,0 };

	_mainDeletionQueue.Push_Function([materialConstants, this]
	{
		Destroy_Buffer(materialConstants);
	});

	materialResources.dataBuffer = materialConstants.buffer;
	materialResources.dataBufferOffset = 0;

	defaultData = metalRoughMaterial.Write_Material(_device, MaterialPass::MainColor, materialResources, _globalDescriptorAllocator);

	// for (auto& m : _testMeshes) 
	// {
	// 	std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
	// 	newNode->mesh = m;
	//
	// 	newNode->localTransform = glm::mat4{ 1.f };
	// 	newNode->worldTransform = glm::mat4{ 1.f };
	//
	// 	for (auto& s : newNode->mesh->surfaces) 
	// 	{
	// 		s.material = std::make_shared<GLTFMaterial>(defaultData);
	// 	}
	//
	// 	_loadedNodes[m->name] = std::move(newNode);
	// }


	const std::string structurePath = { "..\\..\\assets\\structure.glb" };
	const auto structureFile = LoadGLTF(this, structurePath);
	assert(structureFile.has_value());

	_loadedScenes["structure"] = *structureFile;

	//>materials
}

void VulkanEngine::Init_Mesh_Pipeline()
{
	VkResult res = {};

	const auto fragPath = momo_util::BuildShaderPath("tex_image", momo_util::ShaderType::Fragment, false);
	VkShaderModule triangleFragShader;
	if (!vkUtil::LoadShaderModule(fragPath.c_str(), _device, &triangleFragShader, res))
	{
		fmt::print("Error when building the triangle fragment shader module: {}", static_cast<int>(res));
	}
	else
	{
		fmt::print("Frag shader successfully loaded. PATH: {}\n", fragPath);
	}

	const auto vertPath = momo_util::BuildShaderPath("colored_triangle_mesh", momo_util::ShaderType::Vertex, false);
	VkShaderModule triangleVertexShader;
	if (!vkUtil::LoadShaderModule(vertPath.c_str(), _device, &triangleVertexShader, res))
	{
		fmt::print("Error when building the triangle vertex shader module: {}", static_cast<int>(res));
	}
	else
	{
		fmt::print("Vert shader successfully loaded. PATH: {}\n", vertPath);
	}

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkInit::pipeline_layout_create_info();
	pipeline_layout_info.pPushConstantRanges = &bufferRange;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pSetLayouts = &_singleImageDescriptorLayout;
	pipeline_layout_info.setLayoutCount = 1;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));

	PipelineBuilder pipelineBuilder;

	//use the triangle layout we created
	pipelineBuilder._pipelineLayout = _meshPipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.Set_Shaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.Set_Input_Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.Set_Polygon_Mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.Set_Cull_Mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.Set_Multisampling_None();
	
	pipelineBuilder.Disable_Blending();			
	// pipelineBuilder.Enable_Blending_Additive();
	// pipelineBuilder.Enable_Blending_AlphaBlend();
	// pipelineBuilder.Enable_Blending_Multiply();
	// pipelineBuilder.Enable_Blending_Screen();
	// pipelineBuilder.Enable_Blending_PremultipliedAlpha();
	// pipelineBuilder.Enable_Blending_Subtractive();
	// pipelineBuilder.Enable_Blending_Invert();
	// pipelineBuilder.Enable_Blending_Min();
	// pipelineBuilder.Enable_Blending_Max();
	// pipelineBuilder.Enable_Blending_ColorDodge();

	// switch (tempBlendModeIndex) {
	// case 0:  pipelineBuilder.Disable_Blending();					  break;
	// case 1:  pipelineBuilder.Enable_Blending_Additive();           break;
	// case 2:  pipelineBuilder.Enable_Blending_AlphaBlend();         break;
	// case 3:  pipelineBuilder.Enable_Blending_Multiply();           break;
	// case 4:  pipelineBuilder.Enable_Blending_Screen();             break;
	// case 5:  pipelineBuilder.Enable_Blending_PremultipliedAlpha(); break;
	// case 6:  pipelineBuilder.Enable_Blending_Subtractive();        break;
	// case 7:  pipelineBuilder.Enable_Blending_Invert();             break;
	// case 8:  pipelineBuilder.Enable_Blending_Min();                break;
	// case 9:  pipelineBuilder.Enable_Blending_Max();                break;
	// case 10: pipelineBuilder.Enable_Blending_ColorDodge();         break;
	// }

	// pipelineBuilder.Disable_DepthTest();
	pipelineBuilder.Enable_DepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//connect the image format we will draw into, from draw image
	pipelineBuilder.Set_Color_Attachment_Format(_drawImage.imageFormat);
	pipelineBuilder.Set_Depth_Format(_depthImage.imageFormat);

	//finally build the pipeline
	_meshPipeline = pipelineBuilder.Build_Pipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.Push_Function([&]
	{
		vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _meshPipeline, nullptr);
	});

}

void VulkanEngine::Create_Swapchain(const uint32_t aWidth, const uint32_t aHeight)
{
	vkb::SwapchainBuilder swapchainBuilder{_chosen_GPU, _device, _surface};

	_swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{
		  .format = _swapchain_image_format,
		  .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		})
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(aWidth, aHeight)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	_swapchain_extent = vkbSwapchain.extent;
	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchain_images = vkbSwapchain.get_images().value();
	_swapchain_image_views = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::Destroy_Swapchain() const
{
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (const auto& swapchainImageView : _swapchain_image_views)
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

	const ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

	// bind the gradient drawing compute pipeline
	vkCmdBindPipeline(aCmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(aCmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(aCmd, static_cast<uint32_t>(std::ceil(_drawExtent.width / 16.0)), static_cast<uint32_t>(std::ceil(_drawExtent.height / 16.0)), 1);
}

void VulkanEngine::Imgui_Run()
{
	if (ImGui::Begin("settings"))
	{
		ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

		ImGui::Text("Selected effect: ", selected.name);

		ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, static_cast<int>(backgroundEffects.size() - 1));

		ImGui::ColorEdit4("data1", reinterpret_cast<float*>(&selected.data.data1));
		ImGui::ColorEdit4("data2", reinterpret_cast<float*>(&selected.data.data2));
		ImGui::ColorEdit4("data3", reinterpret_cast<float*>(&selected.data.data3));
		ImGui::ColorEdit4("data4", reinterpret_cast<float*>(&selected.data.data4));
		
		ImGui::Separator();

		ImGui::SliderFloat("camera fov", &tempCameraFOV, 1, 180);
		// ImGui::SliderFloat3("pos", &tempView.x, -20.0f, 1.f);
		ImGui::SliderFloat("Render Scale", &_renderScale, 0.3f, 1.f);
		ImGui::Value("cameraPitchRad", _mainCamera.pitch);

		ImGui::Begin("Stats");

		ImGui::Text("frame time %f ms", _stats.frameTime);
		ImGui::Text("draw time %f ms", _stats.mesh_draw_time);
		ImGui::Text("update time %f ms", _stats.scene_update_time);
		ImGui::Text("triangles %u", _stats.tri_count);
		ImGui::Text("draws %i", _stats.drawCall_count);
		ImGui::End();

		//
		// // The list of names matching your functions
		// const char* blendNames[] = 
		// {
		// 	"Disabled",
		// 	"Additive",
		// 	"Alpha Blend",
		// 	"Multiply",
		// 	"Screen",
		// 	"Premultiplied Alpha",
		// 	"Subtractive",
		// 	"Invert",
		// 	"Min",
		// 	"Max",
		// 	"Color Dodge"
		// };
		// // Create the dropdown menu
		// if (ImGui::Combo("Blend Function", &tempBlendModeIndex, blendNames, IM_ARRAYSIZE(blendNames))) 
		// {
		// 	// This block executes only when the value changes
		// 	printf("Blend mode changed to: %s\n", blendNames[tempBlendModeIndex]);
		// }
	}
	ImGui::End();
}

void VulkanEngine::Draw_Geometry(const VkCommandBuffer aCmd)
{
	// reset counters
	_stats.drawCall_count = 0;
	_stats.tri_count = 0;
	auto start = std::chrono::system_clock::now();

	std::vector<uint32_t> opaque_draws;
	opaque_draws.reserve(_mainDrawContext.opaqueSurfaces.size());

	for (uint32_t i = 0; i < _mainDrawContext.opaqueSurfaces.size(); i++) 
	{
		if (is_visible(_mainDrawContext.opaqueSurfaces[i], _sceneData.viewProj))
		{
			opaque_draws.push_back(i);
		}
	}

	std::vector<uint32_t> transparent_draws;
	transparent_draws.reserve(_mainDrawContext.transparentSurfaces.size());
	
	for (uint32_t i = 0; i < _mainDrawContext.transparentSurfaces.size(); i++)
	{
		if (is_visible(_mainDrawContext.transparentSurfaces[i], _sceneData.viewProj))
		{
			transparent_draws.push_back(i);
		}
	}

	// TODO:
	// Another way of doing this is that we would calculate a sort key, and then our opaque_draws would be something like 20 bits draw index, and 44 bits for sort key / hash.That way would be faster than this as it can be sorted through faster methods.
	// this is also done every frame which I don't know is needed? since when do shaders on objects change.....? not that often right?
	// TODO: multithread? maybe?
	
	// sort the opaque surfaces by material and mesh
	std::ranges::sort(opaque_draws, [&](const auto& iA, const auto& iB) 
	{
		const RenderObject& A = _mainDrawContext.opaqueSurfaces[iA];
		const RenderObject& B = _mainDrawContext.opaqueSurfaces[iB];
		if (A.material == B.material) 
		{
			return A.indexBuffer < B.indexBuffer;
		}
		return A.material < B.material;
	});
	
	// TODO- With the transparent objects, you want to also change the sorting code so that it checks distance from bounds to the camera, so that objects draw more correct. But sorting by depth is incompatible with sorting by pipeline, so you will need to decide what works better for your case.
	std::ranges::sort(transparent_draws, [&](const auto& iA, const auto& iB)
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

		const RenderObject& A = _mainDrawContext.transparentSurfaces[iA];
		const RenderObject& B = _mainDrawContext.transparentSurfaces[iB];
		if (A.material != B.material) 
		{
			return A.material < B.material;  // Batch materials.
		}
		const glm::vec3 cameraPos = _sceneData.view[3];
		const float distSqA = distance2(cameraPos, A.bounds.origin);
		const float distSqB = distance2(cameraPos, B.bounds.origin);
		if (distSqA > distSqB) return true;
		if (distSqA < distSqB) return false;

		// Tertiary: stable tie-breaker
		return A.indexBuffer < B.indexBuffer;	
	});

	//begin a render pass  connected to our draw image
	const VkRenderingAttachmentInfo colorAttachment = vkInit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingAttachmentInfo depthAttachment = vkInit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	const VkRenderingInfo renderInfo = vkInit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(aCmd, &renderInfo);

	vkCmdBindPipeline(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

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

	 //allocate a new uniform buffer for the scene data
	 AllocatedBuffer gpuSceneDataBuffer = Create_Buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	
	 //add it to the deletion queue of this frame so it gets deleted once it's been used
	 Get_Current_Frame()._deletionQueue.Push_Function([gpuSceneDataBuffer, this]
	 {
	 	Destroy_Buffer(gpuSceneDataBuffer);
	 });
	
	 //write the buffer
	 GPUSceneData* sceneUniformData = static_cast<GPUSceneData*>(gpuSceneDataBuffer.allocation->GetMappedData());
	 *sceneUniformData = _sceneData;

	// create a descriptor set that binds that buffer and update it
	 VkDescriptorSet globalDescriptor = Get_Current_Frame()._frameDescriptors.Allocate(_device, _gpuSceneDataDescriptorLayout);

	 DescriptorWriter writer;
	 writer.Write_Buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	 writer.Update_Set(_device, globalDescriptor);
	
	 //defined outside of the draw function, this is the state we will try to skip
	 MaterialPipeline* lastPipeline = nullptr;
	 MaterialInstance* lastMaterial = nullptr;
	 VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	 auto draw = [&](const RenderObject& r) 
	 {
		if (r.material != lastMaterial)
		{
			lastMaterial = r.material;
			// rebind pipeline and descriptors if the material changed
			if (r.material->pipeline != lastPipeline)
			{
				lastPipeline = r.material->pipeline;

				vkCmdBindPipeline(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
				vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr);

				//set dynamic viewport and scissor
				VkViewport viewport;
				viewport.x = 0;
				viewport.y = 0;
				viewport.width = static_cast<float>(_windowExtent.width);
				viewport.height = static_cast<float>(_windowExtent.height);
				viewport.minDepth = 0.f;
				viewport.maxDepth = 1.f;

				vkCmdSetViewport(aCmd, 0, 1, &viewport);

				VkRect2D scissor;
				scissor.offset.x = 0;
				scissor.offset.y = 0;
				scissor.extent.width = _windowExtent.width;
				scissor.extent.height = _windowExtent.height;

				vkCmdSetScissor(aCmd, 0, 1, &scissor);
			}
			vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1, &r.material->materialSet, 0, nullptr);

		}
	 	
		if (r.indexBuffer != lastIndexBuffer)
		{
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(aCmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}

	 	GPUDrawPushConstants pushConstants;
	 	pushConstants._worldMatrix = r.transform;
	 	pushConstants._vertexBuffer = r.vertexBufferAddress;
	 	vkCmdPushConstants(aCmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

	 	vkCmdDrawIndexed(aCmd, r.indexCount, 1, r.firstIndex, 0, 0);
	 	_stats.drawCall_count++;
	 	_stats.tri_count += r.indexCount / 3;
	 };

	 for (auto& r : opaque_draws) 
	 {
		 draw(_mainDrawContext.opaqueSurfaces[r]);
	 }

	 for (auto& r : transparent_draws) 
	 {
		 draw(_mainDrawContext.transparentSurfaces[r]);
	 }

	 // we delete the draw commands now that we processed them
	 _mainDrawContext.opaqueSurfaces.clear();
	 _mainDrawContext.transparentSurfaces.clear();


	vkCmdEndRendering(aCmd);
	
	auto end = std::chrono::system_clock::now();
	//convert to microseconds (integer), and then come back to miliseconds
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	_stats.mesh_draw_time = elapsed.count() / 1000.f;
	// old manual drawing
	// VkDescriptorSet imageSet = Get_Current_Frame()._frameDescriptors.Allocate(_device, _singleImageDescriptorLayout);
	// {
	// 	auto& imageToBind = _errorCheckerboardImage;
	// 	DescriptorWriter writer;
	// 	writer.Write_Image(0, imageToBind.imageView, _defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	//
	// 	writer.Update_Set(_device, imageSet);
	// }
	// vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipelineLayout, 0, 1, &imageSet, 0, nullptr);
	// const glm::mat4 view = glm::translate(tempView);
	// // For the projection matrix, we are doing a trick here. Note that we are sending 10000 to the “near” and 0.1 to the “far”. We will be reversing the depth, so that depth 1 is the near plane, and depth 0 the far plane. This is a technique that greatly increases the quality of depth testing.
	// glm::mat4 projection = glm::perspective(glm::radians(tempCameraFOV), static_cast<float>(_drawExtent.width) / static_cast<float>(_drawExtent.height), 10000.f, 0.1f);
	//
	// // invert the Y direction on projection matrix so that we are more similar to opengl and gltf axis
	// projection[1][1] *= -1;
	//
	// const auto& mesh = _testMeshes[2]; // 0 cube, 1 sphere, 2 monke
	// GPUDrawPushConstants push_Constants{};
	// push_Constants._worldMatrix = projection * view;
	// push_Constants._vertexBuffer = mesh->meshBuffers._vertexBufferAddress;
	//
	// vkCmdPushConstants(aCmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_Constants);
	// vkCmdBindIndexBuffer(aCmd, mesh->meshBuffers._indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	//
	// vkCmdDrawIndexed(aCmd, mesh->surfaces[0].count, 1, mesh->surfaces[0].startIndex, 0, 0);
}

AllocatedBuffer VulkanEngine::Create_Buffer(const size_t anAllocSize, const VkBufferUsageFlags aUsage, const VmaMemoryUsage aMemoryUsage) const
{
	// allocate buffer
	VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferInfo.pNext = nullptr;
	bufferInfo.size = anAllocSize;

	bufferInfo.usage = aUsage;

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = aMemoryUsage;
	vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	AllocatedBuffer newBuffer;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation,
		&newBuffer.info));

	return newBuffer;
}

void VulkanEngine::Destroy_Buffer(const AllocatedBuffer& aBuffer) const
{
	vmaDestroyBuffer(_allocator, aBuffer.buffer, aBuffer.allocation);
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

	_resize_requested = false;
}

void VulkanEngine::Update_Scene()
{
	const auto start = std::chrono::system_clock::now();

	_mainDrawContext.opaqueSurfaces.clear();

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
	glm::mat4 projection = glm::perspective(glm::radians(70.f), static_cast<float>(_windowExtent.width) / static_cast<float>(_windowExtent.height), 10000.f, 0.1f);

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	projection[1][1] *= -1;

	_sceneData.view = view;
	_sceneData.proj = projection;
	_sceneData.viewProj = projection * view;

	//some default lighting parameters
	_sceneData.ambientColor = glm::vec4(.1f);
	_sceneData.sunlightColor = glm::vec4(1.f);
	_sceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);

	const auto end = std::chrono::system_clock::now();
	//convert to microseconds (integer), and then come back to miliseconds
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	_stats.scene_update_time = elapsed.count() / 1000.f;
}

bool is_visible(const RenderObject& aObj, const glm::mat4& aViewProj)
{
	// TODO.
	// This is just one of the multiple possible functions we could be using for frustum culling.The way this works is that we are transforming each of the 8 corners of the mesh - space bounding box into screenspace, using object matrix and view - projection matrix.For those, we find the screen - space box bounds, and we check if that box is inside the clip - space view.This way of calculating bounds is on the slow side compared to other formulas, and can have false - positives where it things objects are visible when they arent. All the functions have different tradeoffs, and this one was selected for code simplicity and parallels with the functions we are doing on the vertex shaders.

	constexpr std::array corners
	{
		glm::vec3 { 1, 1, 1 },
		glm::vec3 { 1, 1, -1 },
		glm::vec3 { 1, -1, 1 },
		glm::vec3 { 1, -1, -1 },
		glm::vec3 { -1, 1, 1 },
		glm::vec3 { -1, 1, -1 },
		glm::vec3 { -1, -1, 1 },
		glm::vec3 { -1, -1, -1 },
	};

	const glm::mat4 matrix = aViewProj * aObj.transform;

	glm::vec3 min = { 1.5, 1.5, 1.5 };
	glm::vec3 max = { -1.5, -1.5, -1.5 };
	
	for (int c = 0; c < 8; c++) 
	{
		// project each corner into clip space
		glm::vec4 v = matrix * glm::vec4(aObj.bounds.origin + (corners[c] * aObj.bounds.extents), 1.f);
		
		// perspective correction
		v.x = v.x / v.w;
		v.y = v.y / v.w;
		v.z = v.z / v.w;
	
		min = glm::min(glm::vec3{ v.x, v.y, v.z }, min);
		max = glm::max(glm::vec3{ v.x, v.y, v.z }, max);
	}
	
	// check the clip space box is within the view
	return min.z <= 1.f && max.z >= 0.f && min.x <= 1.f && max.x >= -1.f && min.y <= 1.f && max.y >= -1.f;

	// slop
	// // Compute clip space positions for all 8 corners
	// std::array<glm::vec4, 8> clip_verts;
	// for (int c = 0; c < 8; c++) {
	// 	glm::vec3 corner_pos = aObj.bounds.origin + (corners[c] * aObj.bounds.extents);
	// 	clip_verts[c] = matrix * glm::vec4(corner_pos, 1.f);
	// }
	//
	// // Define the 6 frustum planes' outside conditions
	// // For each plane, check if ALL vertices are on the outside
	// // If yes for any plane, the object is not visible
	//
	// // Left plane: x < -w
	// bool all_outside = true;
	// for (const auto& v : clip_verts) {
	// 	if (!(v.x < -v.w)) {
	// 		all_outside = false;
	// 		break;
	// 	}
	// }
	// if (all_outside) return false;
	//
	// // Right plane: x > w
	// all_outside = true;
	// for (const auto& v : clip_verts) {
	// 	if (!(v.x > v.w)) {
	// 		all_outside = false;
	// 		break;
	// 	}
	// }
	// if (all_outside) return false;
	//
	// // Bottom plane: y < -w
	// all_outside = true;
	// for (const auto& v : clip_verts) {
	// 	if (!(v.y < -v.w)) {
	// 		all_outside = false;
	// 		break;
	// 	}
	// }
	// if (all_outside) return false;
	//
	// // Top plane: y > w
	// all_outside = true;
	// for (const auto& v : clip_verts) {
	// 	if (!(v.y > v.w)) {
	// 		all_outside = false;
	// 		break;
	// 	}
	// }
	// if (all_outside) return false;
	//
	// // Near plane: z < 0
	// all_outside = true;
	// for (const auto& v : clip_verts) {
	// 	if (!(v.z < 0.f)) {
	// 		all_outside = false;
	// 		break;
	// 	}
	// }
	// if (all_outside) return false;
	//
	// // Far plane: z > w
	// all_outside = true;
	// for (const auto& v : clip_verts) {
	// 	if (!(v.z > v.w)) {
	// 		all_outside = false;
	// 		break;
	// 	}
	// }
	// if (all_outside) return false;
	//
	// // If not culled by any plane, the object is potentially visible
	// return true;
}

GPUMeshBuffers VulkanEngine::UploadMesh(const std::span<uint32_t> aIndices, const std::span<Vertex> aVertices) const
{
	const size_t vertexBufferSize = aVertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = aIndices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	// create vertex buffer
	// It's not necessary for meshes to use GPU_ONLY vertex buffers, but it's highly recommended unless it's something like a CPU side particle system or other dynamic effects.

	newSurface._vertexBuffer = Create_Buffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	//find the address of the vertex buffer
	const VkBufferDeviceAddressInfo deviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .pNext = nullptr, .buffer = newSurface._vertexBuffer.buffer};
	newSurface._vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

	//create index buffer
	newSurface._indexBuffer = Create_Buffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	// staging buffer is 1 buffer for both copies to index and vertex buffers.
	const AllocatedBuffer staging = Create_Buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = staging.allocation->GetMappedData(); // doing this gives us the address so we can write to it. not a copy but just pointing to the staging buffer.

	// copy vertex buffer
	memcpy(data, aVertices.data(), vertexBufferSize);
	// copy index buffer
	memcpy(static_cast<char*>(data) + vertexBufferSize, aIndices.data(), indexBufferSize);

	Immediate_Submit([&](const VkCommandBuffer aCmd)
	{
		VkBufferCopy vertexCopy;
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(aCmd, staging.buffer, newSurface._vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy;
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(aCmd, staging.buffer, newSurface._indexBuffer.buffer, 1, &indexCopy);
	});

	Destroy_Buffer(staging);

	return newSurface;
}
