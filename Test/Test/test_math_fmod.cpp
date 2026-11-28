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

FT_TEST(test_math_fmod_nan_input)
{
    double result;

    result = math_fmod(math_nan(), 2.0);
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_fmod_infinite_value)
{
    double result;
    double infinite_value;

    infinite_value = std::numeric_limits<double>::infinity();
    result = math_fmod(infinite_value, 3.0);
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_fmod_zero_modulus)
{
    double result;

    result = math_fmod(4.0, 0.0);
    FT_ASSERT(math_isnan(result));
    return (1);
}

FT_TEST(test_math_fmod_infinite_modulus)
{
    double result;
    double infinite_value;

    infinite_value = std::numeric_limits<double>::infinity();
    result = math_fmod(7.0, infinite_value);
    FT_ASSERT_EQ(7.0, result);
    return (1);
}

FT_TEST(test_math_fmod_success)
{
    double result;

    result = math_fmod(5.5, 2.0);
    FT_ASSERT(math_fabs(result - 1.5) < 0.000001);
    return (1);
}

FT_TEST(test_math_fmod_recovers_after_zero_modulus)
{
    double result;

    result = math_fmod(3.0, 0.0);
    FT_ASSERT(math_isnan(result));
    result = math_fmod(9.0, 4.0);
    FT_ASSERT(math_fabs(result - 1.0) < 0.000001);
    return (1);
}

FT_TEST(test_math_fmod_retains_minimum_subnormal_policy)
{
    const double smallest = std::numeric_limits<double>::denorm_min();
    FT_ASSERT(math_isnan(math_fmod(1.0, -smallest)));
    FT_ASSERT_EQ(-0.0, math_fmod(-3.0 * smallest, 2.0 * smallest));
    FT_ASSERT_EQ(2.0 * smallest, math_fmod(2.0 * smallest, 4.0 * smallest));
    return (1);
}
