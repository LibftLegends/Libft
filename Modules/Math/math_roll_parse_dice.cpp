#include "math_internal.hpp"
#include "../RNG/rng_internal.hpp"

int32_t math_roll_execute_dice(char *expression, int32_t &length)
{
    int32_t index = 0;
    while (index < length && expression[index] != ')')
    {
        if (expression[index] != 'd')
        {
            index += 1;
            continue ;
        }
        int32_t count = 1;
        int32_t error = 0;
        int32_t start = index;
        if (index > 0 && expression[index - 1] >= '0' && expression[index - 1] <= '9')
        {
            start -= 1;
            count = math_roll_convert_previous(expression, &start, &error);
        }
        const int32_t faces = math_roll_convert_next(expression, index + 1, &error);
        int32_t value;
        if (error || count < 1 || faces < 1
            || rng_dice_roll_checked(count, faces, value) != FT_ERR_SUCCESS
            || math_roll_replace_number(expression, length, start, index + 1, value))
            return (1);
        index = start;
    }
    return (0);
}
