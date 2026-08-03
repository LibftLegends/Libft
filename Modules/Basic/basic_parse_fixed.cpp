#include "basic.hpp"
#include "limits.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"

static void parse_set_end(char **end_pointer, const char *position)
{
    if (end_pointer != ft_nullptr)
        *end_pointer = const_cast<char *>(position);
    return ;
}

int32_t ft_parse_uint32(const char *string, char **end_pointer,
    uint32_t *value_pointer)
{
    ft_size_t index;
    uint64_t accumulator;
    uint32_t digit;

    if (string == ft_nullptr || value_pointer == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    parse_set_end(end_pointer, string);
    index = 0;
    while (ft_isspace(static_cast<int32_t>(string[index])) != 0)
        index++;
    if (string[index] == '+' || string[index] == '-')
    {
        if (string[index] == '-')
            return (FT_ERR_INVALID_ARGUMENT);
        index++;
    }
    accumulator = 0;
    if (string[index] < '0' || string[index] > '9')
        return (FT_ERR_INVALID_ARGUMENT);
    while (string[index] >= '0' && string[index] <= '9')
    {
        digit = static_cast<uint32_t>(string[index] - '0');
        if (accumulator > (4294967295ULL - digit) / 10ULL)
        {
            parse_set_end(end_pointer, string + index);
            return (FT_ERR_OUT_OF_RANGE);
        }
        accumulator = accumulator * 10ULL + digit;
        index++;
    }
    *value_pointer = static_cast<uint32_t>(accumulator);
    parse_set_end(end_pointer, string + index);
    return (FT_ERR_SUCCESS);
}

int32_t ft_parse_int64(const char *string, char **end_pointer,
    int64_t *value_pointer)
{
    ft_size_t index;
    uint64_t accumulator;
    uint64_t magnitude_limit;
    uint32_t digit;
    ft_bool negative;

    if (string == ft_nullptr || value_pointer == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    parse_set_end(end_pointer, string);
    index = 0;
    while (ft_isspace(static_cast<int32_t>(string[index])) != 0)
        index++;
    negative = FT_FALSE;
    if (string[index] == '+' || string[index] == '-')
    {
        if (string[index] == '-')
            negative = FT_TRUE;
        index++;
    }
    if (string[index] < '0' || string[index] > '9')
        return (FT_ERR_INVALID_ARGUMENT);
    magnitude_limit = 9223372036854775807ULL;
    if (negative == FT_TRUE)
        magnitude_limit++;
    accumulator = 0;
    while (string[index] >= '0' && string[index] <= '9')
    {
        digit = static_cast<uint32_t>(string[index] - '0');
        if (accumulator > (magnitude_limit - digit) / 10ULL)
        {
            parse_set_end(end_pointer, string + index);
            return (FT_ERR_OUT_OF_RANGE);
        }
        accumulator = accumulator * 10ULL + digit;
        index++;
    }
    if (negative == FT_TRUE)
    {
        if (accumulator == 9223372036854775808ULL)
            *value_pointer = FT_LLONG_MIN;
        else
            *value_pointer = -static_cast<int64_t>(accumulator);
    }
    else
        *value_pointer = static_cast<int64_t>(accumulator);
    parse_set_end(end_pointer, string + index);
    return (FT_ERR_SUCCESS);
}
