#include "../test_internal.hpp"
#include "../../Modules/Math/math.hpp"
#include <limits>
#include <cmath>
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_fabs_negative_value_returns_positive)
{
    double result;

    result = math_fabs(-123.5);
    FT_ASSERT(result > 0.0);
    FT_ASSERT(math_fabs(result - 123.5) < 0.000001);
    return (1);
}

FT_TEST(test_math_fabs_positive_value_unchanged)
{
    double result;

    result = math_fabs(98.25);
    FT_ASSERT(math_fabs(result - 98.25) < 0.000001);
    return (1);
}

FT_TEST(test_math_fabs_negative_zero_returns_positive_zero)
{
    double result;

    result = math_fabs(-0.0);
    FT_ASSERT(math_fabs(result) < 0.000001);
    FT_ASSERT(math_signbit(result) == 0);
    return (1);
}

FT_TEST(test_math_fabs_propagates_nan)
{
    double result;

    result = math_fabs(math_nan());
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_fabs_preserves_infinity)
{
    double result;

    result = math_fabs(math_infinity());
    FT_ASSERT(math_isinf(result));
    FT_ASSERT(result > 0.0);
    return (1);
}

FT_TEST(test_math_fabs_consistent_after_nan)
{
    double result;

    result = math_fabs(math_nan());
    FT_ASSERT(math_isnan(result));
    result = math_fabs(-42.0);
    FT_ASSERT(math_fabs(result - 42.0) < 0.000001);
    return (1);
}

FT_TEST(test_math_fabs_preserves_nan_sign)
{
    const double nan = std::copysign(std::numeric_limits<double>::quiet_NaN(), -1.0);
    const double result = math_fabs(nan);
    FT_ASSERT(math_isnan(result));
    FT_ASSERT(std::signbit(result));
    FT_ASSERT_EQ(0, math_signbit(result));
    FT_ASSERT_EQ(0, math_isinf(result));
    return (1);
}
