#include "../test_internal.hpp"
#include "../../Modules/Math/roll.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_eval_basic)
{
    int *value;
    int ok;

    value = math_eval("1+2");
    if (!value)
        return (0);
    ok = (*value == 3);
    cma_free(value);
    return (ok);
}

FT_TEST(test_math_eval_parentheses)
{
    int *value;
    int ok;

    value = math_eval("2*(3+4)");
    if (!value)
        return (0);
    ok = (*value == 14);
    cma_free(value);
    return (ok);
}

FT_TEST(test_math_eval_dice_rejected)
{
    int *value;

    value = math_eval("1d6");
    if (value != ft_nullptr)
    {
        if (value)
            cma_free(value);
        return (0);
    }
    return (1);
}
