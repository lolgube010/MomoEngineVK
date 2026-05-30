#include <engine_main/engine.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#include <input/Input.h>
#include <imgui/backends/imgui_impl_sdl3.h>

constexpr auto APP_NAME = "MomoVK";

VulkanEngine& VulkanEngine::Get()
{
    static VulkanEngine instance;
    return instance;
}

void VulkanEngine::Init()
{
    SDL_Init(SDL_INIT_VIDEO);
    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    _window = SDL_CreateWindow(APP_NAME, _windowExtent.width, _windowExtent.height, window_flags);
    _renderer.Init(_window, _windowExtent, _stats);
    _scene.Init();
    Input::Instance().Init();
    _isInitialized = true;
}

void VulkanEngine::Run()
{
    bool bQuit = false;

    // Fixed simulation step. Gameplay/physics runs at this rate regardless of render rate.
    constexpr double fixed_Step = 1.0 / 60.0;
    // Spiral-of-doom guard. 8 * 1/60 ~= 133 ms (per Tyler Glaiel's recommendation).
    constexpr double max_Delta  = 8.0 * fixed_Step;
    // Absorbs OS timer noise (~200 us) so a vsynced frame snaps to a clean fraction.
    constexpr double snap_Tol   = 0.0002;

    double accumulator = 0.0;
    uint64_t lastTime = SDL_GetPerformanceCounter();

    while (!bQuit)
    {
        PROFILE_SCOPE_N("Frame")
        const uint64_t currentTime = SDL_GetPerformanceCounter();
        double dt = static_cast<double>(currentTime - lastTime) / static_cast<double>(_stats._frequency);
        lastTime = currentTime;

        dt = std::min(dt, max_Delta);
        _stats._frameTime = static_cast<float>(dt * 1000.0);

        if      (std::abs(dt - 1.0 / 240.0) < snap_Tol) dt = 1.0 / 240.0;
        else if (std::abs(dt - 1.0 / 165.0) < snap_Tol) dt = 1.0 / 165.0;
        else if (std::abs(dt - 1.0 / 144.0) < snap_Tol) dt = 1.0 / 144.0;
        else if (std::abs(dt - 1.0 / 120.0) < snap_Tol) dt = 1.0 / 120.0;
        else if (std::abs(dt - 1.0 /  60.0) < snap_Tol) dt = 1.0 /  60.0;
        else if (std::abs(dt - 1.0 /  30.0) < snap_Tol) dt = 1.0 /  30.0;
        else if (std::abs(dt - 1.0 /  20.0) < snap_Tol) dt = 1.0 /  20.0;

        ProcessEvents(bQuit);
        Input::Instance().PostUpdate();

        if (_freezeRendering)
        {
            accumulator = 0.0;
            Input::Instance().FlushKeyEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (_resizeRequested)
        {
            _renderer.Resize_Swapchain(_windowExtent);
            _resizeRequested = false;
        }

        accumulator += dt;
        while (accumulator >= fixed_Step)
        {
            _scene.Update(static_cast<float>(fixed_Step), _windowExtent);
            Input::Instance().FlushKeyEvents();
            accumulator -= fixed_Step;
        }

        _renderer.ImGui_Update(_scene);
        Draw();
        PROFILE_FRAME;
    }
}

void VulkanEngine::Draw()
{
    _renderer.Draw(_scene.GetDrawContext(), _scene.GetSceneData(), _frameNumber, _resizeRequested);
}

void VulkanEngine::Cleanup()
{
    if (_isInitialized)
    {
        vkDeviceWaitIdle(_renderer.GetDevice());
        _scene._loadedModels.clear();
        _renderer.Cleanup();
        SDL_DestroyWindow(_window);
    }
}

FrameData& VulkanEngine::Get_Current_Frame()
{ return _renderer.GetCurrentFrame(_frameNumber); }

FrameData& VulkanEngine::Get_Last_Frame()
{ return _renderer.GetLastFrame(_frameNumber); }

VkSampler VulkanEngine::GetDefaultSamplerLinear() const
{ return _renderer.GetDefaultSamplerLinear(); }

const AllocatedImage& VulkanEngine::GetErrorCheckerboardImage() const
{ return _renderer.GetErrorCheckerboardImage(); }

const AllocatedImage& VulkanEngine::GetWhiteImage() const
{ return _renderer.GetWhiteImage(); }

TextureCache& VulkanEngine::GetTexCache()
{ return _renderer.GetTexCache(); }

GLTFMetallic_Roughness& VulkanEngine::GetMetalRoughMaterial()
{ return _renderer.GetMetalRoughMaterial(); }

VulkanEngine::VulkanEngine() = default;
VulkanEngine::~VulkanEngine() = default;

// ---------------------------------------------------------------------------
// GPU resource forwards
// ---------------------------------------------------------------------------

AllocatedImage VulkanEngine::Create_Image(const VkExtent3D aSize, const VkFormat aFormat, const VkImageUsageFlags aUsage, const char* aName, const bool aMipmapped) const
{
    return _renderer.Create_Image(aSize, aFormat, aUsage, aName, aMipmapped);
}

AllocatedImage VulkanEngine::Create_Image(const void* aData, const VkExtent3D aSize, const VkFormat aFormat, const VkImageUsageFlags aUsage, const char* aName, const bool aMipmapped) const
{
    return _renderer.Create_Image(aData, aSize, aFormat, aUsage, aName, aMipmapped);
}

AllocatedBuffer VulkanEngine::Create_Buffer(const size_t anAllocSize, const VkBufferUsageFlags aUsage,  const VmaMemoryUsage aMemoryUsage, const char* aName) const
{
    return _renderer.Create_Buffer(anAllocSize, aUsage, aMemoryUsage, aName);
}

void VulkanEngine::Destroy_Image(const AllocatedImage& aImg) const   { _renderer.Destroy_Image(aImg); }
void VulkanEngine::Destroy_Buffer(const AllocatedBuffer& aBuffer) const  { _renderer.Destroy_Buffer(aBuffer); }

GPUMeshBuffers VulkanEngine::UploadMesh(const std::span<uint32_t> aIndices, const std::span<Vertex> aVertices, const char* aMeshName) const
{
    return _renderer.UploadMesh(aIndices, aVertices, aMeshName);
}

void VulkanEngine::Immediate_Submit(const std::function<void(VkCommandBuffer aCmd)>& aFunction) const
{
    _renderer.Immediate_Submit(aFunction);
}

// ---------------------------------------------------------------------------
// Event processing
// ---------------------------------------------------------------------------

void VulkanEngine::ProcessEvents(bool& aQuit)
{
    PROFILE_SCOPE_N("ProcessEvents")
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        ImGui_ImplSDL3_ProcessEvent(&e);
        Input::Instance().ProcessEvent(e);

        if (e.type == SDL_EVENT_QUIT)                       { aQuit = true; }
        if (e.type == SDL_EVENT_WINDOW_MINIMIZED)           { _freezeRendering = true; }
        if (e.type == SDL_EVENT_WINDOW_RESTORED)            { _freezeRendering = false; }
        if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)  { _resizeRequested = true; }
    }
}

