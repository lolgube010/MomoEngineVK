#include <vk/gpu_resources.h>
#include <vk/images.h>
#include <vk/initializers.h>
#include <vk/debug.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

void GpuResources::Init(const VkDevice aDevice, const VmaAllocator aAllocator, const VkQueue aGraphicsQueue, const uint32_t aGraphicsQueueFamily)
{
    _device        = aDevice;
    _allocator     = aAllocator;
    _graphicsQueue = aGraphicsQueue;

    const VkCommandPoolCreateInfo commandPoolInfo = Momo_VkInit::command_pool_create_info(aGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_COMMAND_POOL, _immCommandPool, "_Command Pool Immediate");

    const VkCommandBufferAllocateInfo cmdAllocInfo = Momo_VkInit::command_buffer_allocate_info(_immCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_COMMAND_BUFFER, _immCommandBuffer, "_Command Buffer Immediate");

    const VkFenceCreateInfo fenceCreateInfo = Momo_VkInit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_FENCE, _immFence, "_Fence Immediate");
}

void GpuResources::Cleanup(const VkDevice aDevice)
{
    vkDestroyFence(aDevice, _immFence, nullptr);
    vkDestroyCommandPool(aDevice, _immCommandPool, nullptr);
}

void GpuResources::Immediate_Submit(const std::function<void(VkCommandBuffer aCmd)>& aFunction) const
{
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    const VkCommandBufferBeginInfo cmdBeginInfo = Momo_VkInit::command_buffer_begin_info(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(_immCommandBuffer, &cmdBeginInfo));
    aFunction(_immCommandBuffer);
    VK_CHECK(vkEndCommandBuffer(_immCommandBuffer));

    const VkCommandBufferSubmitInfo cmdInfo = Momo_VkInit::command_buffer_submit_info(_immCommandBuffer);
    const VkSubmitInfo2 submit = Momo_VkInit::submit_info(&cmdInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));
    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

AllocatedImage GpuResources::Create_Image(const VkExtent3D aSize, const VkFormat aFormat, const VkImageUsageFlags aUsage, const char* aName, const bool aMipmapped) const
{
    AllocatedImage newImage;
    newImage._imageFormat = aFormat;
    newImage._imageExtent = aSize;

    VkImageCreateInfo img_Info = Momo_VkInit::image_create_info(aFormat, aUsage, aSize);
    if (aMipmapped)
    {
        img_Info.mipLevels = static_cast<uint32_t>(
            std::floor(std::log2(std::max(aSize.width, aSize.height)))) + 1;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage         = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vmaCreateImage(_allocator, &img_Info, &allocInfo,
                             &newImage._image, &newImage._allocation, nullptr));

    VkImageAspectFlags aspectFlag = (aFormat == VK_FORMAT_D32_SFLOAT)
        ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo view_Info = Momo_VkInit::imageview_create_info(aFormat, newImage._image, aspectFlag);
    view_Info.subresourceRange.levelCount = img_Info.mipLevels;
    VK_CHECK(vkCreateImageView(_device, &view_Info, nullptr, &newImage._imageView));

    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE,      newImage._image,     "_Image Name: {}",     aName);
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_IMAGE_VIEW, newImage._imageView, "_ImageView Name: {}", aName);
    vmaSetAllocationName(_allocator, newImage._allocation, aName);
    return newImage;
}

AllocatedImage GpuResources::Create_Image(const void* aData, const VkExtent3D aSize,
                                           const VkFormat aFormat, const VkImageUsageFlags aUsage,
                                           const char* aName, const bool aMipmapped) const
{
    const size_t data_Size = static_cast<size_t>(aSize.depth) * aSize.width * aSize.height * 4;

    const char* uploadBufferName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string temp = fmt::format("Upload, {}", aName);
    uploadBufferName = temp.c_str();
#endif
    const AllocatedBuffer uploadBuffer = Create_Buffer(data_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                        VMA_MEMORY_USAGE_AUTO, uploadBufferName);
    memcpy(uploadBuffer._info.pMappedData, aData, data_Size);

    const AllocatedImage new_Image = Create_Image(aSize, aFormat,
                                                   aUsage | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT, aName, aMipmapped);

    Immediate_Submit([&](const VkCommandBuffer aCmd)
    {
        Momo_VkUtil::transition_image(aCmd, new_Image._image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, new_Image._imageFormat);
        const VkBufferImageCopy copyRegion = Momo_VkInit::buffer_image_copy(aSize, new_Image._imageFormat);
        vkCmdCopyBufferToImage(aCmd, uploadBuffer._buffer, new_Image._image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
        if (aMipmapped)
        {
            Momo_VkUtil::generate_mipmaps(aCmd, new_Image._image,
                VkExtent2D{.width = new_Image._imageExtent.width, .height = new_Image._imageExtent.height},
                new_Image._imageFormat);
        }
        else
        {
            Momo_VkUtil::transition_image(aCmd, new_Image._image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, new_Image._imageFormat);
        }
    });

    Destroy_Buffer(uploadBuffer);
    return new_Image;
}

void GpuResources::Destroy_Image(const AllocatedImage& aImg) const
{
    vkDestroyImageView(_device, aImg._imageView, nullptr);
    vmaDestroyImage(_allocator, aImg._image, aImg._allocation);
}

AllocatedBuffer GpuResources::Create_Buffer(const size_t anAllocSize, const VkBufferUsageFlags aUsage,
                                             const VmaMemoryUsage aMemoryUsage, const char* aName) const
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size  = anAllocSize;
    bufferInfo.usage = aUsage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = aMemoryUsage;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                         VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer newBuffer;
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo,
                              &newBuffer._buffer, &newBuffer._allocation, &newBuffer._info));

#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    const std::string buffName = fmt::format("_Buffer {}, {}", Get_Buffer_Usage_Flag_String(aUsage), aName);
    MOMO_VK_SET_DEBUG_NAME(_device, VK_OBJECT_TYPE_BUFFER, newBuffer._buffer, buffName);
    vmaSetAllocationName(_allocator, newBuffer._allocation, buffName.c_str());
#endif
    return newBuffer;
}

void GpuResources::Destroy_Buffer(const AllocatedBuffer& aBuffer) const
{
    vmaDestroyBuffer(_allocator, aBuffer._buffer, aBuffer._allocation);
}

GPUMeshBuffers GpuResources::UploadMesh(const std::span<uint32_t> aIndices,
                                         const std::span<Vertex> aVertices,
                                         const char* aMeshName) const
{
    const size_t vertexBufferSize = aVertices.size() * sizeof(Vertex);
    const size_t indexBufferSize  = aIndices.size()  * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    const char* bufferName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    std::string bufferNameStr = fmt::format("(Vertex, BDA), {}", aMeshName);
    bufferName = bufferNameStr.c_str();
#endif
    newSurface._vertexBuffer = Create_Buffer(vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_AUTO, bufferName);

    const VkBufferDeviceAddressInfo deviceAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = newSurface._vertexBuffer._buffer
    };
    newSurface._vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    bufferNameStr = fmt::format("{}", aMeshName);
    bufferName = bufferNameStr.c_str();
#endif
    newSurface._indexBuffer = Create_Buffer(indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO, bufferName);

    const AllocatedBuffer staging = Create_Buffer(vertexBufferSize + indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, bufferName);

    memcpy(staging._info.pMappedData, aVertices.data(), vertexBufferSize);
    memcpy(static_cast<char*>(staging._info.pMappedData) + vertexBufferSize, aIndices.data(), indexBufferSize);

    Immediate_Submit([&](const VkCommandBuffer aCmd)
    {
        VkBufferCopy vertexCopy{.size = vertexBufferSize};
        vkCmdCopyBuffer(aCmd, staging._buffer, newSurface._vertexBuffer._buffer, 1, &vertexCopy);
        VkBufferCopy indexCopy{.srcOffset = vertexBufferSize, .size = indexBufferSize};
        vkCmdCopyBuffer(aCmd, staging._buffer, newSurface._indexBuffer._buffer, 1, &indexCopy);
    });

    Destroy_Buffer(staging);
    return newSurface;
}

std::string GpuResources::Get_Buffer_Usage_Flag_String(const VkBufferUsageFlags aUsageFlag)
{
    std::string typeName;
    if (aUsageFlag & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)          typeName += "Index_";
    if (aUsageFlag & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)         typeName += "Vertex_";
    if (aUsageFlag & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)        typeName += "Uniform_";
    if (aUsageFlag & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)        typeName += "Storage_";
    if (aUsageFlag & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)          typeName += "TransferSrc(Staging)_";
    if (aUsageFlag & VK_BUFFER_USAGE_TRANSFER_DST_BIT)          typeName += "TransferDst_";
    if (aUsageFlag & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)  typeName += "UniformTexel_";
    if (aUsageFlag & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)  typeName += "StorageTexel_";
    if (aUsageFlag & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)       typeName += "Indirect_";
    if (aUsageFlag & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) typeName += "DeviceAddress_";
    if (aUsageFlag & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR) typeName += "AccelStruct_";
    if (aUsageFlag & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR) typeName += "SBT_";
    if (!typeName.empty()) typeName.pop_back();
    else typeName = "Unknown";
    return typeName;
}
