#include "basic.hpp"
#include "utf8.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"

int32_t ft_utf8_validate(const char *string, ft_size_t length)
{
    ft_size_t index;
    uint32_t code_point;
    ft_size_t sequence_length;
    int32_t error_code;

    if (string == ft_nullptr && length != 0)
        return (FT_ERR_INVALID_POINTER);
    index = 0;
    while (index < length)
    {
        code_point = 0;
        sequence_length = 0;
        error_code = ft_utf8_next(string, length, &index, &code_point,
                &sequence_length);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    return (FT_ERR_SUCCESS);
}
