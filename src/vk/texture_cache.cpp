#include <vk/texture_cache.h>

TextureID TextureCache::AddTexture(const VkImageView& aImage, const VkSampler aSampler)
{
    const ViewSamplerKey key{._imageView = aImage, ._sampler = aSampler};

    if (const auto it = _lookup.find(key); it != _lookup.end())
    {
        return TextureID{it->second};
    }

    const VkDescriptorImageInfo info{
        .sampler     = aSampler,
        .imageView   = aImage,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    uint32_t idx;
    if (!_freeSlots.empty())
    {
        idx = *_freeSlots.begin();
        _freeSlots.erase(_freeSlots.begin());
        _cache[idx] = info;
    }
    else
    {
        idx = static_cast<uint32_t>(_cache.size());
        _cache.push_back(info);
    }

    _lookup.emplace(key, idx);
    _dirty = true;
    return TextureID{idx};
}

void TextureCache::MarkEngineImage(const VkImageView aView)
{
    _engineDefaultImages.insert(aView);
}

void TextureCache::FreeTextures(const std::span<const TextureID> aIDs,
                                 const VkDescriptorImageInfo& aFallback)
{
    for (const auto [Index] : aIDs)
    {
        if (_engineDefaultImages.contains(_cache[Index].imageView)) continue;
        if (_freeSlots.contains(Index))                              continue;

        _lookup.erase(ViewSamplerKey{._imageView = _cache[Index].imageView,
                                     ._sampler   = _cache[Index].sampler});
        _cache[Index] = aFallback;
        _freeSlots.insert(Index);
        _dirty = true;
    }
}
