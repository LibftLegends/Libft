#include "roll.hpp"
#include "math_internal.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Template/vector.hpp"

static int32_t math_roll_parse_group(char *expression, int32_t &length)
{
    if (math_roll_execute_dice(expression, length) != 0
        || math_roll_execute_md(expression, length) != 0
        || math_roll_execute_pm(expression, length) != 0)
        return (1);
    return (0);
}

static int32_t math_roll_parse(char *expression, int32_t length, int32_t &value)
{
    ft_vector<int32_t> groups;
    if (groups.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t index = 0;
    while (index < length)
    {
        if (expression[index] == '(' && groups.push_back(index) != FT_ERR_SUCCESS)
            return (1);
        index += 1;
    }
    // The prior recursive walk reduced opening parentheses from right to left.
    while (!groups.empty())
    {
        const int32_t opening = groups[groups.size() - 1U];
        groups.pop_back();
        char *group = expression + opening;
        int32_t group_length = length - opening;
        if (math_roll_parse_group(group, group_length) != 0
            || math_roll_parse_brackets(group, group_length) != 0)
            return (1);
        length = opening + group_length;
    }
    if (math_roll_parse_group(expression, length) != 0
        || !math_check_string_number(expression))
        return (1);
    int32_t error = 0;
    value = math_roll_convert_next(expression, 0, &error);
    return (error);
}

int32_t *math_roll(const char *expression)
{
    if (expression == ft_nullptr)
        return (ft_nullptr);
    if (ft_strchr(expression, 'd') == ft_nullptr)
        return (math_eval(expression));
    const ft_size_t length = ft_strlen_size_t(expression);
    if (length > static_cast<ft_size_t>(INT32_MAX))
        return (ft_nullptr);
    char *buffer = static_cast<char *>(cma_malloc(length + 1U));
    if (buffer == ft_nullptr)
        return (ft_nullptr);
    ft_memcpy(buffer, expression, length + 1U);
    int32_t value;
    int32_t error = math_roll_validate(buffer);
    if (error == 0)
        error = math_roll_parse(buffer, static_cast<int32_t>(length), value);
    cma_free(buffer);
    if (error != 0)
        return (ft_nullptr);
    int32_t *result = static_cast<int32_t *>(cma_malloc(sizeof(value)));
    if (result != ft_nullptr)
        *result = value;
    return (result);
}
