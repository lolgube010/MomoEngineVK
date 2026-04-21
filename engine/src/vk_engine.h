#pragma once
#include <chrono>
#include <vk_types.h>
#include <vk_descriptors.h>
#include "camera.h"
#include "vk_loader.h"
#include "MomoTracy.h"
#include "RenderDocWrapper.h"
#include "vk_debug.h"
#include <unordered_set>

struct FrameData
{
	//The _swapchainSemaphore is going to be used so that our render commands wait on the swapchain image request. 
	//The _renderSemaphore will be used to control presenting the image to the OS once the drawing finishes 
	//The _renderFence will let us wait for the draw commands of a given frame to be finished.

	// NOTE: render semaphore replaced with vector since it's supposed to be tied to image count and not FIF. 
	VkSemaphore _swapchainSemaphore/*, _renderSemaphore*/; // gpu to gpu sync. 
	VkFence _renderFence; // gpu to cpu sync

	VkCommandPool _commandPool; // a command pool creates buffers, one pool / thread, even though pools can create multiple buffers. a memory allocator for our commandBuffers.
	VkCommandBuffer _mainCommandBuffer; // holds commands, this is mainly just a handle, actual data is being handled by vulkan

	DeletionQueue _deletionQueue;
	DescriptorAllocatorGrowable _frameDescriptors; // [0] storage image, [1] storage buffer, [2] uniform buffer, [3] image/sampler
	AllocatedBuffer sceneDataBuffer; // persistent HOST_VISIBLE UBO, written each frame
};

constexpr unsigned int FRAME_OVERLAP = 2; // also known as number of frames in flight

// written into uniform buffers
struct GLTFMetallic_Roughness
{
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout; // [0] Uniform Buffer (gltfMaterialData), [1] image/sampler (colorTex), [2] image/sampler (metalRoughTex)

	struct MaterialConstants
	{
		glm::vec4 colorFactors; // used to multiply the color texture
		glm::vec4 metal_rough_factors; // metallic and roughness parameters on r and b components, plus two more that are used in other places.

		uint32_t colorTexID;
        uint32_t metalRoughTexID;
        uint32_t pad1;
        uint32_t pad2;
		// We have also a bunch of vec4s for padding. In vulkan, when you want to bind a uniform buffer, it needs to meet a minimum requirement for its alignment. 256 bytes is a good default alignment for this which all the gpus we target meet, so we are adding those vec4s to pad the structure to 256 bytes.
		glm::vec4 extra[13] = {};
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
		uint32_t padding; // added by me
	};

	DescriptorWriter writer;

	void Build_Pipelines();
	void Clear_Resources(VkDevice aDevice) const;

	// create the descriptor set and return a fully built MaterialInstance struct
	MaterialInstance Write_Material(VkDevice aDevice, MaterialPass aPass, const MaterialResources& aResources, DescriptorAllocatorGrowable& aDescriptorAllocator, const char* aName);
};

static_assert(sizeof(GLTFMetallic_Roughness::MaterialConstants) == 256); 

struct RenderObject
{
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;
	Bounds bounds;
	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string_view matDebugName;
    std::string_view meshDebugName;
    const char* combinedDebugLabel = nullptr; //  points into GeoSurface::combinedDebugLabel
#endif
};

struct DrawContext
{
	std::vector<RenderObject> opaqueSurfaces;
	std::vector<RenderObject> transparentSurfaces;
};

struct MeshNode : Node 
{
	std::shared_ptr<MeshAsset> mesh;

	void Draw(const glm::mat4& aTopMatrix, DrawContext& aCtx) override;
};

struct TextureCache
{
    struct ViewSamplerKey
    {
        VkImageView imageView;
        VkSampler   sampler;
        bool operator==(const ViewSamplerKey&) const = default;
    };
    struct ViewSamplerHash
    {
        size_t operator()(const ViewSamplerKey& k) const noexcept
        {
            const size_t h1 = std::hash<uint64_t>{}(std::bit_cast<uint64_t>(k.imageView));
            const size_t h2 = std::hash<uint64_t>{}(std::bit_cast<uint64_t>(k.sampler));
            return h1 ^ (h2 * 2654435761u + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
        }
    };

    std::vector<VkDescriptorImageInfo> _cache;
    std::unordered_map<std::string, TextureID> _nameMap;
    std::unordered_set<uint32_t> _freeSlots;
    std::unordered_set<VkImageView> _engineImages;
    std::unordered_map<ViewSamplerKey, uint32_t, ViewSamplerHash> _lookup;
    bool _dirty = true;

    TextureID AddTexture(const VkImageView& aImage, VkSampler aSampler);
    void MarkEngineImage(VkImageView aView);
    // Sets freed slots to aFallback so the descriptor array stays valid, then queues them for reuse.
    // Engine-owned slots and duplicate IDs are silently skipped.
    // Call before destroying the underlying images — the fallback must outlive this call.
    void FreeTextures(std::span<const TextureID> aIDs, const VkDescriptorImageInfo& aFallback);
};

struct EngineStats
{
	float frameTime;
	uint32_t tri_count;
	int drawCall_count;
	float scene_update_time;
	float mesh_draw_time;
    float filler;
    uint64_t frequency;
};

class VulkanEngine
{
public:
	bool _is_initialized{false};
	bool _freeze_rendering{false};
	bool _resize_requested = false;

	int _frame_number{0};
	VkExtent2D _windowExtent{ 1700, 900 }; // og was 1700, 900

	SDL_Window* _window{nullptr};

	static VulkanEngine& Get();

	// singleton stuff
	VulkanEngine(const VulkanEngine&) = delete;
    VulkanEngine& operator=(const VulkanEngine&) = delete;
    VulkanEngine(VulkanEngine&&) = delete;
    VulkanEngine& operator=(VulkanEngine&&) = delete;

	// initializes everything in the engine
	void Init();

	// run main loop
	void Run();
	
    // draw loop
	void Draw();

	// shuts down the engine
	void Cleanup();

	FrameData& Get_Current_Frame()
	{
		return _frames[_frame_number % FRAME_OVERLAP];
	}
    FrameData& Get_Last_Frame() { return _frames[(_frame_number - 1) % FRAME_OVERLAP]; }

	// Send some commands to the GPU without synchronizing with swapchain or with rendering logic.
	void Immediate_Submit(const std::function<void(VkCommandBuffer cmd)>& aFunction) const;

	// TODO:
	// Note that this pattern is not very efficient, as we are waiting for the GPU command to fully execute before continuing with our CPU side logic. This is something people generally put on a background thread, whose sole job is to execute uploads like this one, and deleting/reusing the staging buffers.
	GPUMeshBuffers UploadMesh(std::span<uint32_t> aIndices, std::span<Vertex> aVertices, const char* aMeshName) const;

	VkInstance _instance; // vulkan library handle - "The Vulkan context, used to access drivers."
	VkDebugUtilsMessengerEXT _debug_messenger; // vulkan debug output handle
	VkPhysicalDevice _chosen_GPU; // GPU chosen as the default device. - "A GPU. Used to query physical GPU details, like features, capabilities, memory size, etc."
	VkDevice _device; // Vulkan Device for commands - "The “logical” GPU context that you actually execute things on."
	VkSurfaceKHR _surface; // vulkan window surface, just sent to sdl/swapchain

	// <swapchain
	// Holds the images for the screen. It allows you to render things into a visible window. The KHR suffix shows that it comes from an extension, which in this case is VK_KHR_swapchain
	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;
	std::vector<VkImage> _swapchain_images; // A VkImage is a handle to the actual image object to use as texture or to render into. -  "A texture you can write to and read from."
	std::vector<VkImageView> _swapchain_image_views; // A VkImageView is a wrapper for that image. It allows to do things like swap the colors. We will go into detail about it later.
	VkExtent2D _swapchain_extent;

	// <queues
	FrameData _frames[FRAME_OVERLAP];
	
    uint32_t _swapchainImageCount{0};
    // uint32_t _swapchainImageIndex;
	std::vector<VkSemaphore> ready_for_present_semaphores; // previously called render_semaphore, also called submit semaphores.

	// TODO-
	// It is common to see engines using 3 queue families. One for drawing the frame, other for async compute, and other for data transfer. We use a single queue that will run all our commands for simplicity.
	VkQueue _graphicsQueue; // what the command buffers submit into
	uint32_t _graphicsQueueFamily; // what type of graphics queue we want

	DeletionQueue _mainDeletionQueue;

	VmaAllocator _allocator; // allocates / deallocates images, buffers

	//draw resources
	AllocatedImage _drawImage; // our main draw image
	AllocatedImage _depthImage; // main depth

	VkExtent2D _drawExtent;
	// float _renderScale = 1.f;

	DescriptorAllocatorGrowable _globalDescriptorAllocator; // lives throughout the engines lifetime, used for compute image and fallback shader.

	VkDescriptorSet _drawImageDescriptors; // allocated from globalDescriptorAllocator
	VkDescriptorSetLayout _drawImageDescriptorLayout; // for compute draw (storage image), bg shader

	// VkPipeline _gradientPipeline;
	VkPipelineLayout _ComputePipelineLayout; // prev: gradientPipelineLayout

	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{0};

    // momo_vkDebug::Vk_Debug_Info _debugInfo;

	// VkPipelineLayout _trianglePipelineLayout;
	// VkPipeline _trianglePipeline;

	// old mesh pipeline. maybe repurpose for test meshes or something. 
	// VkPipelineLayout _meshPipelineLayout; 
	// VkPipeline _meshPipeline; 
	// GPUMeshBuffers _rectangle;
	// std::vector<std::shared_ptr<MeshAsset>> _testMeshes; 

	GPUSceneData _sceneData = {};
	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
	VkDescriptorPool _persistentDescPool{VK_NULL_HANDLE};
	std::array<VkDescriptorSet, FRAME_OVERLAP> _persistentGlobalDescriptors{};

	// actually allocate a new image
	AllocatedImage Create_Image(VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, const char* aName, bool aMipmapped = false) const;
	// call the above function to allocate a new image and just pass the data to that.
    AllocatedImage Create_Image(const void* aData, VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, const char* aName, bool aMipmapped = false) const;
	
    [[nodiscard]] AllocatedBuffer Create_Buffer(size_t anAllocSize, VkBufferUsageFlags aUsage, VmaMemoryUsage aMemoryUsage, const char* aName) const;
	
    void Destroy_Image(const AllocatedImage& aImg) const;
	void Destroy_Buffer(const AllocatedBuffer& aBuffer) const;

	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

    TextureCache texCache;

	GLTFMetallic_Roughness metalRoughMaterial;

	DrawContext _mainDrawContext;

	Camera _mainCamera;

	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> _loadedScenes;

#ifdef TRACY_ENABLE
	tracy::VkCtx* _tracyVkCtx = nullptr;
#endif

	EngineStats _stats = {};

	RenderDocWrapper _render_doc;

private:
    VulkanEngine() = default;
    ~VulkanEngine() = default;

	void Init_Vulkan();
	void Init_Swapchain();
	void Init_Commands();
	void Init_Sync_Structures();
	void Init_Descriptors();
	void Init_ImGui();
	void Init_Tracy();
	void Init_Default_Data();
	void Init_Models();

	void Init_Pipelines();
	void Init_Background_Pipelines();

	void Create_Swapchain(uint32_t aWidth, uint32_t aHeight);
	void Destroy_Swapchain() const;

	void Draw_Background(VkCommandBuffer aCmd) const;
	void Draw_Geometry(VkCommandBuffer aCmd);

	void Draw_ImGui(VkCommandBuffer aCmd, VkImageView aTargetImageView) const;
    void Draw_Main(VkCommandBuffer aCmd);
	void ImGui_Run();
    void ImGuiFrame();

	void Resize_Swapchain();
	void Update_Scene();

    void ProcessEvents(bool& aQuit);
	// temp camera settings
	float tempCameraFOV = 70.f;
    glm::vec4 tempAmbientColor = glm::vec4(1.f);
    glm::vec4 tempSunColor = glm::vec4(1.f);
    glm::vec4 tempSunDir = glm::vec4(0, 1, 0.5, 1.f);
	// int tempBlendModeIndex = 0;

    static bool Is_Visible(const RenderObject& aObj, const glm::mat4& aViewProj);

    static const char* Get_Device_Type_String(VkPhysicalDeviceType aType);
    static std::string Get_Buffer_Usage_Flag_String(VkBufferUsageFlags aUsageFlag);
    
    template <typename T>
    std::string FormatWithCommas(T aValue)
    {
        std::string str = std::to_string(aValue);

        // If the number is negative, we don't want to insert a comma after the minus sign
        int limit = (aValue < 0) ? 1 : 0;
        int insert_idx = static_cast<int>(str.length()) - 3;

        while (insert_idx > limit)
        {
            str.insert(insert_idx, ",");
            insert_idx -= 3;
        }

        return str;
    }

    momo_vkDebug::ValidationCapture _validationCapture;

    VmaTotalStatistics _cachedVmaStats{};
    std::chrono::steady_clock::time_point _lastVmaStatsTime{};
};
