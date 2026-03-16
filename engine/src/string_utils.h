#pragma once 
#include <string>
#include <string_view>
#include <cstdint>

namespace momo_stringUtils
{
    // FNV-1a 32bit hashing algorithm.
    constexpr uint32_t fnv1a_32(char const* s, const std::size_t count)
    {
        return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
    }

    constexpr size_t const_strlen(const char* s)
    {
        size_t size = 0;
        while (s[size])
        {
            size++;
        };
        return size;
    }

    struct StringHash
    {
        uint32_t computedHash;

        explicit constexpr StringHash(const uint32_t aHash) noexcept : computedHash(aHash) {}

        explicit constexpr StringHash(const char* s) noexcept : computedHash(0) { computedHash = fnv1a_32(s, const_strlen(s)); }
        constexpr StringHash(const char* s, const std::size_t count) noexcept : computedHash(0) { computedHash = fnv1a_32(s, count); }
        explicit constexpr StringHash(const std::string_view s) noexcept : computedHash(0) { computedHash = fnv1a_32(s.data(), s.size()); }
        StringHash(const StringHash& other) = default;

        explicit constexpr operator uint32_t() const noexcept { return computedHash; }
    };
}
