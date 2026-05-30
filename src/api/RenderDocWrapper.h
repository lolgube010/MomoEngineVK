#pragma once

struct SDL_Window;

#ifdef MOMOVK_ENABLE_RENDERDOC

#include "renderdoc_app.h"

class RenderDocWrapper
{
public:
    // Call BEFORE Init_Vulkan() — DLL must be loaded before Vulkan init so RenderDoc can intercept it
    void Load();
    // Call AFTER Init_Vulkan() — tells RenderDoc which window to target and caches the device pointer
    void Set_Window(VkInstance aInstance, SDL_Window* aWindow);

    void Trigger_Capture() const;
    void Launch_Replay_UI() const;

    // Call once per draw immediately before vkCmdDrawIndexed.
    // aMaterial / aMesh may be nullptr (annotation skipped for that key).
    // Silently no-ops if RenderDoc < 1.7.0 is installed (SetCommandAnnotation will be null).
    void Annotate_Draw(VkCommandBuffer aCmd, const char* aMaterial, const char* aMesh, const char* aPass) const;

    bool Is_Loaded() const { return _rdoc_api != nullptr; }

private:
    void Annotate_Object(VkInstance aInst, void* aVulkanObj) const;
    static void Evaluate_Result(uint32_t aRes);

    RENDERDOC_API_1_6_0* _rdoc_api = nullptr;
    void* _devicePtr = nullptr;
};

#else

// When MOMOVK_ENABLE_RENDERDOC is off, RenderDocWrapper compiles to zero-cost no-ops.
// All call sites in vk_engine remain unchanged.
class RenderDocWrapper
{
public:
    // ReSharper disable CppMemberFunctionMayBeStatic
    void Load() {}
    void Set_Window(VkInstance, SDL_Window*) {}
    void Trigger_Capture() {}
    void Launch_Replay_UI() {}
    void Annotate_Draw(VkCommandBuffer, const char*, const char*, const char*) {}
    bool Is_Loaded() { return false; }
};

#endif
