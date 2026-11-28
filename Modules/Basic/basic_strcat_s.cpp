#include <cstddef>
#include "basic.hpp"
#include "../Errno/errno.hpp"

static void zero_buffer(char *buffer, ft_size_t buffer_size)
{
    if (!buffer || buffer_size == 0)
        return ;
    ft_size_t index = 0;
    while (index < buffer_size)
    {
        buffer[index] = '\0';
        ++index;
    }
    return ;
}

int32_t ft_strcat_s(char *destination, ft_size_t destination_size, const char *source)
{
    if (!destination || !source)
    {
        zero_buffer(destination, destination_size);
        return (FT_ERR_INVALID_POINTER);
    }
    if (destination_size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_size_t destination_length = ft_strlen_size_t(destination);
    ft_size_t source_length = ft_strlen_size_t(source);
    if (destination_length + source_length + 1 > destination_size)
    {
        zero_buffer(destination, destination_size);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    (void)ft_memcpy(destination + destination_length, source, source_length);
    destination[destination_length + source_length] = '\0';
    return (FT_ERR_SUCCESS);
}
