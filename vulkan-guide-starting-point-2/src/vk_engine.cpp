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

// globals
namespace
{
	VulkanEngine* gl_LoadedEngine = nullptr;
}

constexpr bool bUseValidationLayers = true;
constexpr auto AppName = "MomoVK";

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

	constexpr auto window_flags = SDL_WINDOW_VULKAN;

	_window = SDL_CreateWindow(
		AppName,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_window_extent.width,
		_window_extent.height,
		window_flags
	);

	Init_Vulkan();
	Init_Swapchain();
	Init_Commands();
	Init_Sync_Structures();
	Init_Descriptors();
	Init_Pipelines();
	Init_Imgui();

	_is_initialized = true;
}

void VulkanEngine::Cleanup()
{
	if (_is_initialized)
	{
		vkDeviceWaitIdle(_device);

		for (auto& frame : _frames)
		{
			vkDestroyCommandPool(_device, frame._commandPool, nullptr);

			//destroy sync objects
			vkDestroyFence(_device, frame._renderFence, nullptr);
			vkDestroySemaphore(_device, frame._swapchainSemaphore, nullptr);
			//vkDestroySemaphore(_device, _frame._renderSemaphore, nullptr);

			frame._deletionQueue.Flush();
		}
		for (const auto& ready_for_present_semaphore : ready_for_present_semaphores)
		{
			vkDestroySemaphore(_device, ready_for_present_semaphore, nullptr);
		}

		_mainDeletionQueue.Flush();

		DestroySwapchain();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);

		vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
	gl_LoadedEngine = nullptr;
}

void VulkanEngine::Draw()
{
	//> draw_1
	// wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &Get_Current_Frame()._renderFence, true, 1000000000));
	Get_Current_Frame()._deletionQueue.Flush();
	VK_CHECK(vkResetFences(_device, 1, &Get_Current_Frame()._renderFence));
	//< draw_1

	//> draw_2
	// request image from the swapchain
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, Get_Current_Frame()._swapchainSemaphore, nullptr, &swapchainImageIndex));
	//< draw_2

	//> draw_3
	// naming it cmd for shorter writing
	const VkCommandBuffer cmd = Get_Current_Frame()._mainCommandBuffer;

	// now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	const VkCommandBufferBeginInfo cmdBeginInfo = vkInit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	_drawExtent.width = _drawImage.imageExtent.width;
	_drawExtent.height = _drawImage.imageExtent.height;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// transition our main draw image into general layout so we can write into it.
	// we will overwrite it all so we don't care about what was the older layout
	vkUtil::Transition_Image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	DrawBackground(cmd);

	//transition the draw image and the swapchain image into their correct transfer layouts
	vkUtil::Transition_Image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
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

	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	//increase the number of frames drawn
	_frame_number++;
	//< draw_6
}

void VulkanEngine::Draw_Imgui(const VkCommandBuffer aCmd, const VkImageView aTargetImageView) const
{
	const VkRenderingAttachmentInfo colorAttachment = vkInit::attachment_info(aTargetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingInfo renderInfo = vkInit::rendering_info(_swapchain_extent, &colorAttachment, nullptr);

	vkCmdBeginRendering(aCmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), aCmd);

	vkCmdEndRendering(aCmd);
}

void VulkanEngine::Run()
{
	SDL_Event e;
	bool bQuit = false;

	// main loop
	while (!bQuit)
	{
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
	}
}

void VulkanEngine::Immediate_Submit(std::function<void(VkCommandBuffer cmd)>&& aFunction)
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

	// vk 1.3 features
	VkPhysicalDeviceVulkan13Features features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	features.dynamicRendering = true;
	features.synchronization2 = true;

	// vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	// Use vk-bootstrap to select a gpu. 
	// We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
	vkb::PhysicalDeviceSelector selector{vkb_inst};
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.set_surface(_surface)
		.select()
		.value();

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

	//> init vma
	// initialize the memory allocator
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _chosen_GPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	// momo debug adventure
	_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(_instance, "vkSetDebugUtilsObjectNameEXT");

	_mainDeletionQueue.Push_Function([&]()
	{
		vmaDestroyAllocator(_allocator);
	});
	//< init vma
}

void VulkanEngine::Init_Swapchain()
{
	CreateSwapchain(_window_extent.width, _window_extent.height);

	//> create image (fullscreen render target/render image)
	//draw image size will match the window
	const VkExtent3D drawImageExtent = {
		_window_extent.width,
		_window_extent.height,
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

	//add to deletion queues
	_mainDeletionQueue.Push_Function([=]()
	{
		vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
	});
	//< create image
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

	_mainDeletionQueue.Push_Function([=]()
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
	_mainDeletionQueue.Push_Function([=]() { vkDestroyFence(_device, _immFence, nullptr); });

}

void VulkanEngine::Init_Descriptors()
{
	//create a descriptor pool that will hold 10 sets with 1 image each
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{._type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ._ratio = 1}
	};

	_globalDescriptorAllocator.Init_Pool(_device, 10, sizes);

	//make the descriptor set layout for our compute draw
	{
		DescriptorLayoutBuilder builder;
		builder.Add_Binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		_drawImageDescriptorLayout = builder.Build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	//allocate a descriptor set for our draw image
	_drawImageDescriptors = _globalDescriptorAllocator.Allocate(_device, _drawImageDescriptorLayout);

	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo.imageView = _drawImage.imageView;

	VkWriteDescriptorSet drawImageWrite = {};
	drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	drawImageWrite.pNext = nullptr;

	drawImageWrite.dstBinding = 0;
	drawImageWrite.dstSet = _drawImageDescriptors;
	drawImageWrite.descriptorCount = 1;
	drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	drawImageWrite.pImageInfo = &imgInfo;

	vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

	//make sure both the descriptor allocator and the new layout get cleaned up properly
	_mainDeletionQueue.Push_Function([&]()
	{
		_globalDescriptorAllocator.Destroy_Pool(_device);

		vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
	});
}

void VulkanEngine::Init_Pipelines()
{
	Init_Background_Pipelines();
}

void VulkanEngine::Init_Background_Pipelines()
{
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant;
	pushConstant.offset = 0;
	pushConstant.size = sizeof(ComputePushConstants);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

	VkResult loadShaderResult = {};

	VkShaderModule gradientShader;
	{
		const std::string gradientShaderPath = momo_util::BuildShaderPath("gradient_color", momo_util::ShaderType::Compute, true);
		if (!vkUtil::LoadShaderModule(gradientShaderPath.c_str(), _device, &gradientShader, loadShaderResult))
		{
			fmt::print("Error when building the compute shader {}\n", static_cast<int>(loadShaderResult));
		}
	}

	VkShaderModule skyShader;
	{
		const std::string skyShaderPath = momo_util::BuildShaderPath("sky", momo_util::ShaderType::Compute, true);
		if (!vkUtil::LoadShaderModule(skyShaderPath.c_str(), _device, &skyShader, loadShaderResult))
		{
			fmt::print("Error when building the compute shader {}\n", static_cast<int>(loadShaderResult));
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

	ComputeEffect gradient;
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

	ComputeEffect sky;
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
	_mainDeletionQueue.Push_Function([=]()
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
	_mainDeletionQueue.Push_Function([=]()
	{
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
	});

}

void VulkanEngine::CreateSwapchain(const uint32_t aWidth, const uint32_t aHeight)
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

void VulkanEngine::DestroySwapchain() const
{
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (const auto& swapchainImageView : _swapchain_image_views)
	{
		vkDestroyImageView(_device, swapchainImageView, nullptr);
	}
}

void VulkanEngine::DrawBackground(const VkCommandBuffer aCmd) const
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
	if (ImGui::Begin("background"))
	{
		ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

		ImGui::Text("Selected effect: ", selected.name);

		ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

		ImGui::ColorEdit4("data1", reinterpret_cast<float*>(&selected.data.data1));
		ImGui::ColorEdit4("data2", reinterpret_cast<float*>(&selected.data.data2));
		ImGui::ColorEdit4("data3", reinterpret_cast<float*>(&selected.data.data3));
		ImGui::ColorEdit4("data4", reinterpret_cast<float*>(&selected.data.data4));
	}
	ImGui::End();
}
