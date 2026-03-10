#include "RenderDocWrapper.h"

#if defined(_WIN32)
// Define WIN32_LEAN_AND_MEAN to exclude rarely-used services and
// prevent Windows.h from polluting your namespace with things like min/max macros
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <SDL_syswm.h>

void RenderDocWrapper::Init_RenderDoc(const VkInstance* aVkInstance, SDL_Window* aSDLWindow)
{
    bool succeeded = false;
    // See if the RenderDoc DLL is currently loaded in our process
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
    
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_7_0, (void**)&_rdoc_api);
        if (ret == 1)
        {
            fmt::print("RenderDoc Successfully Loaded automatically through attachment.\n");
            succeeded = true;
        }
    }
    else
    {
        HMODULE mod2 = LoadLibraryA(R"(C:\Program Files\RenderDoc\renderdoc.dll)");

        if (mod2)
        {
            pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod2, "RENDERDOC_GetAPI");

            int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_7_0, (void**)&_rdoc_api);
            if (ret == 1)
            {
                fmt::print("RenderDoc Successfully Loaded through manually finding the .dll.\n");
                succeeded = true;
            }
        }
        else
        {
            fmt::print("Couldn't find RenderDoc installation at 'C:/Program Files/RenderDoc/renderdoc.dll' or as an attached DLL. RenderDoc will be unavailable. \n");
        }
    }
    if (!succeeded)
    {
        return;
    }
    if (!aVkInstance)
    {
        return;
    }
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version)
    if (SDL_GetWindowWMInfo(aSDLWindow, &wmInfo))
    {
        void* windowHandle = wmInfo.info.win.window;
        // from here on, renderDoc is loaded.
        auto* ptr = RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(*aVkInstance);
        _rdoc_api->SetActiveWindow(ptr, windowHandle);
    }
}

// There are C++ helper structs RDGLObjectHelper and RDAnnotationHelper that can simplify code for specifying single scalar values.
// For Vulkan, annotating VkInstance, VkPhysicalDevice, or VkDevice objects may encounter problems due to loader wrapping. To address this, you can use vkSetDebugUtilsObjectTagEXT to set a tag with the tagName set to RENDERDOC_APIObjectAnnotationHelper and the pTag set to the handle of the object itself. After doing that once RenderDoc will be able to recognise those handles.
template <typename T>
void RenderDocWrapper::AnnotateCommand(const VkInstance aInst, const T aCmdOrQueue) const
{
    // Prevent accidental passing of VkBuffer, VkImage, etc.
    static_assert(std::is_same_v<T, VkCommandBuffer> || std::is_same_v<T, VkQueue>, "AnnotateCommand only accepts VkCommandBuffer or VkQueue!");

    void* dev = RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(aInst);
    auto res = _rdoc_api->SetCommandAnnotation(dev, aCmdOrQueue, "draw_mesh.my_property", eRENDERDOC_Int32, 0, RDAnnotationHelper(200));
    EvaluateRes(res);
    res = _rdoc_api->SetCommandAnnotation(dev, aCmdOrQueue, "draw_mesh.source_name", eRENDERDOC_String, 0, RDAnnotationHelper("Default_Model.file"));
    EvaluateRes(res);
}

void RenderDocWrapper::AnnotateObject(const VkInstance aInst, void* aVulkanObj) const
{
    // TODO:
    const char* key = "poop";
    const char* value = "poop2";
    RENDERDOC_AnnotationType type = eRENDERDOC_String; // what type are we filling this with?
    void* dev = RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(aInst);
    auto res = _rdoc_api->SetObjectAnnotation(dev, aVulkanObj, key, type, 0, RDAnnotationHelper(value));
    EvaluateResult(res);
}

void RenderDocWrapper::StartCapture()
{
    // _rdoc_api->StartFrameCapture();
}

void RenderDocWrapper::EndCapture()
{
    // _rdoc_api->EndFrameCapture();
}

void RenderDocWrapper::Capture() const
{
    _rdoc_api->TriggerCapture();
}

void RenderDocWrapper::EvaluateResult(const uint32_t aRes)
{
    if (aRes == 0)
    {
        fmt::print("// annotation successfully set\n");
    }
    if (aRes == 1)
    {
        fmt::print("// device unknown or invalid\n");
        
    }
    if (aRes == 2)
    {
        fmt::print("// device is valid but the annotation is not recognized or not supported for API-specific reasons, such as an unrecognised or invalid object or queue/command buffer\n");
    }
    if (aRes == 3)
    {
        fmt::print("// the call is ill-formed or invalid e.g. empty is specified with a value pointer, or non-empty is specified with a NULL value pointer.\n");
    }
}
