#pragma once

class Vk_Debug_Info
{
public:
    void SetDebugInfo(const VkDevice* aDevice, uint64_t aObjectHandle, VkObjectType aObjectType, const char* a_pObjectName) const
    {
        if (_vkSetDebugUtilsObjectNameEXT)
        {
            VkDebugUtilsObjectNameInfoEXT nameInfo = {};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = aObjectType;
            nameInfo.objectHandle = aObjectHandle;
            nameInfo.pObjectName = a_pObjectName;

            _vkSetDebugUtilsObjectNameEXT(*aDevice, &nameInfo);
        }
    }

    PFN_vkSetDebugUtilsObjectNameEXT _vkSetDebugUtilsObjectNameEXT;
};