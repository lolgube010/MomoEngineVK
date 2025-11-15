// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

union SDL_Event;

class VulkanEngine
{
public:
	bool _is_initialized{false};
	bool _stop_rendering{false};
	bool __PADDING1 = false;
	bool __PADDING2 = false;
	int _frame_number{0};
	VkExtent2D _window_extent{1500, 700}; // og was 1700, 900

	struct SDL_Window* _window{nullptr};

	static VulkanEngine& Get();

	// initializes everything in the engine
	void init();

	// shuts down the engine
	void cleanup();

	// draw loop
	void draw();

	// run main loop
	void run();

	VkInstance _instance; // vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger; // vulkan debug output handle
	VkPhysicalDevice _chosen_GPU; // GPU chosen as the default device
	VkDevice _device; // Vulkan Device for commands
	VkSurfaceKHR _surface; // vulkan window surface

	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;
	int __PADDING3 = 0;

	std::vector<VkImageView> _swapchain_image_views;
	std::vector<VkImage> _swapchain_images;
	VkExtent2D _swapchain_extent;

private:
	void process_input(SDL_Event& anE);
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();

	void create_swapchain(uint32_t aWidth, uint32_t aHeight);
	void destroy_swapchain() const;
};
