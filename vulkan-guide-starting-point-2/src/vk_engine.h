// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
// This will be the main class for the engine, and where most of the code of the tutorial will go
#pragma once

#include <vk_types.h>

#include <ranges>

#include <vk_descriptors.h>

#include "camera.h"
#include "vk_loader.h"

union SDL_Event;

struct DeletionQueue
{
	// Doing callbacks like this is inefficient at scale, because we are storing whole std::functions for every object we are deleting, which is not going to be optimal.For the amount of objects we will use in this tutorial, it's going to be fine.but if you need to delete thousands of objects and want them deleted faster, a better implementation would be to store arrays of vulkan handles of various types such as VkImage, VkBuffer, and so on.And then delete those from a loop.

	std::deque<std::function<void()>> _deleters;

	void Push_Function(std::function<void()>&& aFunction)
	{
		_deleters.push_back(std::move(aFunction));
	}

	void Flush()
	{
		// reverse iterate the deletion queue to execute all the functions
		for (auto& deleter : std::ranges::reverse_view(_deleters))
		{
			deleter(); //call functors
		}

		_deleters.clear();
	}
};

struct FrameData
{
	//The _swapchainSemaphore is going to be used so that our render commands wait on the swapchain image request. 
	//The _renderSemaphore will be used to control presenting the image to the OS once the drawing finishes 
	//The _renderFence will let us wait for the draw commands of a given frame to be finished.

	// NOTE: render semaphore replaced with vector since it's supposed to be tied to image count and not FIF. 
	VkSemaphore _swapchainSemaphore/*, _renderSemaphore*/; // gpu to gpu sync. 
	VkFence _renderFence; // gpu to cpu sync

	VkCommandPool _commandPool; // a command pool creates buffers, one pool / thread, even though pools can create multiple buffers
	VkCommandBuffer _mainCommandBuffer; // holds commands, this is mainly just a handle, actual data is being handled by vulkan

	DeletionQueue _deletionQueue;
	DescriptorAllocatorGrowable _frameDescriptors;
};

constexpr unsigned int FRAME_OVERLAP = 2; // also known as number of frames in flight


struct ComputePushConstants
{
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect
{
	const char* name;
	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

struct GLTFMetallic_Roughness
{
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants
	{
		glm::vec4 colorFactors; // used to multiply the color texture
		glm::vec4 metal_rough_factors; // metallic and roughness parameters on r and b components, plus two more that are used in other places.

		// We have also a bunch of vec4s for padding. In vulkan, when you want to bind a uniform buffer, it needs to meet a minimum requirement for its alignment. 256 bytes is a good default alignment for this which all the gpus we target meet, so we are adding those vec4s to pad the structure to 256 bytes.
		glm::vec4 extra[14];
	};

	// When we create the descriptor set, there are some textures we want to bind, and the uniform buffer with the color factors and other properties. We will hold those in the MaterialResources struct, so that its easy to send them to the write_material function.
	struct MaterialResources
	{
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void Build_Pipelines(VulkanEngine* aEngine);
	void clear_resources(VkDevice device) const;

	// create the descriptor set and return a fully built MaterialInstance struct
	MaterialInstance Write_Material(VkDevice aDevice, MaterialPass aPass, const MaterialResources& aResources, DescriptorAllocatorGrowable& aDescriptorAllocator);
};

struct RenderObject
{
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;

	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

struct DrawContext
{
	std::vector<RenderObject> opaqueSurfaces;
};

struct MeshNode : public Node 
{
	std::shared_ptr<MeshAsset> mesh;

	void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) override;
};

class VulkanEngine
{
public:
	bool _is_initialized{false};
	int _frame_number{0};
	bool _stop_rendering{false};
	VkExtent2D _windowExtent{ 1700, 900 }; // og was 1700, 900
	bool _resize_requested;

	struct SDL_Window* _window{nullptr};

	static VulkanEngine& Get();

	// initializes everything in the engine
	void Init();

	// draw loop
	void Draw();

	// run main loop
	void Run();
	
	// shuts down the engine
	void Cleanup();


	FrameData& Get_Current_Frame()
	{
		return _frames[_frame_number % FRAME_OVERLAP];
	}

	void Immediate_Submit(std::function<void(VkCommandBuffer aCmd)>&& aFunction) const;

	// added by momo
	void SetDebugInfo(uint64_t aObjectHandle, VkObjectType aObjectType, const char* a_pObjectName) const;
	
	// TODO:
	// Note that this pattern is not very efficient, as we are waiting for the GPU command to fully execute before continuing with our CPU side logic. This is something people generally put on a background thread, whose sole job is to execute uploads like this one, and deleting/reusing the staging buffers.
	GPUMeshBuffers UploadMesh(std::span<uint32_t> aIndices, std::span<Vertex> aVertices) const;

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
	AllocatedImage _drawImage; // our main draw image
	AllocatedImage _depthImage; // main depth

	VkExtent2D _drawExtent;
	float _renderScale = 1.f;

	DescriptorAllocatorGrowable _globalDescriptorAllocator;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{0};

	// momo debug adventure
	PFN_vkSetDebugUtilsObjectNameEXT _vkSetDebugUtilsObjectNameEXT;

	// VkPipelineLayout _trianglePipelineLayout;
	// VkPipeline _trianglePipeline;

	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;
	// GPUMeshBuffers _rectangle;
	
	std::vector<std::shared_ptr<MeshAsset>> _testMeshes;

	GPUSceneData _sceneData = {};
	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

	AllocatedImage Create_Image(VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, bool aMipmapped = false) const;
	AllocatedImage Create_Image(const void* aData, VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, bool aMipmapped = false) const;
	[[nodiscard]] AllocatedBuffer Create_Buffer(size_t anAllocSize, VkBufferUsageFlags aUsage, VmaMemoryUsage aMemoryUsage) const;
	void Destroy_Image(const AllocatedImage& aImg) const;

	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	VkDescriptorSetLayout _singleImageDescriptorLayout;

	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;

	DrawContext _mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<Node>> _loadedNodes;

	Camera _mainCamera;

	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> _loadedScenes;

private:
	void ProcessInput(SDL_Event& anE);

	void Init_Vulkan();
	void Init_Swapchain();
	void Init_Commands();
	void Init_Sync_Structures();
	void Init_Descriptors();
	void Init_Imgui();
	void Init_Default_Data(); // where we load models.

	void Init_Pipelines();
	void Init_Background_Pipelines();
	void Init_Mesh_Pipeline();

	void Create_Swapchain(uint32_t aWidth, uint32_t aHeight);
	void Destroy_Swapchain() const;

	void Draw_Background(VkCommandBuffer aCmd) const;
	void Draw_Geometry(VkCommandBuffer aCmd);

	void Draw_Imgui(VkCommandBuffer aCmd, VkImageView aTargetImageView) const;
	void Imgui_Run();

	void Destroy_Buffer(const AllocatedBuffer& aBuffer) const;

	void Resize_Swapchain();
	void Update_Scene();

	// temp camera settings
	float tempCameraFOV = 70.f;
	// int tempBlendModeIndex = 0;

	// momo slop bs

	VmaDeviceMemoryCallbacks _callbacks = {};
};

	static uint64_t g_TotalAllocatedBytes;
	static uint64_t g_TotalFreedBytes;
	static uint32_t g_AllocationCount;

	static void VKAPI_PTR MyAllocateCallback(
		VmaAllocator     allocator,
		uint32_t         memoryType,
		VkDeviceMemory   vkMem,
		VkDeviceSize     size,
		void* pUserData)
	{
		g_TotalAllocatedBytes += size;
		g_AllocationCount++;

		// Optional: you can also log memory type, handle etc.
		// printf("Allocated %llu B  (type %u)\n", size, memoryType);
	}

	static void VKAPI_PTR MyFreeCallback(
		VmaAllocator     allocator,
		uint32_t         memoryType,
		VkDeviceMemory   vkMem,
		VkDeviceSize     size,
		void* pUserData)
	{
		g_TotalFreedBytes += size;
		g_AllocationCount--;

		// printf("Freed %llu B  (type %u)\n", size, memoryType);
	}