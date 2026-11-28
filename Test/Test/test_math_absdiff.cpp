#include "../test_internal.hpp"
#include "../../Modules/Math/math.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_absdiff_int_symmetry)
{
    int forward_difference;
    int reverse_difference;

    forward_difference = math_absdiff(12, -5);
    reverse_difference = math_absdiff(-5, 12);
    FT_ASSERT_EQ(forward_difference, reverse_difference);
    FT_ASSERT_EQ(17, forward_difference);
    return (1);
}

FT_TEST(test_math_absdiff_long_precision)
{
    int64_t first_number;
    int64_t second_number;
    int64_t difference;

    first_number = 123456789LL;
    second_number = -98765432LL;
    difference = math_absdiff(first_number, second_number);
    FT_ASSERT_EQ(222222221LL, difference);
    return (1);
}

FT_TEST(test_math_absdiff_long_long_large_values)
{
    int64_t first_number;
    int64_t second_number;
    int64_t difference;

    first_number = 922337203685477000LL;
    second_number = 922337203685470000LL;
    difference = math_absdiff(first_number, second_number);
    FT_ASSERT_EQ(7000LL, difference);
    return (1);
}

FT_TEST(test_math_absdiff_double_precision)
{
    double difference;

    difference = math_absdiff(-12.5, 3.25);
    FT_ASSERT(math_fabs(difference - 15.75) < 0.000001);
    return (1);
}

FT_TEST(test_math_absdiff_extreme_integers)
{
    int int_difference;
    int64_t long_difference;
    int64_t long_long_difference;

    int_difference = math_absdiff(FT_INT32_MIN, FT_INT32_MAX);
    long_difference = math_absdiff(static_cast<int64_t>(FT_LLONG_MIN),
            static_cast<int64_t>(FT_LLONG_MAX));
    long_long_difference = math_absdiff(FT_LLONG_MIN, FT_LLONG_MAX);
    FT_ASSERT_EQ(FT_INT32_MAX, int_difference);
    FT_ASSERT_EQ(static_cast<int64_t>(FT_LLONG_MAX), long_difference);
    FT_ASSERT_EQ(FT_LLONG_MAX, long_long_difference);
    return (1);
}

FT_TEST(test_math_absdiff_zero_inputs)
{
    int int_difference;
    int64_t long_difference;

    int_difference = math_absdiff(0, 0);
    long_difference = math_absdiff(static_cast<int64_t>(-12345),
            static_cast<int64_t>(-12345));
    FT_ASSERT_EQ(0, int_difference);
    FT_ASSERT_EQ(static_cast<int64_t>(0), long_difference);
    return (1);
}

FT_TEST(test_math_absdiff_mixed_signs_near_signed_max)
{
    FT_ASSERT_EQ(INT32_MAX - 1, math_absdiff(INT32_MIN, -2));
    FT_ASSERT_EQ(INT32_MAX - 1, math_absdiff(1, INT32_MIN + 3));
    FT_ASSERT_EQ(INT32_MAX, math_absdiff(INT32_MIN, INT32_MAX));

    const int64_t minimum = INT64_MIN;
    const int64_t maximum = INT64_MAX;
    FT_ASSERT_EQ(maximum - 1, math_absdiff(minimum, static_cast<int64_t>(-2)));
    FT_ASSERT_EQ(maximum - 1, math_absdiff(static_cast<int64_t>(1), minimum + 3));
    FT_ASSERT_EQ(maximum, math_absdiff(maximum, minimum));
    return (1);
}
