#include "../test_internal.hpp"
#include "../../Modules/Math/math.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstdint>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_lcm_basic_values)
{
    int32_t result;

    result = math_lcm(21, 6);
    FT_ASSERT_EQ(42, result);
    return (1);
}

FT_TEST(test_math_lcm_with_zero)
{
    int64_t result;

    result = math_lcm(static_cast<int64_t>(0), static_cast<int64_t>(12));
    FT_ASSERT_EQ(static_cast<int64_t>(0), result);
    return (1);
}

FT_TEST(test_math_lcm_negative_inputs)
{
    int64_t result;

    result = math_lcm(static_cast<int64_t>(-15), static_cast<int64_t>(20));
    FT_ASSERT_EQ(static_cast<int64_t>(60), result);
    return (1);
}

FT_TEST(test_math_lcm_int_overflow)
{
    int32_t result;

    result = math_lcm(FT_INT32_MAX, 2);
    FT_ASSERT_EQ(0, result);
    result = math_lcm(FT_INT32_MIN, 1);
    FT_ASSERT_EQ(0, result);
    return (1);
}

FT_TEST(test_math_lcm_long_overflow)
{
    int64_t result;

    result = math_lcm(static_cast<int64_t>(FT_LLONG_MAX), static_cast<int64_t>(2));
    FT_ASSERT_EQ(static_cast<int64_t>(0), result);
    result = math_lcm(static_cast<int64_t>(FT_LLONG_MIN), static_cast<int64_t>(1));
    FT_ASSERT_EQ(static_cast<int64_t>(0), result);
    return (1);
}

FT_TEST(test_math_lcm_reduces_before_multiplying)
{
    const int32_t multiple_int32 = INT32_MAX - 1;
    const int64_t multiple_int64 = INT64_MAX - 1;
    FT_ASSERT_EQ(multiple_int32, math_lcm(multiple_int32, -2));
    FT_ASSERT_EQ(multiple_int64, math_lcm(static_cast<int64_t>(-2), multiple_int64));
    return (1);
}

FT_TEST(test_math_lcm_signed_minimum_inputs)
{
    FT_ASSERT_EQ(0, math_lcm(INT32_MIN, INT32_MAX));
    FT_ASSERT_EQ(0, math_lcm(0, INT32_MIN));

    const int64_t minimum = INT64_MIN;
    const int64_t maximum = INT64_MAX;
    FT_ASSERT_EQ(static_cast<int64_t>(0), math_lcm(maximum, minimum));
    FT_ASSERT_EQ(static_cast<int64_t>(0), math_lcm(minimum, static_cast<int64_t>(0)));
    return (1);
}
