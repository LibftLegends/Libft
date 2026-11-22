#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"

ft_size_t ft_strlcpy(char *destination, const char *source, ft_size_t buffer_size)
{
    if (!source)
        return (0);

    ft_size_t source_length = ft_strlen_size_t(source);

    if (buffer_size > 0 && destination == ft_nullptr)
        return (0);

    if (destination && buffer_size > 0)
    {
        ft_size_t copy_length;
        if (source_length >= buffer_size)
            copy_length = buffer_size - 1;
        else
            copy_length = source_length;
        (void)ft_memcpy(destination, source, copy_length);
        destination[copy_length] = '\0';
    }

    return (source_length);
}
