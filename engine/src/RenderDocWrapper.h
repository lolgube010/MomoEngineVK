#pragma once
#include "renderdoc_app.h"

struct SDL_Window;

class RenderDocWrapper
{
private: // don't use.
    void Init_RenderDoc(const VkInstance* aVkInstance, SDL_Window* aSDLWindow);
    
    template <typename T>
    void AnnotateCommand(VkInstance aInst, const T aCmdOrQueue) const;
    void AnnotateObject(VkInstance aInst, void* aVulkanObj) const;

    void StartCapture();
    void EndCapture();
    void Capture() const; // idk about this one
    RENDERDOC_API_1_1_2* _rdoc_api = nullptr;
    static void EvaluateResult(uint32_t aRes);
};