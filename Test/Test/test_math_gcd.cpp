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

FT_TEST(test_math_gcd_basic_values)
{
    int result;

    result = math_gcd(48, 18);
    FT_ASSERT_EQ(6, result);
    return (1);
}

FT_TEST(test_math_gcd_negative_inputs)
{
    int64_t result;

    result = math_gcd(static_cast<int64_t>(-42), static_cast<int64_t>(-56));
    FT_ASSERT_EQ(14LL, result);
    return (1);
}

FT_TEST(test_math_gcd_zero_arguments)
{
    int64_t result;

    result = math_gcd(static_cast<int64_t>(0), static_cast<int64_t>(0));
    FT_ASSERT_EQ(0LL, result);
    return (1);
}

FT_TEST(test_math_gcd_one_zero_argument)
{
    FT_ASSERT_EQ(25LL, math_gcd(static_cast<int64_t>(0), static_cast<int64_t>(-25)));
    FT_ASSERT_EQ(9LL, math_gcd(static_cast<int64_t>(9), static_cast<int64_t>(0)));
    return (1);
}

FT_TEST(test_math_gcd_mixed_signs_match_positive_result)
{
    int64_t result_first_sign;
    int64_t result_second_sign;

    result_first_sign = math_gcd(static_cast<int64_t>(-84), static_cast<int64_t>(30));
    result_second_sign = math_gcd(static_cast<int64_t>(84), static_cast<int64_t>(-30));
    FT_ASSERT_EQ(result_first_sign, result_second_sign);
    FT_ASSERT_EQ(6LL, result_first_sign);
    return (1);
}

FT_TEST(test_math_gcd_signed_minimum_inputs)
{
    FT_ASSERT_EQ(2, math_gcd(INT32_MIN, 6));
    FT_ASSERT_EQ(2, math_gcd(-6, INT32_MIN));
    FT_ASSERT_EQ(INT32_MAX, math_gcd(INT32_MIN, 0));
    FT_ASSERT_EQ(INT32_MAX, math_gcd(INT32_MIN, INT32_MIN));

    const int64_t minimum = INT64_MIN;
    FT_ASSERT_EQ(static_cast<int64_t>(4), math_gcd(minimum, static_cast<int64_t>(12)));
    FT_ASSERT_EQ(static_cast<int64_t>(4), math_gcd(static_cast<int64_t>(-12), minimum));
    FT_ASSERT_EQ(static_cast<int64_t>(INT64_MAX), math_gcd(static_cast<int64_t>(0), minimum));
    FT_ASSERT_EQ(static_cast<int64_t>(INT64_MAX), math_gcd(minimum, minimum));
    return (1);
}
