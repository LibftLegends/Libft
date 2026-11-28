#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include <cstring>

void *ft_memmem(const void *haystack, ft_size_t haystack_size,
    const void *needle, ft_size_t needle_size)
{
    const unsigned char *haystack_bytes;
    const unsigned char *needle_bytes;

    if (needle_size == 0)
        return (const_cast<void *>(haystack));
    if (haystack == ft_nullptr || needle == ft_nullptr)
        return (ft_nullptr);
    if (needle_size > haystack_size || haystack_size > SIZE_MAX)
        return (ft_nullptr);
    haystack_bytes = static_cast<const unsigned char *>(haystack);
    needle_bytes = static_cast<const unsigned char *>(needle);
    const ft_size_t prefix_size = needle_size - 1U;
    const unsigned char first_byte = needle_bytes[0];
    const unsigned char last_byte = needle_bytes[prefix_size];
    ft_size_t remaining = haystack_size - prefix_size;
    const unsigned char *candidate = haystack_bytes;
    while (remaining > 0U)
    {
        const unsigned char *anchor = candidate;
        unsigned char anchor_byte = first_byte;

        if (*anchor == first_byte)
        {
            anchor += prefix_size;
            anchor_byte = last_byte;
        }
        if (*anchor != anchor_byte)
        {
            const unsigned char *match = static_cast<const unsigned char *>(
                std::memchr(anchor, anchor_byte,
                    FT_UINT64_TO_NATIVE_SIZE_CAST(remaining)));
            if (match == ft_nullptr)
                return (ft_nullptr);
            const ft_size_t skipped = static_cast<ft_size_t>(match - anchor);
            candidate += skipped;
            remaining -= skipped;
            continue ;
        }
        if (needle_size <= 2U || std::memcmp(candidate + 1U, needle_bytes + 1U,
                FT_UINT64_TO_NATIVE_SIZE_CAST(needle_size - 2U)) == 0)
            return (const_cast<unsigned char *>(candidate));
        candidate += 1U;
        remaining -= 1U;
    }
    return (ft_nullptr);
}
