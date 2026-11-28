#include "dumb_console.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../GetNextLine/get_next_line.hpp"
#include <iostream>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

void ft_print_char(char character)
{
    std::cout << character;
    return ;
}

void ft_print_string(const char *string)
{
    std::cout << string;
    return ;
}

void ft_print_line(const char *string)
{
    std::cout << string << std::endl;
    return ;
}

int32_t ft_read_line(char *buffer, ft_size_t buffer_size)
{
    if (!buffer || buffer_size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    char *raw_line = get_next_line(0, buffer_size);
    if (!raw_line)
        return (FT_ERR_END_OF_FILE);
    ft_size_t length = ft_strlen_size_t(raw_line);
    if (length > 0 && raw_line[length - 1] == '\n')
        --length;
    if (length >= buffer_size)
    {
        cma_free(raw_line);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    (void)ft_memcpy(buffer, raw_line, length);
    buffer[length] = '\0';
    cma_free(raw_line);
    return (FT_ERR_SUCCESS);
}
