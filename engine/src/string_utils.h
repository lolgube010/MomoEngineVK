#pragma once 
#include <string>
#include <string_view>
#include <cstdint>

namespace momo_stringUtils
{
    // FNV-1a 32bit hashing algorithm.
    // ReSharper disable once CppInconsistentNaming
    constexpr uint32_t fnv1a_32(char const* aS, const std::size_t aCount)
    {
        return ((aCount ? fnv1a_32(aS, aCount - 1) : 2166136261u) ^ aS[aCount]) * 16777619u;
    }

    constexpr size_t const_strlen(const char* aS)
    {
        size_t size = 0;
        while (aS[size])
        {
            size++;
        };
        return size;
    }

    struct StringHash
    {
        uint32_t _computedHash;

        explicit constexpr StringHash(const uint32_t aHash) noexcept : _computedHash(aHash) {}

        explicit constexpr StringHash(const char* aS) noexcept : _computedHash(0) { _computedHash = fnv1a_32(aS, const_strlen(aS)); }
        constexpr StringHash(const char* aS, const std::size_t aCount) noexcept : _computedHash(0) { _computedHash = fnv1a_32(aS, aCount); }
        explicit constexpr StringHash(const std::string_view aS) noexcept : _computedHash(0) { _computedHash = fnv1a_32(aS.data(), aS.size()); }
        StringHash(const StringHash& aOther) = default;

        explicit constexpr operator uint32_t() const noexcept { return _computedHash; }
    };
}
