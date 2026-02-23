#pragma once

class Vk_Debug_Info
{
public:
    void Init(const VkInstance& aVkInstance) 
    { 
        _vkSetDebugUtilsObjectNameExt = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(aVkInstance, "vkSetDebugUtilsObjectNameEXT"));
    }

    void SetDebugInfo(const VkDevice* aDevice, const uint64_t aObjectHandle, const VkObjectType aObjectType, const char* a_pObjectName) const
    {
        if (_vkSetDebugUtilsObjectNameExt)
        {
            VkDebugUtilsObjectNameInfoEXT nameInfo = {};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = aObjectType;
            nameInfo.objectHandle = aObjectHandle;
            nameInfo.pObjectName = a_pObjectName;

            _vkSetDebugUtilsObjectNameExt(*aDevice, &nameInfo);
        }
    }

private:
    PFN_vkSetDebugUtilsObjectNameEXT _vkSetDebugUtilsObjectNameExt = {};
};