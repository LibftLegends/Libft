#include "roll.hpp"
#include "math_internal.hpp"
#include "../Printf/printf.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t    math_check_open_braces(char *string, int32_t index, int32_t *open_braces)
{
    if (DEBUG == 1)
        pf_printf("open braces string=%s\n", &string[index]);
    if (index > 0)
        if (math_roll_check_character(string[index - 1]))
            if (string[index - 1] != ')' && string[index - 1] != '(')
                return (1);
    if (math_roll_check_number_next(string, index))
        if (string[index + 1] != '(' && string[index + 1] != 'd')
            return (1);
    (*open_braces)++;
    return (0);
}

static int32_t    math_check_close_braces(char *string, int32_t index,
                int32_t open_braces, int32_t *close_braces)
{
    (*close_braces)++;
    if (DEBUG == 1)
        pf_printf("close braces string=%s\n", &string[index]);
    if (index == 0 || open_braces < *close_braces)
        return (1);
    if (math_roll_check_character(string[index + 1]))
        if (string[index + 1] != ')')
            return (1);
    if (math_roll_check_number_previous(string, index))
        if (string[index + 1] != '(' && string[index + 1] != ')')
            return (1);
    return (0);
}

static int32_t    math_check_plus_minus(char *string, int32_t index)
{
    int32_t sign_seen;

    sign_seen = 0;
    if (index > 0)
    {
        if (string[index - 1] == '+' || string[index - 1] == '-')
            sign_seen++;
        else if (math_roll_check_number_previous(string, index) &&
                (math_roll_check_character(string[index - 1])))
            return (1);
    }
    else if (math_roll_check_number_previous(string, index))
        return (1);
    if (!((string[index + 1] == '+' && !sign_seen) ||
            (string[index + 1] == '-' && !sign_seen) ||
            (string[index + 1] >= '0' && string[index + 1] <= '9') ||
            (string[index + 1] == '(') || string[index + 1] == 'd'))
        return (1);
    return (0);
}

static int32_t    math_check_divide_multiply(char *string, int32_t index)
{
    if (index == 0)
        return (1);
    if (math_roll_check_number_next(string, index))
        if (string[index + 1] != '(' && string[index + 1] != 'd')
            return (2);
    if (math_roll_check_number_previous(string, index))
        return (3);
    return (0);
}

static int32_t math_check_dice(char *string, int32_t index)
{
    if (math_roll_check_number_next(string, index))
        return (1);
    if (index == 0)
        return (0);
    const char previous = string[index - 1];
    return (!((previous >= '0' && previous <= '9') || previous == '('
            || previous == '+' || previous == '-' || previous == '*' || previous == '/'));
}

int32_t math_roll_validate(char *string)
{
    int32_t open_braces = 0;
    int32_t close_braces = 0;
    int32_t index = 0;

    ft_bool found_digit = FT_FALSE;
    while (string[index])
    {
        if (string[index] >= '0' && string[index] <= '9')
            found_digit = FT_TRUE;
        else if (string[index] == '(')
        {
            if (math_check_open_braces(string, index, &open_braces))
                return (1);
        }
        else if (string[index] == ')')
        {
            if (math_check_close_braces(string, index, open_braces, &close_braces))
                return (1);
        }
        else if (string[index] == '-' || string[index] == '+')
        {
            if (math_check_plus_minus(string, index))
                return (1);
        }
        else if (string[index] == '/' || string[index] == '*')
        {
            if (math_check_divide_multiply(string, index))
                return (1);
        }
        else if (string[index] == 'd')
        {
            if (math_check_dice(string, index))
                return (1);
        }
        else
            return (1);
        index += 1;
    }
    return (!found_digit || open_braces != close_braces);
}
