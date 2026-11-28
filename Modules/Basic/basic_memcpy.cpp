#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include <cstring>

void* ft_memcpy(void* destination, const void* source, ft_size_t size)
{
    if (size == 0)
        return (destination);

    if (destination == ft_nullptr || source == ft_nullptr)
        return (ft_nullptr);
    if (destination == source)
        return (destination);

    const uintptr_t destination_address =
        reinterpret_cast<uintptr_t>(destination);
    const uintptr_t source_address = reinterpret_cast<uintptr_t>(source);

    // Forward overlap intentionally propagates bytes written earlier in the copy.
    if (size <= SIZE_MAX && (destination_address < source_address
            || destination_address - source_address >= size))
        return (std::memmove(destination, source,
            FT_UINT64_TO_NATIVE_SIZE_CAST(size)));

    unsigned char* destination_bytes = static_cast<unsigned char*>(destination);
    const unsigned char* source_bytes = static_cast<const unsigned char*>(source);
    ft_size_t index = 0;
    while (index < size)
    {
        destination_bytes[index] = source_bytes[index];
        ++index;
    }

    return (destination);
}
