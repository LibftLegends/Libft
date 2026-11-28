#include <cstddef>
#include "basic.hpp"
#include "utf8.hpp"
#include "../Errno/errno.hpp"

int32_t ft_utf8_count(const char *string, ft_size_t *code_point_count_pointer)
{
    int32_t error_code;

    if (!string || !code_point_count_pointer)
        return (FT_ERR_INVALID_POINTER);
    ft_size_t length = ft_strlen_size_t(string);
    ft_size_t index = 0;
    ft_size_t count = 0;
    while (index < length)
    {
        if (static_cast<uint8_t>(string[index]) < 0x80U)
            ++index;
        else
        {
            uint32_t code_point;

            error_code = ft_utf8_next(string, length, &index, &code_point, ft_nullptr);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        ++count;
    }
    *code_point_count_pointer = count;
    return (FT_ERR_SUCCESS);
}
