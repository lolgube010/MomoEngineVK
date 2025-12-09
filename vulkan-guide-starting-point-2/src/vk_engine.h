// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
// This will be the main class for the engine, and where most of the code of the tutorial will go
#pragma once

#include <vk_types.h>

#include <ranges>

#include <vk_descriptors.h>


union SDL_Event;

struct AllocatedImage
{
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

struct DeletionQueue
{
	// Doing callbacks like this is inefficient at scale, because we are storing whole std::functions for every object we are deleting, which is not going to be optimal.For the amount of objects we will use in this tutorial, it's going to be fine.but if you need to delete thousands of objects and want them deleted faster, a better implementation would be to store arrays of vulkan handles of various types such as VkImage, VkBuffer, and so on.And then delete those from a loop.

	std::deque<std::function<void()>> _deletors;

	void Push_Function(std::function<void()>&& aFunction)
	{
		_deletors.push_back(aFunction);
	}

	void Flush()
	{
		// reverse iterate the deletion queue to execute all the functions
		for (auto& deletor : std::ranges::reverse_view(_deletors))
		{
			deletor(); //call functors
		}

		_deletors.clear();
	}
};

struct FrameData
{
	VkCommandPool _commandPool; // a command pool creates buffers, one pool / thread, even though pools can create multiple buffers
	VkCommandBuffer _mainCommandBuffer; // holds commands, this is mainly just a handle, actual data is being handled by vulkan

	//The _swapchainSemaphore is going to be used so that our render commands wait on the swapchain image request. 
	//The _renderSemaphore will be used to control presenting the image to the OS once the drawing finishes 
	//The _renderFence will lets us wait for the draw commands of a given frame to be finished.

	// NOTE: render semaphore replaced with vector since it's supposed to be tied to image count and not FIF. 
	VkSemaphore _swapchainSemaphore/*, _renderSemaphore*/; // gpu to gpu sync. 
	VkFence _renderFence; // gpu to cpu sync
	DeletionQueue _deletionQueue;
};

constexpr unsigned int FRAME_OVERLAP = 2; // also known as number of frames in flight

class VulkanEngine
{
public:
	bool _is_initialized{false};
	int _frame_number{0};
	bool _stop_rendering{false};
	VkExtent2D _window_extent{1500, 700}; // og was 1700, 900

	struct SDL_Window* _window{nullptr};

	static VulkanEngine& Get();

	// initializes everything in the engine
	void Init();

	// shuts down the engine
	void Cleanup();

	// draw loop
	void Draw();

	void Draw_Imgui(VkCommandBuffer aCmd, VkImageView aTargetImageView) const;

	// run main loop
	void Run();

	FrameData& Get_Current_Frame()
	{
		return _frames[_frame_number % FRAME_OVERLAP];
	}

	void Immediate_Submit(std::function<void(VkCommandBuffer cmd)>&& aFunction);

	VkInstance _instance; // vulkan library handle - "The Vulkan context, used to access drivers."
	VkDebugUtilsMessengerEXT _debug_messenger; // vulkan debug output handle
	VkPhysicalDevice _chosen_GPU; // GPU chosen as the default device. - "A GPU. Used to query physical GPU details, like features, capabilities, memory size, etc."
	VkDevice _device; // Vulkan Device for commands - "The “logical” GPU context that you actually execute things on."
	VkSurfaceKHR _surface; // vulkan window surface

	// <swapchain
	VkSwapchainKHR _swapchain;
	// Holds the images for the screen. It allows you to render things into a visible window. The KHR suffix shows that it comes from an extension, which in this case is VK_KHR_swapchain
	VkFormat _swapchain_image_format;
	std::vector<VkImage> _swapchain_images; // A VkImage is a handle to the actual image object to use as texture or to render into. -  "A texture you can write to and read from."
	std::vector<VkImageView> _swapchain_image_views; // A VkImageView is a wrapper for that image. It allows to do things like swap the colors. We will go into detail about it later.
	VkExtent2D _swapchain_extent;
	// swapchain>

	// momo fix, previously called render_semaphore, also called submit semaphores
	std::vector<VkSemaphore> ready_for_present_semaphores; // submit semaphores, bug from vulkan from before.

	// <queues
	FrameData _frames[FRAME_OVERLAP];

	VkQueue _graphicsQueue; // what the command buffers submit into
	uint32_t _graphicsQueueFamily; // what type of graphics queue we want
	// queues>

	DeletionQueue _mainDeletionQueue;

	VmaAllocator _allocator;

	//draw resources
	AllocatedImage _drawImage;
	VkExtent2D _drawExtent;

	DescriptorAllocator _globalDescriptorAllocator;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

private:
	void ProcessInput(SDL_Event& anE);

	void Init_Vulkan();
	void Init_Swapchain();
	void Init_Commands();
	void Init_Sync_Structures();
	void Init_Descriptors();
	void Init_Pipelines();
	void Init_Background_Pipelines();
	void Init_Imgui();

	void CreateSwapchain(uint32_t aWidth, uint32_t aHeight);
	void DestroySwapchain() const;

	void DrawBackground(VkCommandBuffer aCmd) const;
};
