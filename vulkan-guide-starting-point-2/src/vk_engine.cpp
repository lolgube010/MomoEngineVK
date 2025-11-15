//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#include <chrono>
#include <thread>


VulkanEngine* gl_LoadedEngine = nullptr;

constexpr bool bUseValidationLayers = true;
constexpr auto AppName = "momoEngine VK";

VulkanEngine& VulkanEngine::Get()
{
	return *gl_LoadedEngine;
}

void VulkanEngine::init()
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

	init_vulkan();
	init_swapchain();
	init_commands();
	init_sync_structures();

	// everything went fine
	_is_initialized = true;
}

void VulkanEngine::cleanup()
{
	if (_is_initialized)
	{
		destroy_swapchain();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyDevice(_device, nullptr);

		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);
		SDL_DestroyWindow(_window);
	}
	// clear engine pointer
	gl_LoadedEngine = nullptr;
}

void VulkanEngine::draw()
{
	// nothing yet
}

void VulkanEngine::run()
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

			process_input(e);
		}

		// do not draw if we are minimized
		if (_stop_rendering)
		{
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		draw();
	}
}

void VulkanEngine::process_input(SDL_Event& anE)
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

void VulkanEngine::init_vulkan()
{
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

	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	// vk 1.3 features
	VkPhysicalDeviceVulkan13Features features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	features.dynamicRendering = true;
	features.synchronization2 = true;

	// vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	// Use vkbootstrap to select a gpu. 
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
}

void VulkanEngine::init_swapchain()
{
	create_swapchain(_window_extent.width, _window_extent.height);
}

void VulkanEngine::init_commands() {}
void VulkanEngine::init_sync_structures() {}

void VulkanEngine::create_swapchain(const uint32_t aWidth, const uint32_t aHeight)
{
	vkb::SwapchainBuilder swapchainBuilder{_chosen_GPU, _device, _surface};

	_swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{.format = _swapchain_image_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
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

void VulkanEngine::destroy_swapchain() const
{
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (const auto& swapchainImageView : _swapchain_image_views)
	{
		vkDestroyImageView(_device, swapchainImageView, nullptr);
	}
}
