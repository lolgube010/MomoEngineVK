#include <vk/loader_stbi.h>

#include <stb_image.h>
#include <engine_main/engine.h>
#include <vk/images.h>
#include <vk/initializers.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>

std::optional<momo_vkGLTF::PendingTextureUpload>
momo_vkGLTF::load_image_stbi(fastgltf::Asset& aAsset, fastgltf::Image& aImage,
                               std::string_view aFilePath)
{
    // Decodes one GLTF image to RGBA pixels, allocates the VkImage and a host-visible staging
    // buffer, and copies the pixels in. Does NOT submit any GPU work — call site batches uploads.
    const auto& engine = VulkanEngine::Get();

    const char* imgName = nullptr;
#ifdef MOMOVK_ENABLE_DEBUG_NAMES
    const std::string temp = fmt::format("{}, Path: {}", aImage.name, aFilePath);
    imgName = temp.c_str();
#endif

    int width = 0, height = 0, nrChannels = 0;
    unsigned char* pixels = nullptr;

    std::visit(
        fastgltf::visitor{
            [&](const auto& anArg)
            {
                fmt::print(stderr, "load_image_stbi: unhandled source type '{}' for image '{}' at filepath '{}'\n",
                           typeid(anArg).name(), aImage.name, aFilePath);
            },
            [&](const fastgltf::sources::URI& aGLTFImageFilePath)
            {
                if (aGLTFImageFilePath.fileByteOffset != 0)
                {
                    fmt::print(stderr, "load_image_stbi: non-zero byte offset not supported (image '{}')\n", aImage.name);
                    return;
                }
                if (!aGLTFImageFilePath.uri.isLocalPath())
                {
                    fmt::print(stderr, "load_image_stbi: non-local URI not supported: '{}' (image '{}')\n",
                               aGLTFImageFilePath.uri.string(), aImage.name);
                    return;
                }
                const std::string path(aGLTFImageFilePath.uri.path().begin(),
                                       aGLTFImageFilePath.uri.path().end());
                pixels = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if (!pixels)
                {
                    fmt::print(stderr, "load_image_stbi: stbi failed to load '{}': {}\n", path, stbi_failure_reason());
                }
            },
            [&](fastgltf::sources::Array& anArray)
            {
                const auto* bytes = reinterpret_cast<const stbi_uc*>(anArray.bytes.data());
                pixels = stbi_load_from_memory(bytes, static_cast<int>(anArray.bytes.size()),
                                               &width, &height, &nrChannels, 4);
                if (!pixels)
                {
                    fmt::print(stderr, "load_image_stbi: stbi failed to decode embedded array for image '{}': {}\n",
                               aImage.name, stbi_failure_reason());
                }
            },
            [&](const fastgltf::sources::BufferView& aView)
            {
                const auto& bufferView = aAsset.bufferViews[aView.bufferViewIndex];
                auto& buffer = aAsset.buffers[bufferView.bufferIndex];
                std::visit(fastgltf::visitor{
                    [&](auto& anArg)
                    {
                        fmt::print(stderr, "load_image_stbi: unhandled buffer source type '{}' for image '{}'\n",
                                   typeid(anArg).name(), aImage.name);
                    },
                    [&](fastgltf::sources::Array& anArray)
                    {
                        const auto* bytes = reinterpret_cast<const stbi_uc*>(
                            anArray.bytes.data() + bufferView.byteOffset);
                        pixels = stbi_load_from_memory(bytes, static_cast<int>(bufferView.byteLength),
                                                       &width, &height, &nrChannels, 4);
                        if (!pixels)
                        {
                            fmt::print(stderr, "load_image_stbi: stbi failed to decode buffer view for image '{}': {}\n",
                                       aImage.name, stbi_failure_reason());
                        }
                    },
                    [&](fastgltf::sources::ByteView&)
                    {
                        fmt::print("BYTEVIEW RAN WHEN LOADING TEXTURE! MAKE SURE TO FIX THIS ASAP!");
                    }
                }, buffer.data);
            },
        },
        aImage.data);

    if (!pixels)
    {
        return std::nullopt;
    }

    const VkExtent3D extent = {
        .width  = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .depth  = 1
    };

    // TODO: KTX2 replacement — format from ktxTex->vkFormat, no TRANSFER_SRC, staging holds all mips.
    AllocatedImage image = engine.Create_Image(extent, VK_FORMAT_R8G8B8A8_UNORM,
                                               VK_IMAGE_USAGE_SAMPLED_BIT |
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                               imgName, /*mipmapped=*/true);

    const size_t dataSize = static_cast<size_t>(width) * height * 4;
    const AllocatedBuffer staging = engine.Create_Buffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                          VMA_MEMORY_USAGE_AUTO, imgName);
    memcpy(staging._info.pMappedData, pixels, dataSize);
    stbi_image_free(pixels);

    return PendingTextureUpload{._image = std::move(image), ._stagingBuffer = staging};
}
