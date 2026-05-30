#pragma once
#include <vk/engine_rendering.h>
#include <engine_main/engine_scene.h>

class VulkanEngine
{
public:
    bool _isInitialized = false;
    bool _freezeRendering = false;
    bool _resizeRequested = false;

    int _frameNumber = 0;
    VkExtent2D _windowExtent{.width = 1700, .height = 900};

    SDL_Window* _window = nullptr;

    EngineStats _stats = {};

    // Subsystems
    EngineRenderer _renderer;
    EngineScene    _scene;

    static VulkanEngine& Get();

    VulkanEngine(const VulkanEngine&)            = delete;
    VulkanEngine& operator=(const VulkanEngine&) = delete;
    VulkanEngine(VulkanEngine&&)                 = delete;
    VulkanEngine& operator=(VulkanEngine&&)      = delete;

    void Init();
    void Run();
    void Draw();
    void Cleanup();

    VkDevice GetDevice() const { return _renderer.GetDevice(); }

    // ---------------------------------------------------------------------------
    // Frame helpers (forward to renderer)
    // ---------------------------------------------------------------------------
    FrameData& Get_Current_Frame();
    FrameData& Get_Last_Frame();

    // ---------------------------------------------------------------------------
    // GPU resource helpers — thin forwards to _renderer for loader.cpp compat
    // ---------------------------------------------------------------------------
    [[nodiscard]] AllocatedImage Create_Image(VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, const char* aName, bool aMipmapped = false) const;
    [[nodiscard]] AllocatedImage Create_Image(const void* aData, VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, const char* aName, bool aMipmapped = false) const;
    [[nodiscard]] AllocatedBuffer Create_Buffer(size_t anAllocSize, VkBufferUsageFlags aUsage, VmaMemoryUsage aMemoryUsage, const char* aName) const;
    void Destroy_Image(const AllocatedImage& aImg) const;
    void Destroy_Buffer(const AllocatedBuffer& aBuffer) const;
    GPUMeshBuffers UploadMesh(std::span<uint32_t> aIndices, std::span<Vertex> aVertices, const char* aMeshName) const;
    void Immediate_Submit(const std::function<void(VkCommandBuffer aCmd)>& aFunction) const;

    // Accessors used by loader.cpp (forward to _renderer)
    VkSampler GetDefaultSamplerLinear() const;
    const AllocatedImage& GetErrorCheckerboardImage() const;
    const AllocatedImage& GetWhiteImage() const;
    TextureCache& GetTexCache();
    GLTFMetallic_Roughness& GetMetalRoughMaterial();

private:
    VulkanEngine();
    ~VulkanEngine();

    void ProcessEvents(bool& aQuit);
};
