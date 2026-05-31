#pragma once
#include <vk/render_types.h>
#include <vk/descriptors.h>
#include <vk/material.h>
#include <vk/texture_cache.h>
#include <vk/debug.h>
#include <api/RenderDocWrapper.h>
#include <vk/swapchain.h>
#include <vk/gpu_resources.h>
#include <api/engine_imgui.h>
#include <chrono>
#include <array>

#include <vk/deletion_queue.h>

struct SDL_Window;
class EngineScene;

// ---------------------------------------------------------------------------
// Frame pipelining
// ---------------------------------------------------------------------------

struct FrameData
{
    VkSemaphore _swapchainSemaphore;
    VkFence     _renderFence;

    VkCommandPool   _commandPool;
    VkCommandBuffer _mainCommandBuffer;

    DeletionQueue              _deletionQueue;
    DescriptorAllocatorGrowable _frameDescriptors;
    AllocatedBuffer             _sceneDataBuffer;
};

constexpr unsigned int FRAME_OVERLAP = 2;

// ---------------------------------------------------------------------------
// EngineRenderer
// ---------------------------------------------------------------------------

class EngineRenderer
{
public:
    void Init(SDL_Window* aWindow, VkExtent2D aWindowExtent, EngineStats& aStats, EngineImGui& aImgui);

    void Cleanup();

    // Called once per frame
    void Draw(const DrawContext& aDrawContext, const GPUSceneData& aSceneData,
              int& aFrameNumber, bool& aResizeRequested);

    // Swapchain resize — pass VulkanEngine's windowExtent so it stays in sync
    void Resize_Swapchain(VkExtent2D& aWindowExtent);

    // ---------------------------------------------------------------------------
    // GPU resource helpers (forwarded from VulkanEngine for loader.cpp compat)
    // ---------------------------------------------------------------------------
    [[nodiscard]] AllocatedImage Create_Image(VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, const char* aName, bool aMipmapped = false) const;
    [[nodiscard]] AllocatedImage Create_Image(const void* aData, VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, const char* aName, bool aMipmapped = false) const;
    [[nodiscard]] AllocatedBuffer Create_Buffer(size_t anAllocSize, VkBufferUsageFlags aUsage, VmaMemoryUsage aMemoryUsage, const char* aName) const;
    void Destroy_Image(const AllocatedImage& aImg) const;
    void Destroy_Buffer(const AllocatedBuffer& aBuffer) const;
    GPUMeshBuffers UploadMesh(std::span<uint32_t> aIndices, std::span<Vertex> aVertices, const char* aMeshName) const;
    void Immediate_Submit(const std::function<void(VkCommandBuffer aCmd)>& aFunction) const;

    // ---------------------------------------------------------------------------
    // Accessors (used by VulkanEngine forwarding wrappers and loader.cpp)
    // ---------------------------------------------------------------------------
    VkDevice GetDevice() const;
    VkSampler GetDefaultSamplerLinear() const;
    const AllocatedImage& GetErrorCheckerboardImage() const;
    const AllocatedImage& GetWhiteImage() const;
    TextureCache& GetTexCache();
    GLTFMetallic_Roughness& GetMetalRoughMaterial();
    VkExtent2D GetWindowExtent() const;

    FrameData& GetCurrentFrame(int aFrameNumber);
    FrameData& GetLastFrame(int aFrameNumber);

    ImGui_InitInfo GetImGuiInitInfo() const;
private:
    friend class EngineImGui;

    // Core Vulkan objects (owned)
    VkInstance       _instance{};
    VkDebugUtilsMessengerEXT _debugMessenger{};
    VkPhysicalDevice _chosenGPU{};
    VkDevice         _device{};
    VmaAllocator     _allocator{};
    VkQueue          _graphicsQueue{};
    uint32_t         _graphicsQueueFamily{};
    VkSurfaceKHR     _surface{};
    SDL_Window*      _window{};
    VkExtent2D       _windowExtent{};

    Momo_VkDebug::ValidationCapture _validationCapture;
    RenderDocWrapper _renderDoc;

    // Pointer to VulkanEngine-owned stats; set in Init()
    EngineStats* _pStats = nullptr;

    // Swapchain
    Swapchain                 _swapchain;
    std::vector<VkSemaphore>  _readyForPresentSemaphores;

    // Frame-in-flight data
    FrameData _frames[FRAME_OVERLAP]{};

    // Draw targets
    AllocatedImage _drawImage{};
    AllocatedImage _depthImage{};
    VkExtent2D     _drawExtent{};

    // Descriptors
    DescriptorAllocatorGrowable _globalDescriptorAllocator;
    VkDescriptorSet             _drawImageDescriptors{};
    VkDescriptorSetLayout       _drawImageDescriptorLayout{};
    VkDescriptorSetLayout       _gpuSceneDataDescriptorLayout{};

    VkPipelineLayout _computePipelineLayout{};

    // GPU resources (immediate submit, buffers, images)
    GpuResources _gpuResources;

    // Background compute
    std::vector<ComputeEffect> _backgroundEffects;
    int _currentBackgroundEffect{0};

    // Default resources
    AllocatedImage _whiteImage{};
    AllocatedImage _blackImage{};
    AllocatedImage _greyImage{};
    AllocatedImage _errorCheckerboardImage{};
    VkSampler _defaultSamplerLinear{};
    VkSampler _defaultSamplerNearest{};

    TextureCache           _texCache;
    GLTFMetallic_Roughness _metalRoughMaterial{};

    // Persistent descriptor set (global texture array)
    VkDescriptorPool                         _persistentDescPool{};
    std::array<VkDescriptorSet, FRAME_OVERLAP> _persistentGlobalDescriptors{};

    // Main deletion queue for renderer-owned resources
    DeletionQueue _deletionQueue;

    // VMA stats cache (re-computed every 5 s)
    VmaTotalStatistics                   _cachedVmaStats{};
    std::chrono::steady_clock::time_point _lastVmaStatsTime{};

    // for imgui
    VkDescriptorPool _imGuiPool{};

#if TRACY_ENABLE && TRACY_GPU_ENABLE
    tracy::VkCtx* _tracyVkCtx = nullptr;
#endif

    // ---------------------------------------------------------------------------
    // Init helpers
    // ---------------------------------------------------------------------------
    void Init_Vulkan();
    void Init_Swapchain();
    void Init_Commands();
    void Init_Sync_Structures();
    void Init_Descriptors();
    void Init_Pipelines();
    void Init_Background_Pipelines();
    void Init_Default_Data();
    void Init_Tracy();
    void Resize_Draw_Images();

    // ---------------------------------------------------------------------------
    // Draw helpers
    // ---------------------------------------------------------------------------
    void Draw_Background(VkCommandBuffer aCmd) const;
    void Draw_Geometry(VkCommandBuffer aCmd, const DrawContext& aDrawContext, const GPUSceneData& aSceneData, int aFrameNumber);
    void Draw_ImGui_Cmd(VkCommandBuffer aCmd, VkImageView aTargetImageView) const;
    void Draw_Main(VkCommandBuffer aCmd, const DrawContext& aDrawContext, const GPUSceneData& aSceneData, int aFrameNumber);

    // ---------------------------------------------------------------------------
    // Utilities
    // ---------------------------------------------------------------------------
    static bool Is_Visible(const RenderObject& aObj, const glm::mat4& aViewProj);
    static const char* Get_Device_Type_String(VkPhysicalDeviceType aType);
};
