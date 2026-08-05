#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"

void *ft_memswap(void *left, void *right, ft_size_t size)
{
    unsigned char *left_bytes;
    unsigned char *right_bytes;
    unsigned char temporary;
    ft_size_t index;

    if (size == 0)
        return (left);
    if (left == ft_nullptr || right == ft_nullptr)
        return (ft_nullptr);
    if (left == right)
        return (left);
    left_bytes = static_cast<unsigned char *>(left);
    right_bytes = static_cast<unsigned char *>(right);
    index = 0;
    while (index < size)
    {
        temporary = left_bytes[index];
        left_bytes[index] = right_bytes[index];
        right_bytes[index] = temporary;
        index++;
    }
    return (left);
}
