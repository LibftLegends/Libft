#include "../test_internal.hpp"
#include "../../Modules/Math/math.hpp"
#include <cmath>
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <limits>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_acos_nan_input)
{
    double result;

    result = math_acos(math_nan());
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_acos_infinite_input)
{
    double result;
    double infinite_value;

    infinite_value = std::numeric_limits<double>::infinity();
    result = math_acos(infinite_value);
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_acos_above_one)
{
    double result;

    result = math_acos(1.5);
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_acos_below_negative_one)
{
    double result;

    result = math_acos(-1.5);
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_acos_success)
{
    double result;
    double expected;

    expected = 1.0471975511965979;
    result = math_acos(0.5);
    FT_ASSERT(math_fabs(result - expected) < 0.000001);
    return (1);
}

FT_TEST(test_math_acos_tolerance_clamps_slightly_above_one)
{
    double result;
    double input_value;

    input_value = 1.0 + 5e-14;
    result = math_acos(input_value);
    FT_ASSERT(math_fabs(result) <= 1e-12);
    return (1);
}

FT_TEST(test_math_acos_tolerance_clamps_slightly_below_negative_one)
{
    double result;
    double input_value;
    double pi_value;

    input_value = -1.0 - 5e-14;
    pi_value = 3.14159265358979323846;
    result = math_acos(input_value);
    FT_ASSERT(math_fabs(result - pi_value) <= 1e-12);
    return (1);
}

FT_TEST(test_math_acos_tolerance_handles_near_zero)
{
    double result;
    double expected;

    expected = 3.14159265358979323846 * 0.5;
    result = math_acos(5e-14);
    FT_ASSERT(math_fabs(result - expected) <= 1e-12);
    return (1);
}

FT_TEST(test_math_acos_value_beyond_tolerance)
{
    double result;
    double input_value;

    input_value = 1.0 + 1e-9;
    result = math_acos(input_value);
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_acos_recovers_after_invalid_input)
{
    double result;
    double expected;

    result = math_acos(2.0);
    FT_ASSERT(math_isnan(result));
    expected = 3.14159265358979323846 * 0.25;
    result = math_acos(0.7071067811865475);
    FT_ASSERT(math_fabs(result - expected) <= 1e-12);
    return (1);
}

FT_TEST(test_math_acos_resolves_values_near_endpoints)
{
    constexpr double INPUT = 1.0 - 1e-12;

    FT_ASSERT(math_fabs(math_acos(INPUT) - std::acos(INPUT)) < 1e-12);
    FT_ASSERT(math_fabs(math_acos(-INPUT) - std::acos(-INPUT)) < 1e-12);
    return (1);
}
