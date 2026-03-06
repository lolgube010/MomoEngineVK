#pragma once
#include "renderdoc_app.h"

struct SDL_Window;

class RenderDocWrapper
{
public:
    void Init_RenderDoc(const VkInstance* aVkInstance, SDL_Window* aSDLWindow);
    
    template <typename T>
    void AnnotateCommand(VkInstance aInst, const T aCmdOrQueue) const;
    void AnnotateObject(VkInstance aInst, void* aVulkanObj) const;
    RENDERDOC_API_1_1_2* _rdoc_api = nullptr;

private:
    static void EvaluateRes(uint32_t aRes);
};