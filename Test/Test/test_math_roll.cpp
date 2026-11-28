#include "../test_internal.hpp"
#include "../../Modules/Math/roll.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/RNG/rng_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_roll_null_input)
{
    int *value;

    value = math_roll(ft_nullptr);
    if (value != ft_nullptr)
    {
        cma_free(value);
        return (0);
    }
    return (1);
}

FT_TEST(test_math_roll_empty_expression)
{
    int *value;

    value = math_roll("");
    if (value != ft_nullptr)
    {
        cma_free(value);
        return (0);
    }
    return (1);
}

FT_TEST(test_math_roll_invalid_character)
{
    int *value;

    value = math_roll("2a+3");
    if (value != ft_nullptr)
    {
        cma_free(value);
        return (0);
    }
    return (1);
}

FT_TEST(test_math_roll_unbalanced_parentheses)
{
    int *value;

    value = math_roll(")2+3(");
    if (value != ft_nullptr)
    {
        cma_free(value);
        return (0);
    }
    return (1);
}

FT_TEST(test_math_roll_division_by_zero)
{
    int *value;

    value = math_roll("10/0");
    if (value != ft_nullptr)
    {
        cma_free(value);
        return (0);
    }
    return (1);
}

FT_TEST(test_math_roll_detects_overflow)
{
    int *value;

    value = math_roll("2147483647+1");
    if (value != ft_nullptr)
    {
        cma_free(value);
        return (0);
    }
    return (1);
}

FT_TEST(test_math_roll_complex_expression)
{
    int *value;

    value = math_roll("((2+3)*(4+5)-6)/(3-1)");
    if (value == ft_nullptr)
        return (0);
    FT_ASSERT_EQ(19, *value);
    cma_free(value);
    return (1);
}

FT_TEST(test_math_roll_operator_precedence)
{
    int *value;

    value = math_roll("2+3*4-5/2+6*(3+1)");
    if (value == ft_nullptr)
        return (0);
    FT_ASSERT_EQ(36, *value);
    cma_free(value);
    return (1);
}

FT_TEST(test_math_roll_negative_numbers)
{
    int *value;

    value = math_roll("-5+3-10/2+(3-10)*2");
    if (value == ft_nullptr)
        return (0);
    FT_ASSERT_EQ(-21, *value);
    cma_free(value);
    return (1);
}

FT_TEST(test_math_roll_dice_expression)
{
    int *value;

    value = math_roll("2d6");
    if (value == ft_nullptr)
        return (0);
    FT_ASSERT(*value >= 2);
    FT_ASSERT(*value <= 12);
    cma_free(value);
    return (1);
}

FT_TEST(test_math_roll_long_expression)
{
    int *value;

    value = math_roll("1+2+3+4+5+6+7+8+9+10+11+12+13+14+15+16+17+18+19+20-5-4-3-2-1+6*3-(2*2)+(3*(4+5))-6/2+((3+4)*(5-2))");
    if (value == ft_nullptr)
        return (0);
    FT_ASSERT_EQ(254, *value);
    cma_free(value);
    return (1);
}

static int32_t test_math_roll_expected(int32_t *value, int32_t expected)
{
    if (value == ft_nullptr)
        return (0);
    const int32_t actual = *value;
    cma_free(value);
    FT_ASSERT_EQ(expected, actual);
    return (1);
}

FT_TEST(test_math_eval_checks_numeric_limits_and_precedence)
{
    FT_ASSERT(test_math_roll_expected(math_eval("2*-3+8/(-2)"), -10));
    FT_ASSERT(test_math_roll_expected(math_eval("10-3-2+8/2/2"), 7));
    FT_ASSERT(test_math_roll_expected(math_roll("-(3+4)*-2"), 14));
    FT_ASSERT(test_math_roll_expected(math_eval("-2147483648"), INT32_MIN));
    FT_ASSERT(test_math_roll_expected(math_roll("0000000000000000000000000009"), 9));
    FT_ASSERT(math_eval("2147483648") == ft_nullptr);
    FT_ASSERT(math_roll("-2147483649") == ft_nullptr);
    FT_ASSERT(math_eval("-2147483648/-1") == ft_nullptr);
    FT_ASSERT(math_eval("-(-2147483648)") == ft_nullptr);
    FT_ASSERT(math_eval("46341*46341") == ft_nullptr);
    return (1);
}

FT_TEST(test_math_expr_rejects_bad_grammar_and_eval_rejects_dice)
{
    FT_ASSERT(math_eval("1d6") == ft_nullptr);
    FT_ASSERT(math_eval("(d6)") == ft_nullptr);
    FT_ASSERT(math_eval("1+") == ft_nullptr);
    FT_ASSERT(math_eval("1(2)") == ft_nullptr);
    FT_ASSERT(math_eval("()") == ft_nullptr);
    FT_ASSERT(math_eval("(1") == ft_nullptr);
    FT_ASSERT(math_eval("1)") == ft_nullptr);
    FT_ASSERT(math_roll("2+(d6") == ft_nullptr);
    return (1);
}

FT_TEST(test_math_eval_uses_stack_storage_for_flat_expression)
{
    cma_set_alloc_limit(sizeof(int32_t));
    int32_t *value = math_eval("7*5+5*6-2*3+4/2");
    cma_set_alloc_limit(0U);
    FT_ASSERT(test_math_roll_expected(value, 61));
    return (1);
}

FT_TEST(test_math_eval_handles_deep_parentheses)
{
    constexpr ft_size_t DEPTH = 1024U;
    char expression[2U * DEPTH + 2U];
    ft_size_t index = 0U;
    while (index < DEPTH)
    {
        expression[index] = '(';
        expression[DEPTH + 1U + index] = ')';
        index += 1U;
    }
    expression[DEPTH] = '7';
    expression[2U * DEPTH + 1U] = '\0';
    FT_ASSERT(test_math_roll_expected(math_eval(expression), 7));
    return (1);
}

FT_TEST(test_math_roll_preserves_seeded_order_across_sibling_groups)
{
    constexpr uint32_t SEED = 17659U;
    ft_seed_random_engine(SEED);
    const int32_t right = ft_dice_roll(1, 11);
    const int32_t middle = ft_dice_roll(1, 8);
    const int32_t left = ft_dice_roll(1, 6);
    const int32_t expected_next = ft_random_int();

    ft_seed_random_engine(SEED);
    FT_ASSERT(test_math_roll_expected(math_roll("1d6+(1d8+1)+(1d11*2)"),
        left + middle + 1 + right * 2));
    FT_ASSERT_EQ(expected_next, ft_random_int());
    return (1);
}

FT_TEST(test_math_roll_reports_overflow_separately_from_a_numeric_roll)
{
    ft_seed_random_engine(1337U);
    FT_ASSERT(math_roll("20d2147483647") == ft_nullptr);
    FT_ASSERT(test_math_roll_expected(math_roll("10d1"), 10));
    FT_ASSERT(test_math_roll_expected(math_roll("2+d1-3*d1"), 0));
    FT_ASSERT(test_math_roll_expected(math_roll("2d1d1"), 2));
    return (1);
}

FT_TEST(test_math_roll_handles_many_nested_dice_groups)
{
    constexpr ft_size_t DEPTH = 1024U;
    char expression[2U * DEPTH + 4U];
    ft_size_t index = 0U;
    while (index < DEPTH)
    {
        expression[index] = '(';
        expression[DEPTH + 3U + index] = ')';
        index += 1U;
    }
    expression[DEPTH] = '1';
    expression[DEPTH + 1U] = 'd';
    expression[DEPTH + 2U] = '1';
    expression[2U * DEPTH + 3U] = '\0';
    FT_ASSERT(test_math_roll_expected(math_roll(expression), 1));
    return (1);
}
