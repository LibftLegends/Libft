#include "math_internal.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include <charconv>

static int32_t math_roll_read_value(const char *expression, int32_t *error)
{
    while (*expression == ' ' || (*expression >= '\t' && *expression <= '\r'))
        expression += 1;
    const ft_bool negative = (*expression == '-');
    if (negative || *expression == '+')
        expression += 1;
    const uint64_t limit = static_cast<uint64_t>(INT32_MAX) + negative;
    uint64_t magnitude = 0U;
    while (*expression >= '0' && *expression <= '9')
    {
        magnitude = magnitude * 10U + static_cast<uint64_t>(*expression - '0');
        if (magnitude > limit)
        {
            *error = 1;
            return (0);
        }
        expression += 1;
    }
    int64_t value = static_cast<int64_t>(magnitude);
    if (negative)
        value = -value;
    return (static_cast<int32_t>(value));
}

int32_t math_check_string_number(char *expression)
{
    if (*expression == '+' || *expression == '-')
        expression += 1;
    if (*expression < '0' || *expression > '9')
        return (0);
    while (*expression >= '0' && *expression <= '9')
        expression += 1;
    return (*expression == '\0');
}

void math_free_parse(char **parsed_strings)
{
    if (parsed_strings != ft_nullptr)
    {
        ft_size_t index = 0U;
        while (parsed_strings[index] != ft_nullptr)
        {
            cma_free(parsed_strings[index]);
            index += 1U;
        }
        cma_free(parsed_strings);
    }
    return ;
}

int32_t math_roll_convert_previous(char *expression, int32_t *index, int32_t *error)
{
    while (*index > 0 && expression[*index] >= '0' && expression[*index] <= '9')
        *index -= 1;
    if (expression[*index] == '-' || expression[*index] == '+')
    {
        if (*index > 0 && ((expression[*index - 1] >= '0' && expression[*index - 1] <= '9')
                || expression[*index - 1] == ')'))
            *index += 1;
    }
    else if (expression[*index] < '0' || expression[*index] > '9')
        *index += 1;
    return (math_roll_read_value(expression + *index, error));
}

int32_t math_roll_convert_next(char *expression, int32_t index, int32_t *error)
{
    return (math_roll_read_value(expression + index, error));
}

static int32_t math_roll_format_number(int32_t value, char *digits)
{
    constexpr ft_size_t DECIMAL_CAPACITY = 11U;
    const auto result = std::to_chars(digits, digits + DECIMAL_CAPACITY, value);
    if (result.ec != std::errc{})
        return (0);
    return (static_cast<int32_t>(result.ptr - digits));
}

int32_t math_roll_itoa(int32_t value, int32_t *index, char *expression)
{
    char digits[11];
    const int32_t count = math_roll_format_number(value, digits);
    if (count == 0)
        return (1);
    ft_memcpy(expression + *index, digits, static_cast<ft_size_t>(count));
    *index += count;
    return (0);
}

int32_t math_roll_replace_number(char *expression, int32_t &length,
    int32_t &index, int32_t right_index, int32_t value)
{
    int32_t operand_end = right_index;
    if (expression[operand_end] == '+' || expression[operand_end] == '-')
        operand_end += 1;
    if (expression[operand_end] < '0' || expression[operand_end] > '9')
        return (1);
    while (expression[operand_end] >= '0' && expression[operand_end] <= '9')
        operand_end += 1;

    char digits[11];
    const int32_t count = math_roll_format_number(value, digits);
    if (count == 0 || count > operand_end - index)
        return (1);
    ft_memcpy(expression + index, digits, static_cast<ft_size_t>(count));
    index += count;
    ft_memmove(expression + index, expression + operand_end,
        static_cast<ft_size_t>(length - operand_end) + 1U);
    length -= operand_end - index;
    return (0);
}
