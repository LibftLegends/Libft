#include "basic.hpp"
#include "utf8.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"

int32_t ft_utf8_validate(const char *string, ft_size_t length)
{
    ft_size_t index;
    uint32_t code_point;
    int32_t error_code;

    if (string == ft_nullptr && length != 0)
        return (FT_ERR_INVALID_POINTER);
    index = 0;
    while (index < length)
    {
        if (static_cast<uint8_t>(string[index]) < 0x80U)
            ++index;
        else
        {
            error_code = ft_utf8_next(string, length, &index, &code_point,
                    ft_nullptr);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
    }
    return (FT_ERR_SUCCESS);
}
