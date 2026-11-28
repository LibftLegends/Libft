#include "basic.hpp"
#include "limits.hpp"
#include "../Basic/class_nullptr.hpp"

static int32_t ft_digit_value(char character)
{
    if (character >= '0' && character <= '9')
        return (character - '0');
    if (character >= 'a' && character <= 'z')
        return (character - 'a' + 10);
    if (character >= 'A' && character <= 'Z')
        return (character - 'A' + 10);
    return (-1);
}

int64_t ft_strtol(const char *input_string, char **end_pointer, int32_t numeric_base)
{
    const char *current_character = input_string;
    int32_t sign_value = 1;
    uint64_t accumulated_value = 0;
    int32_t digit_value;
    ft_bool digit_processed = FT_FALSE;
    uint64_t base_value;

    if (current_character == ft_nullptr)
    {
        if (end_pointer != ft_nullptr)
            *end_pointer = ft_nullptr;
        return (0L);
    }
    if (numeric_base != 0 && (numeric_base < 2 || numeric_base > 36))
    {
        if (end_pointer != ft_nullptr)
            *end_pointer = const_cast<char *>(input_string);
        return (0L);
    }
    while (*current_character == ' ' || (*current_character >= '\t'
                && *current_character <= '\r'))
        ++current_character;
    if (*current_character == '+' || *current_character == '-')
    {
        if (*current_character == '-')
            sign_value = -1;
        ++current_character;
    }
    if (numeric_base == 0)
    {
        if (*current_character == '0')
        {
            if (current_character[1] == 'x' || current_character[1] == 'X')
            {
                numeric_base = 16;
                current_character += 2;
            }
            else
                numeric_base = 8;
        }
        else
            numeric_base = 10;
    }
    else if (numeric_base == 16 && current_character[0] == '0'
             && (current_character[1] == 'x' || current_character[1] == 'X'))
        current_character += 2;
    base_value = static_cast<uint64_t>(numeric_base);
    uint64_t limit_value = static_cast<uint64_t>(FT_LLONG_MAX);
    if (sign_value < 0)
        ++limit_value;
    const uint64_t limit_division = limit_value / base_value;
    const uint64_t limit_remainder = limit_value % base_value;
    while ((digit_value = ft_digit_value(*current_character)) >= 0
            && digit_value < numeric_base)
    {
        uint64_t digit_as_unsigned;

        digit_as_unsigned = static_cast<uint64_t>(digit_value);
        digit_processed = FT_TRUE;
        if (accumulated_value > limit_division
                || (accumulated_value == limit_division
                    && digit_as_unsigned > limit_remainder))
        {
            accumulated_value = limit_value;
            ++current_character;
            while ((digit_value = ft_digit_value(*current_character)) >= 0
                    && digit_value < numeric_base)
                ++current_character;
            break ;
        }
        accumulated_value = accumulated_value * base_value
                          + digit_as_unsigned;
        ++current_character;
    }
    if (!digit_processed)
    {
        if (end_pointer != ft_nullptr)
            *end_pointer = const_cast<char *>(input_string);
        return (0L);
    }
    if (end_pointer)
        *end_pointer = const_cast<char *>(current_character);
    if (sign_value < 0)
    {
        if (accumulated_value == limit_value)
            return (FT_LLONG_MIN);
        return (-static_cast<int64_t>(accumulated_value));
    }
    return (static_cast<int64_t>(accumulated_value));
}
