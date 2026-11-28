#include "roll.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Template/vector.hpp"

struct s_math_eval_frame
{
    int32_t sum;
    int32_t term;
    char sum_operator;
    char term_operator;
    ft_bool negate;
};

static ft_bool math_eval_calculate(int32_t left, int32_t right, char operation,
    int32_t &result)
{
    int64_t value = left;
    if (operation == '+')
        value += right;
    else if (operation == '-')
        value -= right;
    else if (operation == '*')
        value *= right;
    else
    {
        if (right == 0)
            return (FT_FALSE);
        value /= right;
    }
    if (value < INT32_MIN || value > INT32_MAX)
        return (FT_FALSE);
    result = static_cast<int32_t>(value);
    return (FT_TRUE);
}

static ft_bool math_eval_read_number(const char *&cursor, ft_bool negative,
    int32_t &result)
{
    if (*cursor < '0' || *cursor > '9')
        return (FT_FALSE);

    constexpr uint64_t POSITIVE_LIMIT = static_cast<uint64_t>(INT32_MAX);
    const uint64_t limit = POSITIVE_LIMIT + negative;
    uint64_t magnitude = 0U;
    while (*cursor >= '0' && *cursor <= '9')
    {
        magnitude = magnitude * 10U + static_cast<uint64_t>(*cursor - '0');
        if (magnitude > limit)
            return (FT_FALSE);
        cursor += 1;
    }
    int64_t signed_value = static_cast<int64_t>(magnitude);
    if (negative)
        signed_value = -signed_value;
    result = static_cast<int32_t>(signed_value);
    return (FT_TRUE);
}

static ft_bool math_eval_apply_operand(s_math_eval_frame &frame, int32_t value)
{
    if (frame.term_operator == '\0')
    {
        frame.term = value;
        return (FT_TRUE);
    }
    return (math_eval_calculate(frame.term, value, frame.term_operator, frame.term));
}

static ft_bool math_eval_expression(const char *cursor, int32_t &value)
{
    ft_vector<s_math_eval_frame> parents;
    if (parents.initialize() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    s_math_eval_frame frame{0, 0, '+', '\0', FT_FALSE};
    while (FT_TRUE)
    {
        ft_bool negate = FT_FALSE;
        if (*cursor == '+' || *cursor == '-')
        {
            negate = (*cursor == '-');
            cursor += 1;
        }
        if (*cursor == '(')
        {
            if (parents.push_back(frame) != FT_ERR_SUCCESS)
                return (FT_FALSE);
            frame = {0, 0, '+', '\0', negate};
            cursor += 1;
            continue ;
        }
        if (!math_eval_read_number(cursor, negate, value)
            || !math_eval_apply_operand(frame, value))
            return (FT_FALSE);
        while (*cursor == ')')
        {
            if (parents.empty() || !math_eval_calculate(frame.sum, frame.term,
                    frame.sum_operator, value))
                return (FT_FALSE);
            if (frame.negate && !math_eval_calculate(0, value, '-', value))
                return (FT_FALSE);
            frame = parents[parents.size() - 1U];
            parents.pop_back();
            cursor += 1;
            if (!math_eval_apply_operand(frame, value))
                return (FT_FALSE);
        }
        if (*cursor == '*' || *cursor == '/')
            frame.term_operator = *cursor;
        else
        {
            if (!math_eval_calculate(frame.sum, frame.term, frame.sum_operator, frame.sum))
                return (FT_FALSE);
            if (*cursor == '\0')
            {
                value = frame.sum;
                return (parents.empty());
            }
            if (*cursor != '+' && *cursor != '-')
                return (FT_FALSE);
            frame.sum_operator = *cursor;
            frame.term_operator = '\0';
        }
        cursor += 1;
    }
}

int32_t *math_eval(const char *expression)
{
    int32_t value;
    if (expression == ft_nullptr || !math_eval_expression(expression, value))
        return (ft_nullptr);
    int32_t *result = static_cast<int32_t *>(cma_malloc(sizeof(value)));
    if (result != ft_nullptr)
        *result = value;
    return (result);
}
