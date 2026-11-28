#include "math_internal.hpp"
#include "../Basic/basic.hpp"

int32_t math_roll_parse_brackets(char *expression, int32_t &length)
{
    int32_t closing = 1;
    while (closing < length && expression[closing] != ')')
        closing += 1;
    if (closing == length)
        return (1);
    ft_memmove(expression, expression + 1, static_cast<ft_size_t>(closing - 1));
    ft_memmove(expression + closing - 1, expression + closing + 1,
        static_cast<ft_size_t>(length - closing));
    length -= 2;
    return (0);
}
