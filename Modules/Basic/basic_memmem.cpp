#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"

void *ft_memmem(const void *haystack, ft_size_t haystack_size,
    const void *needle, ft_size_t needle_size)
{
    const unsigned char *haystack_bytes;
    const unsigned char *needle_bytes;
    ft_size_t start_index;
    ft_size_t compare_index;

    if (needle_size == 0)
        return (const_cast<void *>(haystack));
    if (haystack == ft_nullptr || needle == ft_nullptr)
        return (ft_nullptr);
    if (needle_size > haystack_size)
        return (ft_nullptr);
    haystack_bytes = static_cast<const unsigned char *>(haystack);
    needle_bytes = static_cast<const unsigned char *>(needle);
    start_index = 0;
    while (start_index + needle_size <= haystack_size)
    {
        compare_index = 0;
        while (compare_index < needle_size
            && haystack_bytes[start_index + compare_index]
                == needle_bytes[compare_index])
            compare_index++;
        if (compare_index == needle_size)
            return (const_cast<unsigned char *>(
                    haystack_bytes + start_index));
        start_index++;
    }
    return (ft_nullptr);
}
