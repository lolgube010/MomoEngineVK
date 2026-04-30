#include <api/RenderDocWrapper.h>

#ifdef MOMOVK_ENABLE_RENDERDOC

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <SDL3/SDL.h>

static std::string Find_RenderDoc_DLL()
{
    if (const char* envPath = std::getenv("RENDERDOC_PATH"))
    {
        std::string path = envPath;
        if (!path.empty())
        {
            if (path.back() == '\\' || path.back() == '/')
                path += "renderdoc.dll";
            return path;
        }
    }
    return R"(C:\Program Files\RenderDoc\renderdoc.dll)";
}

void RenderDocWrapper::Load()
{
    HMODULE mod = GetModuleHandleA("renderdoc.dll");
    if (!mod)
    {
        const std::string dllPath = Find_RenderDoc_DLL();
        mod = LoadLibraryA(dllPath.c_str());
        if (!mod)
        {
            fmt::println("RenderDoc: DLL not found at '{}'. Launch via RenderDoc or set RENDERDOC_PATH.", dllPath);
            return;
        }
    }

    auto getApi = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mod, "RENDERDOC_GetAPI"));
    if (!getApi)
    {
        fmt::println("RenderDoc: Failed to locate RENDERDOC_GetAPI.");
        return;
    }

    const int ret = getApi(eRENDERDOC_API_Version_1_6_0, reinterpret_cast<void**>(&_rdoc_api));
    if (ret != 1)
    {
        fmt::println("RenderDoc: GetAPI returned {} — version may be too old (need 1.6.0+).", ret);
        _rdoc_api = nullptr;
        return;
    }

    // Disable F12 capture key — captures are driven from ImGui instead
    _rdoc_api->SetCaptureKeys(nullptr, 0);

    fmt::println("RenderDoc: Loaded successfully.");
}

void RenderDocWrapper::Set_Window(const VkInstance aInstance, SDL_Window* aWindow)
{
    if (!_rdoc_api) return;

    auto* hwnd = static_cast<HWND>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(aWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd) return;

    _devicePtr = RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(aInstance);
    _rdoc_api->SetActiveWindow(_devicePtr, hwnd);
}

void RenderDocWrapper::Annotate_Draw(const VkCommandBuffer aCmd, const char* aMaterial, const char* aMesh, const char* aPass) const
{
    if (!_rdoc_api || !_devicePtr || !_rdoc_api->SetCommandAnnotation) return;

    _rdoc_api->SetCommandAnnotation(_devicePtr, aCmd, "pass",     eRENDERDOC_String, 0, RDAnnotationHelper(aPass));
    if (aMaterial)
        _rdoc_api->SetCommandAnnotation(_devicePtr, aCmd, "material", eRENDERDOC_String, 0, RDAnnotationHelper(aMaterial));
    if (aMesh)
        _rdoc_api->SetCommandAnnotation(_devicePtr, aCmd, "mesh",     eRENDERDOC_String, 0, RDAnnotationHelper(aMesh));
}

void RenderDocWrapper::Trigger_Capture() const
{
    if (!_rdoc_api) return;
    _rdoc_api->TriggerCapture();
}

void RenderDocWrapper::Launch_Replay_UI() const
{
    if (!_rdoc_api) return;
    // If the replay UI is already open and connected, just bring it to front
    if (_rdoc_api->IsTargetControlConnected())
        _rdoc_api->ShowReplayUI();
    else
        _rdoc_api->LaunchReplayUI(1, nullptr);
}

void RenderDocWrapper::Annotate_Object(const VkInstance aInst, void* aVulkanObj) const
{
    if (!_rdoc_api) return;
    void* dev = RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(aInst);
    const auto res = _rdoc_api->SetObjectAnnotation(dev, aVulkanObj, "key", eRENDERDOC_String, 0, RDAnnotationHelper("value"));
    Evaluate_Result(res);
}

void RenderDocWrapper::Evaluate_Result(const uint32_t aRes)
{
    switch (aRes)
    {
    case 0: break; // success
    case 1: fmt::println("RenderDoc annotation: device unknown or invalid"); break;
    case 2: fmt::println("RenderDoc annotation: not recognised for this object/queue"); break;
    case 3: fmt::println("RenderDoc annotation: ill-formed call"); break;
    default: fmt::println("RenderDoc annotation: unknown result {}", aRes); break;
    }
}

#endif // MOMOVK_ENABLE_RENDERDOC
