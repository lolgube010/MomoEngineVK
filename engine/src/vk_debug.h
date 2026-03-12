#pragma once
#include "vk_initializers.h"

// 2. Add these helper macros to generate unique variable names
#define MOMO_CONCAT_IMPL(x, y) x##y
#define MOMO_MACRO_CONCAT(x, y) MOMO_CONCAT_IMPL(x, y)

// 3. Define the scoping macros
#ifdef _DEBUG
#define MOMO_VK_SCOPED_CMD_LABEL(cmd, name, ...) momo_vkDebug::ScopedDebugLabelCmdBuff MOMO_MACRO_CONCAT(vk_label_, __LINE__)(cmd, name, ##__VA_ARGS__)
#define MOMO_VK_SCOPED_QUEUE_LABEL(queue, name, ...) momo_vkDebug::ScopedDebugLabelQueue MOMO_MACRO_CONCAT(vk_label_, __LINE__)(queue, name, ##__VA_ARGS__)
#define MOMO_VK_SET_DEBUG_NAME(device, objType, handle, fmtString, ...) momo_vkDebug::Set_Debug_Name(device, objType, handle, fmtString, ##__VA_ARGS__)
#else
// In release, the macro resolves to literal nothing.
// Arguments are NEVER evaluated.
#define MOMO_VK_SCOPED_CMD_LABEL(cmd, name, ...)
#define MOMO_VK_SCOPED_QUEUE_LABEL(queue, name, ...)
#define MOMO_VK_SET_DEBUG_NAME(device, objType, handle, fmtString, ...)
#endif

#ifdef _DEBUG // DON'T USE! USE THE MACROS ABOVE INSTEAD!
namespace momo_vkDebug
{
    template <typename T, typename... Args>
    static void Set_Debug_Name(const VkDevice aDevice, const VkObjectType aObjectType, T aHandle, fmt::string_view aFmtString, Args&&... aArgs)
    {
    // Check if we are in a debug build.
    // MSVC defines _DEBUG by default in Debug, and NDEBUG in Release.

        // 1. Safety check! If the debug extension wasn't loaded
        if (!vkSetDebugUtilsObjectNameEXT)
            return;

        std::string finalName = fmt::vformat(aFmtString, fmt::make_format_args(aArgs...));
        VkDebugUtilsObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = aObjectType;

        // ReSharper disable once CppCStyleCast
        nameInfo.objectHandle = (uint64_t)(aHandle);
        nameInfo.pObjectName = finalName.c_str();

        vkSetDebugUtilsObjectNameEXT(aDevice, &nameInfo);
    }

    struct ScopedDebugLabelCmdBuff
    {
        VkCommandBuffer cmd;
        ScopedDebugLabelCmdBuff(const VkCommandBuffer aCmd, const char* aName, const glm::vec4 aColor = glm::vec4(1.f, 1.f, 1.f, 1.f)) : cmd(aCmd)
        {
            const VkDebugUtilsLabelEXT labelInfo = vkInit::debug_label(aName, aColor);
            vkCmdBeginDebugUtilsLabelEXT(aCmd, &labelInfo);
        }
        ~ScopedDebugLabelCmdBuff()
        {
            vkCmdEndDebugUtilsLabelEXT(cmd);
        }
    };

    struct ScopedDebugLabelQueue
    {
        VkQueue queue;
        ScopedDebugLabelQueue(const VkQueue aQueue, const char* aName, const glm::vec4 aColor = glm::vec4(1.f, 1.f, 1.f, 1.f)) : queue(aQueue)
        {
            const VkDebugUtilsLabelEXT labelInfo = vkInit::debug_label(aName, aColor);
            vkQueueBeginDebugUtilsLabelEXT(queue, &labelInfo);
        }
        ~ScopedDebugLabelQueue()
        {
            vkQueueEndDebugUtilsLabelEXT(queue);
        }
    };

    static void BeginAnnotationCmdBuff(const VkCommandBuffer aCmd, const char* aName, const glm::vec4 aColor = glm::vec4(1.f, 1.f, 1.f, 1.f))
    {
        const VkDebugUtilsLabelEXT labelInfo = vkInit::debug_label(aName, aColor);
        vkCmdBeginDebugUtilsLabelEXT(aCmd, &labelInfo);
    }

    static void EndAnnotationCmdBuff(const VkCommandBuffer aCmd) { vkCmdEndDebugUtilsLabelEXT(aCmd); }

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
}
#endif