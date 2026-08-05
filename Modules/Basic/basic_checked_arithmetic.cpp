#include "basic.hpp"
#include "limits.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"

int32_t ft_size_add_checked(ft_size_t left, ft_size_t right,
    ft_size_t *result_pointer)
{
    if (result_pointer == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (left > FT_SYSTEM_SIZE_MAX - right)
        return (FT_ERR_OUT_OF_RANGE);
    *result_pointer = left + right;
    return (FT_ERR_SUCCESS);
}

int32_t ft_size_multiply_checked(ft_size_t left, ft_size_t right,
    ft_size_t *result_pointer)
{
    if (result_pointer == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (left != 0 && right > FT_SYSTEM_SIZE_MAX / left)
        return (FT_ERR_OUT_OF_RANGE);
    *result_pointer = left * right;
    return (FT_ERR_SUCCESS);
}
