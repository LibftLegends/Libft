#include "math_internal.hpp"

int32_t math_roll_execute_pm(char *expression, int32_t &length)
{
    int32_t index = 0;
    while (index < length && expression[index] != ')')
    {
        if ((expression[index] == '+' || expression[index] == '-')
            && !math_is_unary_sign(expression, index))
        {
            if (math_process_sign(expression, length, index))
                return (1);
        }
        else
            index += 1;
    }
    return (0);
}
