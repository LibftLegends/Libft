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

static int assert_near(double expected, double actual, double tolerance)
{
    if (math_fabs(expected - actual) <= tolerance)
        return (1);
    ft_test_fail("math_fabs(expected - actual) <= tolerance", __FILE__, __LINE__);
    return (0);
}

FT_TEST(test_math_mean_handles_mixed_values)
{
    double values[4];
    double result;

    values[0] = 1.0;
    values[1] = -1.0;
    values[2] = 5.0;
    values[3] = 3.0;
    result = ft_mean(values, 4);
    if (!assert_near(2.0, result, 0.000001))
        return (0);
    return (1);
}

FT_TEST(test_math_mean_zero_length_returns_zero)
{
    double values[1];

    values[0] = 42.0;
    FT_ASSERT_EQ(0.0, ft_mean(values, 0));
    FT_ASSERT_EQ(0.0, ft_mean(values, -5));
    return (1);
}

FT_TEST(test_math_median_handles_even_count)
{
    double values[4];
    double median_value;

    values[0] = 9.0;
    values[1] = 1.0;
    values[2] = 5.0;
    values[3] = 3.0;
    median_value = ft_median(values, 4);
    if (!assert_near(4.0, median_value, 0.000001))
        return (0);
    return (1);
}

FT_TEST(test_math_median_handles_odd_count)
{
    double values[5];
    double median_value;

    values[0] = 7.0;
    values[1] = 2.0;
    values[2] = 5.0;
    values[3] = 10.0;
    values[4] = 3.0;
    median_value = ft_median(values, 5);
    if (!assert_near(5.0, median_value, 0.000001))
        return (0);
    return (1);
}

FT_TEST(test_math_mode_handles_close_duplicates)
{
    double values[6];
    double mode_value;

    values[0] = 2.5;
    values[1] = 2.5000004;
    values[2] = 1.0;
    values[3] = 2.5;
    values[4] = 4.0;
    values[5] = 3.0;
    mode_value = ft_mode(values, 6);
    if (!assert_near(2.5, mode_value, 0.000001))
        return (0);
    return (1);
}

FT_TEST(test_math_mode_zero_length_returns_zero)
{
    double values[2];

    values[0] = 1.0;
    values[1] = 2.0;
    FT_ASSERT_EQ(0.0, ft_mode(values, 0));
    FT_ASSERT_EQ(0.0, ft_mode(values, -4));
    return (1);
}

FT_TEST(test_math_variance_matches_population_formula)
{
    double values[8];
    double variance_value;

    values[0] = 2.0;
    values[1] = 4.0;
    values[2] = 4.0;
    values[3] = 4.0;
    values[4] = 5.0;
    values[5] = 5.0;
    values[6] = 7.0;
    values[7] = 9.0;
    variance_value = ft_variance(values, 8);
    if (!assert_near(4.0, variance_value, 0.000001))
        return (0);
    return (1);
}

FT_TEST(test_math_stddev_is_square_root_of_variance)
{
    double values[5];
    double variance_value;
    double stddev_value;

    values[0] = 6.0;
    values[1] = 2.0;
    values[2] = 3.0;
    values[3] = 1.0;
    values[4] = 5.0;
    variance_value = ft_variance(values, 5);
    stddev_value = ft_stddev(values, 5);
    if (!assert_near(math_sqrt(variance_value), stddev_value, 0.000001))
        return (0);
    return (1);
}

FT_TEST(test_math_stddev_uniform_values_returns_zero)
{
    double values[3];
    double result;

    values[0] = 4.0;
    values[1] = 4.0;
    values[2] = 4.0;
    result = ft_stddev(values, 3);
    if (!assert_near(0.0, result, 0.000001))
        return (0);
    return (1);
}

FT_TEST(test_math_geometric_mean_positive_values)
{
    double values[3];
    double result;

    values[0] = 1.0;
    values[1] = 4.0;
    values[2] = 9.0;
    result = ft_geometric_mean(values, 3);
    if (!assert_near(3.301927, result, 0.0001))
        return (0);
    return (1);
}

FT_TEST(test_math_geometric_mean_invalid_values_return_zero)
{
    double values[3];

    values[0] = 1.0;
    values[1] = 0.0;
    values[2] = 3.0;
    FT_ASSERT_EQ(0.0, ft_geometric_mean(values, 3));
    FT_ASSERT_EQ(0.0, ft_geometric_mean(values, 0));
    values[1] = -2.0;
    FT_ASSERT_EQ(0.0, ft_geometric_mean(values, 3));
    return (1);
}

FT_TEST(test_math_harmonic_mean_handles_basic_values)
{
    double values[3];
    double result;

    values[0] = 1.0;
    values[1] = 2.0;
    values[2] = 4.0;
    result = ft_harmonic_mean(values, 3);
    if (!assert_near(1.714285, result, 0.0001))
        return (0);
    return (1);
}

FT_TEST(test_math_harmonic_mean_rejects_zero_entries)
{
    double values[2];
    double result;

    values[0] = 1.0;
    values[1] = 0.0;
    FT_ASSERT_EQ(0.0, ft_harmonic_mean(values, 2));
    FT_ASSERT_EQ(0.0, ft_harmonic_mean(values, 0));
    values[0] = -1.0;
    values[1] = -2.0;
    result = ft_harmonic_mean(values, 2);
    if (!assert_near(-1.333333, result, 0.0001))
        return (0);
    return (1);
}

FT_TEST(test_math_order_statistics_preserve_input_and_first_mode_tie)
{
    const double values[] = {8.0, 1.0, 8.0, 2.0, 1.0, 3.0};
    FT_ASSERT_EQ(2.5, ft_median(values, 6));
    FT_ASSERT_EQ(1.0, ft_mode(values, 6));
    FT_ASSERT_EQ(8.0, values[0]);
    FT_ASSERT_EQ(1.0, values[4]);
    return (1);
}

FT_TEST(test_math_order_statistics_keep_nan_barriers)
{
    const double invalid = math_nan();
    const double even_values[] = {4.0, 4.0, invalid, 2.0, 1.0, 1.0};
    const double odd_values[] = {4.0, 3.0, invalid, 2.0, 1.0};
    FT_ASSERT(math_isnan(ft_median(even_values, 6)));
    FT_ASSERT(math_isnan(ft_median(odd_values, 5)));
    FT_ASSERT_EQ(4.0, ft_mode(even_values, 6));
    FT_ASSERT_EQ(3.0, ft_mode(odd_values, 5));
    return (1);
}

FT_TEST(test_math_order_statistics_keep_signed_zero_order)
{
    const double values[] = {1.0, -0.0, 0.0, -0.0, -1.0};
    FT_ASSERT_EQ(0.0, ft_median(values, 5));
    FT_ASSERT_EQ(0, math_signbit(ft_median(values, 5)));
    FT_ASSERT_EQ(-0.0, ft_mode(values, 5));
    FT_ASSERT_EQ(1, math_signbit(ft_mode(values, 5)));
    return (1);
}

FT_TEST(test_math_mode_keeps_adjacent_epsilon_chains)
{
    const double values[] = {3.0, 2.0000015, 2.0, 2.00000075, 3.0};
    FT_ASSERT_EQ(2.0000015, ft_mode(values, 5));
    return (1);
}
