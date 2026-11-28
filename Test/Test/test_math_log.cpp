#include "../test_internal.hpp"
#include "../../Modules/Math/math.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cmath>
#include <limits>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_log_zero_input)
{
    double result;

    result = math_log(0.0);
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_log_negative_input)
{
    double result;

    result = math_log(-4.2);
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_log_subnormal_positive)
{
    double input;
    double result;
    double expected;
    double difference;
    double tolerance;

    input = std::numeric_limits<double>::denorm_min();
    result = math_log(input);
    expected = std::log(input);
    difference = math_fabs(result - expected);
    tolerance = 0.000000001;
    FT_ASSERT(difference < tolerance);
    return (1);
}

FT_TEST(test_math_log_positive_input)
{
    double result;

    result = math_log(8.0);
    FT_ASSERT(math_fabs(result - std::log(8.0)) < 0.000000001);
    return (1);
}

FT_TEST(test_math_log_unity_returns_zero)
{
    double result;
    double difference;

    result = math_log(1.0);
    difference = math_fabs(result - 0.0);
    FT_ASSERT(difference < 0.0000000001);
    return (1);
}

FT_TEST(test_math_log_recovers_after_invalid_input)
{
    double result;

    FT_ASSERT(math_isnan(math_log(-2.0)));
    result = math_log(10.0);
    FT_ASSERT(math_fabs(result - std::log(10.0)) < 0.000000001);
    return (1);
}

FT_TEST(test_math_log_handles_nonfinite_inputs)
{
    const double infinity = std::numeric_limits<double>::infinity();
    FT_ASSERT_EQ(infinity, math_log(infinity));
    FT_ASSERT(math_isnan(math_log(-infinity)));
    FT_ASSERT(math_isnan(math_log(-0.0)));
    FT_ASSERT(math_isnan(math_log(std::numeric_limits<double>::quiet_NaN())));
    return (1);
}

FT_TEST(test_math_log_resolves_values_just_below_unity)
{
    const double input = std::nextafter(1.0, 0.0);
    const double expected = -std::numeric_limits<double>::epsilon() * 0.5;
    FT_ASSERT(math_fabs(math_log(input) / expected - 1.0) < 1e-12);
    return (1);
}
