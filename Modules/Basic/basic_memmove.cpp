#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include <cstring>

void *ft_memmove(void *destination, const void *source, ft_size_t size)
{
    constexpr ft_size_t FORWARD_COPY_BLOCK_BYTES = 8192U;

    if (destination == source || size == 0)
        return (destination);

    if (destination == ft_nullptr || source == ft_nullptr)
        return (ft_nullptr);

    unsigned char *destination_bytes = static_cast<unsigned char *>(destination);
    const unsigned char *source_bytes = static_cast<const unsigned char *>(source);
    const uintptr_t destination_address = reinterpret_cast<uintptr_t>(destination);
    const uintptr_t source_address = reinterpret_cast<uintptr_t>(source);

    if (size <= SIZE_MAX)
    {
        /* Small forward moves keep the native copy on its fast path when
         * an overlapping range has a large length and a short offset. */
        if (destination_address < source_address
            && source_address - destination_address < size)
        {
            while (size > FORWARD_COPY_BLOCK_BYTES)
            {
                std::memmove(destination_bytes, source_bytes, FORWARD_COPY_BLOCK_BYTES);
                destination_bytes += FORWARD_COPY_BLOCK_BYTES;
                source_bytes += FORWARD_COPY_BLOCK_BYTES;
                size -= FORWARD_COPY_BLOCK_BYTES;
            }
        }
        (void)std::memmove(destination_bytes, source_bytes,
            FT_UINT64_TO_NATIVE_SIZE_CAST(size));
        return (destination);
    }

    if (destination_address < source_address)
    {
        ft_size_t index = 0;
        while (index < size)
        {
            destination_bytes[index] = source_bytes[index];
            ++index;
        }
    }
    else
    {
        ft_size_t index = size;
        while (index > 0)
        {
            destination_bytes[index - 1] = source_bytes[index - 1];
            --index;
        }
    }

    return (destination);
}
