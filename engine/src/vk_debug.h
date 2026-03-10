#pragma once

class Vk_Debug_Info
{
public:
    void Init(const VkInstance& aVkInstance) 
    { 
        // momo debug stuff
        g_TotalAllocatedBytes = 0;
        g_TotalFreedBytes = 0;
        g_AllocationCount = 0;

        _callbacks.pUserData = nullptr;
        _callbacks.pfnAllocate = MyAllocateCallback;
        _callbacks.pfnFree = MyFreeCallback;
    }

    static void SetDebugInfo(const VkDevice* aDevice, const uint64_t aObjectHandle, const VkObjectType aObjectType, const char* a_pObjectName)
    {
        VkDebugUtilsObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = aObjectType;
        nameInfo.objectHandle = aObjectHandle;
        nameInfo.pObjectName = a_pObjectName;

        vkSetDebugUtilsObjectNameEXT(*aDevice, &nameInfo);
    }


    static void VKAPI_PTR MyAllocateCallback(VmaAllocator allocator, uint32_t memoryType, VkDeviceMemory vkMem, VkDeviceSize size, void* pUserData)
    {
        g_TotalAllocatedBytes += size;
        g_AllocationCount++;

        // Optional: you can also log memory type, handle etc.
        // printf("Allocated %llu B  (type %u)\n", size, memoryType);
    }

    static void VKAPI_PTR MyFreeCallback(VmaAllocator allocator, uint32_t memoryType, VkDeviceMemory vkMem, VkDeviceSize size, void* pUserData)
    {
        g_TotalFreedBytes += size;
        g_AllocationCount--;

        // printf("Freed %llu B  (type %u)\n", size, memoryType);
    }

private:
    VmaDeviceMemoryCallbacks _callbacks = {};
    inline static uint64_t g_TotalAllocatedBytes;
    inline static uint64_t g_TotalFreedBytes;
    inline static uint32_t g_AllocationCount;
};