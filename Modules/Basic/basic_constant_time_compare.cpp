#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"

ft_bool ft_constant_time_equal(const void *pointer1, const void *pointer2,
    ft_size_t size) noexcept
{
    const uint8_t *bytes_left;
    const uint8_t *bytes_right;
    uint8_t difference;
    ft_size_t index;

    if (size == 0)
        return (FT_TRUE);
    if (pointer1 == ft_nullptr || pointer2 == ft_nullptr)
        return (FT_FALSE);
    bytes_left = static_cast<const uint8_t *>(pointer1);
    bytes_right = static_cast<const uint8_t *>(pointer2);
    difference = 0;
    index = 0;
    while (index < size)
    {
        difference = static_cast<uint8_t>(difference
            | static_cast<uint8_t>(bytes_left[index] ^ bytes_right[index]));
        ++index;
    }
    return (static_cast<ft_bool>(difference == 0U));
}
