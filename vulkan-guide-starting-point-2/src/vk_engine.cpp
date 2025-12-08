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

// globals
namespace
{
	VulkanEngine* gl_LoadedEngine = nullptr;
}

constexpr bool bUseValidationLayers = true;
constexpr auto AppName = "momoEngine VK";

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

	auto window_flags = SDL_WINDOW_VULKAN;

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
	VkCommandBuffer cmd = Get_Current_Frame()._mainCommandBuffer;

	// now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkInit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	_drawExtent.width = _drawImage.imageExtent.width;
	_drawExtent.height = _drawImage.imageExtent.height;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkUtil::Transition_Image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	DrawBackground(cmd);

	//transition the draw image and the swapchain image into their correct transfer layouts
	vkUtil::Transition_Image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkUtil::Transition_Image(cmd, _swapchain_images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// execute a copy from the draw image into the swapchain
	vkUtil::copy_image_to_image(cmd, _drawImage.image, _swapchain_images[swapchainImageIndex], _drawExtent, _swapchain_extent);

	// set swapchain image layout to Present so we can show it on the screen
	vkUtil::Transition_Image(cmd, _swapchain_images[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//< draw_4

	//> draw_5
	// prepare the submission to the queue. 
	// we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	// we will signal the _renderSemaphore, to signal that rendering has finished

	const VkCommandBufferSubmitInfo cmdInfo = vkInit::command_buffer_submit_info(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, Get_Current_Frame()._swapchainSemaphore);
	//VkSemaphoreSubmitInfo signalInfo = vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, Get_Current_Frame()._renderSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, ready_for_present_semaphores[swapchainImageIndex]);

	VkSubmitInfo2 submit = vkInit::submit_info(&cmdInfo, &signalInfo, &waitInfo);

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

			//process_input(e);
		}

		// do not draw if we are minimized
		if (_stop_rendering)
		{
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		Draw();
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
	VkExtent3D drawImageExtent = {
		_window_extent.width,
		_window_extent.height,
		1
	};

	//hardcoding the draw format to 32 bit float
	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_drawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo rimg_info = vkInit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

	// build an image-view for the draw image to use for rendering
	VkImageViewCreateInfo rview_info = vkInit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

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
	VkCommandPoolCreateInfo commandPoolInfo = vkInit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (auto& frame : _frames)
	{
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &frame._commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkInit::command_buffer_allocate_info(frame._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &frame._mainCommandBuffer));
	}
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
}

void VulkanEngine::Init_Descriptors()
{
	//create a descriptor pool that will hold 10 sets with 1 image each
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{._type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ._ratio = 1}
	};

	globalDescriptorAllocator.Init_Pool(_device, 10, sizes);

	//make the descriptor set layout for our compute draw
	{
		DescriptorLayoutBuilder builder;
		builder.Add_Binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		_drawImageDescriptorLayout = builder.Build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	//allocate a descriptor set for our draw image
	_drawImageDescriptors = globalDescriptorAllocator.Allocate(_device, _drawImageDescriptorLayout);

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
		globalDescriptorAllocator.Destroy_Pool(_device);

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

	VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

	VkShaderModule computeDrawShader;
	VkResult loadShaderResult = {};
	if (!vkUtil::LoadShaderModule("../../shaders/gradient.comp.spv", _device, &computeDrawShader, loadShaderResult))
	{
		fmt::print("Error when building the compute shader {}\n", static_cast<int>(loadShaderResult));
	}

	VkPipelineShaderStageCreateInfo stageInfo{};
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.pNext = nullptr;
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = computeDrawShader;
	stageInfo.pName = "main"; // this option gives you the ability to have multiple shaders in the same file, having different entry points.

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = _gradientPipelineLayout;
	computePipelineCreateInfo.stage = stageInfo;

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_gradientPipeline));

	vkDestroyShaderModule(_device, computeDrawShader, nullptr);

	_mainDeletionQueue.Push_Function([&]()
	{
		vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _gradientPipeline, nullptr);
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

	// bind the gradient drawing compute pipeline
	vkCmdBindPipeline(aCmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(aCmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(aCmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);

}
