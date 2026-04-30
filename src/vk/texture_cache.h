#pragma once
#include <vk/gpu_types.h>
#include <unordered_set>
#include <unordered_map>
#include <span>

struct TextureCache
{
    struct ViewSamplerKey
    {
        VkImageView _imageView;
        VkSampler   _sampler;
        bool operator==(const ViewSamplerKey&) const = default;
    };
    struct ViewSamplerHash
    {
        size_t operator()(const ViewSamplerKey& aKey) const noexcept
        {
            const size_t h1 = std::hash<uint64_t>{}(std::bit_cast<uint64_t>(aKey._imageView));
            const size_t h2 = std::hash<uint64_t>{}(std::bit_cast<uint64_t>(aKey._sampler));
            return h1 ^ (h2 * 2654435761u + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
        }
    };

    std::vector<VkDescriptorImageInfo> _cache;
    std::unordered_set<uint32_t>       _freeSlots;
    std::unordered_set<VkImageView>    _engineDefaultImages;
    std::unordered_map<ViewSamplerKey, uint32_t, ViewSamplerHash> _lookup;
    bool _dirty = true;

    TextureID AddTexture(const VkImageView& aImage, VkSampler aSampler);
    void MarkEngineImage(VkImageView aView);
    void FreeTextures(std::span<const TextureID> aIDs, const VkDescriptorImageInfo& aFallback);

    size_t CacheSize() const         { return _cache.size(); }
    size_t EngineDefaultCount() const { return _engineDefaultImages.size(); }
    size_t FreeSlotCount() const     { return _freeSlots.size(); }
};
