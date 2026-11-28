#include "../test_internal.hpp"
#include "../../Modules/Math/math.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <limits>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_sqrt_basic)
{
    double result;

    result = math_sqrt(25.0);
    FT_ASSERT(math_fabs(result - 5.0) < 0.000001);
    return (1);
}

FT_TEST(test_math_sqrt_zero)
{
    double result;

    result = math_sqrt(0.0);
    FT_ASSERT_EQ(0.0, result);
    return (1);
}

FT_TEST(test_math_sqrt_nan_input)
{
    double result;

    result = math_sqrt(math_nan());
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_sqrt_negative_returns_nan)
{
    double result;

    result = math_sqrt(-4.0);
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_sqrt_positive_infinity)
{
    double input;
    double result;

    input = std::numeric_limits<double>::infinity();
    result = math_sqrt(input);
    FT_ASSERT_EQ(input, result);
    return (1);
}

FT_TEST(test_math_sqrt_small_positive_input)
{
    double input;
    double result;

    input = 1e-14;
    result = math_sqrt(input);
    FT_ASSERT(math_fabs(result - 1e-7) < 1e-12);
    return (1);
}

FT_TEST(test_math_sqrt_recovers_after_nan)
{
    double result;

    result = math_sqrt(math_nan());
    FT_ASSERT(math_isnan(result));
    result = math_sqrt(81.0);
    FT_ASSERT(math_fabs(result - 9.0) < 0.000001);
    return (1);
}

FT_TEST(test_math_sqrt_preserves_relative_accuracy_at_exponent_limits)
{
    constexpr double TINY_INPUT = 1e-300;
    constexpr double LARGE_INPUT = 1e300;

    FT_ASSERT(math_fabs(math_sqrt(TINY_INPUT) / 1e-150 - 1.0) < 1e-12);
    FT_ASSERT(math_fabs(math_sqrt(LARGE_INPUT) / 1e150 - 1.0) < 1e-12);
    FT_ASSERT_EQ(0.0, math_sqrt(std::numeric_limits<double>::denorm_min()));
    return (1);
}
