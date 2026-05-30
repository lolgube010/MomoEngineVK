#pragma once
#include <vk/gpu_types.h>
#include <fastgltf/types.hpp>
#include <optional>
#include <string_view>

namespace Momo_VkGLTF
{
    // CPU-side decode result + staging buffer ready for a batched GPU upload.
    // The VkImage is allocated but contains no data yet; Immediate_Submit fills it.
    struct PendingTextureUpload
    {
        AllocatedImage _image;
        AllocatedBuffer _stagingBuffer;
    };

    // Decodes one GLTF image to RGBA pixels, allocates the VkImage and a host-visible
    // staging buffer, and copies the pixels in. Does NOT submit any GPU work.
    // Replace this function when adding KTX2 / BCn support — the rest of load_gltf stays the same.
    std::optional<PendingTextureUpload> load_image_stbi(fastgltf::Asset& aAsset,
                                                         fastgltf::Image& aImage,
                                                         std::string_view aFilePath);
}
