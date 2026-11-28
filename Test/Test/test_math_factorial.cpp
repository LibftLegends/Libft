#include "../test_internal.hpp"
#include "../../Modules/Math/math.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_factorial_int_basic)
{
    int result;

    result = math_factorial(5);
    FT_ASSERT_EQ(120, result);
    return (1);
}

FT_TEST(test_math_factorial_zero_is_one)
{
    int64_t result;

    result = math_factorial(static_cast<int64_t>(0));
    FT_ASSERT_EQ(static_cast<int64_t>(1), result);
    return (1);
}

FT_TEST(test_math_factorial_negative_input)
{
    int result;

    result = math_factorial(-3);
    FT_ASSERT_EQ(0, result);
    return (1);
}

FT_TEST(test_math_factorial_int_overflow)
{
    int result;

    result = math_factorial(13);
    FT_ASSERT_EQ(0, result);
    return (1);
}

FT_TEST(test_math_factorial_long_large_value)
{
    int64_t result;

    result = math_factorial(static_cast<int64_t>(20));
    FT_ASSERT_EQ(2432902008176640000LL, result);
    return (1);
}

FT_TEST(test_math_factorial_long_overflow)
{
    int64_t result;

    result = math_factorial(static_cast<int64_t>(21));
    FT_ASSERT_EQ(static_cast<int64_t>(0), result);
    return (1);
}

FT_TEST(test_math_factorial_long_long_large_value)
{
    int64_t result;

    result = math_factorial(static_cast<int64_t>(20));
    FT_ASSERT_EQ(2432902008176640000LL, result);
    return (1);
}

FT_TEST(test_math_factorial_long_long_overflow)
{
    int64_t result;

    result = math_factorial(static_cast<int64_t>(21));
    FT_ASSERT_EQ(0LL, result);
    return (1);
}

FT_TEST(test_math_factorial_recovers_after_invalid_input)
{
    int64_t result;

    result = math_factorial(static_cast<int64_t>(-5));
    FT_ASSERT_EQ(static_cast<int64_t>(0), result);
    result = math_factorial(static_cast<int64_t>(6));
    FT_ASSERT_EQ(static_cast<int64_t>(720), result);
    return (1);
}

FT_TEST(test_math_factorial_signed_width_boundaries)
{
    FT_ASSERT_EQ(479001600, math_factorial(12));
    FT_ASSERT_EQ(0, math_factorial(INT32_MIN));
    FT_ASSERT_EQ(0, math_factorial(INT32_MAX));
    FT_ASSERT_EQ(static_cast<int64_t>(0), math_factorial(static_cast<int64_t>(INT64_MIN)));
    FT_ASSERT_EQ(static_cast<int64_t>(0), math_factorial(static_cast<int64_t>(INT64_MAX)));
    return (1);
}
