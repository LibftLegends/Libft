#include "basic.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"

ft_bool ft_is_power_of_two(ft_size_t value)
{
    if (value == 0)
        return (FT_FALSE);
    if ((value & (value - 1)) != 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

int32_t ft_align_up_checked(ft_size_t value, ft_size_t alignment,
    ft_size_t *result_pointer)
{
    ft_size_t remainder;
    ft_size_t increment;

    if (result_pointer == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (ft_is_power_of_two(alignment) != FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    remainder = value & (alignment - 1);
    if (remainder == 0)
    {
        *result_pointer = value;
        return (FT_ERR_SUCCESS);
    }
    increment = alignment - remainder;
    if (value > FT_SYSTEM_SIZE_MAX - increment)
        return (FT_ERR_OUT_OF_RANGE);
    *result_pointer = value + increment;
    return (FT_ERR_SUCCESS);
}
