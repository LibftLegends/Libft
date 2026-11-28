#include "math_internal.hpp"

int32_t math_is_unary_sign(const char *expression, int32_t index)
{
    while (index > 0)
    {
        index -= 1;
        const char token = expression[index];
        if ((token >= '0' && token <= '9') || token == ')')
            return (0);
        if (token == '(' || token == '+' || token == '-' || token == '*' || token == '/')
            return (1);
    }
    return (1);
}

int32_t math_process_sign(char *expression, int32_t &length, int32_t &index)
{
    if (index == 0 || expression[index - 1] < '0' || expression[index - 1] > '9')
        return (1);
    const char sign = expression[index];
    const int32_t right_index = index + 1;
    int32_t error = 0;
    index -= 1;
    const int32_t left = math_roll_convert_previous(expression, &index, &error);
    const int32_t right = math_roll_convert_next(expression, right_index, &error);
    if (error)
        return (1);
    int64_t result = left;
    if (sign == '+')
        result += right;
    else if (sign == '-')
        result -= right;
    else if (sign == '*')
        result *= right;
    else
    {
        if (right == 0)
            return (1);
        result /= right;
    }
    if (result < INT32_MIN || result > INT32_MAX)
        return (1);
    return (math_roll_replace_number(expression, length, index, right_index,
        static_cast<int32_t>(result)));
}
