#pragma once
#include <vk/gpu_types.h>
#include <functional>
#include <span>

class GpuResources
{
public:
    void Init(VkDevice aDevice, VmaAllocator aAllocator, VkQueue aGraphicsQueue, uint32_t aGraphicsQueueFamily);
    void Cleanup(VkDevice aDevice);

    void Immediate_Submit(const std::function<void(VkCommandBuffer aCmd)>& aFunction) const;

    [[nodiscard]] AllocatedImage Create_Image(VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, const char* aName, bool aMipmapped = false) const;
    [[nodiscard]] AllocatedImage Create_Image(const void* aData, VkExtent3D aSize, VkFormat aFormat, VkImageUsageFlags aUsage, const char* aName, bool aMipmapped = false) const;
    [[nodiscard]] AllocatedBuffer Create_Buffer(size_t anAllocSize, VkBufferUsageFlags aUsage, VmaMemoryUsage aMemoryUsage, const char* aName) const;
    void Destroy_Image(const AllocatedImage& aImg) const;
    void Destroy_Buffer(const AllocatedBuffer& aBuffer) const;
    GPUMeshBuffers UploadMesh(std::span<uint32_t> aIndices, std::span<Vertex> aVertices, const char* aMeshName) const;

    VkCommandBuffer GetImmCommandBuffer() const { return _immCommandBuffer; }

private:
    VkDevice     _device{};
    VmaAllocator _allocator{};
    VkQueue      _graphicsQueue{};

    VkFence         _immFence{};
    VkCommandBuffer _immCommandBuffer{};
    VkCommandPool   _immCommandPool{};

    static std::string Get_Buffer_Usage_Flag_String(VkBufferUsageFlags aUsageFlag);
};
