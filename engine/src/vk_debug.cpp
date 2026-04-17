#include "vk_debug.h"
#include "imgui.h"

namespace momo_vkDebug
{

void ValidationCapture::Init(VkInstance aInstance)
{
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = Callback;
    info.pUserData       = this;
    vkCreateDebugUtilsMessengerEXT(aInstance, &info, nullptr, &_messenger);
}

void ValidationCapture::Destroy(VkInstance aInstance)
{
    if (_messenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT(aInstance, _messenger, nullptr);
        _messenger = VK_NULL_HANDLE;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL ValidationCapture::Callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* /*pCallbackData*/,
    void* pUserData)
{
    auto* self = static_cast<ValidationCapture*>(pUserData);
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        self->_hasErrors.store(true, std::memory_order_relaxed);
    else
        self->_hasWarnings.store(true, std::memory_order_relaxed);
    return VK_FALSE;
}

void ValidationCapture::DrawImGui() const
{
    if (_messenger == VK_NULL_HANDLE)
        return;

    const bool errors   = _hasErrors.load(std::memory_order_relaxed);
    const bool warnings = _hasWarnings.load(std::memory_order_relaxed);

    if (errors)
        ImGui::TextColored(ImVec4(1.f, 0.25f, 0.25f, 1.f), "Validation errors detected - check console");
    else if (warnings)
        ImGui::TextColored(ImVec4(1.f, 0.85f, 0.f, 1.f), "Validation warnings detected - check console");

    // if ((errors || warnings) && ImGui::Button("Clear##val"))
    // {
    //     _hasErrors.store(false, std::memory_order_relaxed);
    //     _hasWarnings.store(false, std::memory_order_relaxed);
    // }
}

} // namespace momo_vkDebug
