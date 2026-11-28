#include "../test_internal.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <limits>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_interval_create_valid_range)
{
    ft_interval interval;

    interval = ft_interval_create(-2.0, 5.0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interval._error_code);
    FT_ASSERT_DOUBLE_EQ(-2.0, interval.lower);
    FT_ASSERT_DOUBLE_EQ(5.0, interval.upper);
    return (1);
}

FT_TEST(test_interval_create_invalid_range)
{
    ft_interval interval;

    interval = ft_interval_create(3.0, -1.0);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, interval._error_code);
    return (1);
}

FT_TEST(test_interval_add_propagates_bounds)
{
    ft_interval left_interval;
    ft_interval right_interval;
    ft_interval sum_interval;

    left_interval = ft_interval_create(-1.0, 2.0);
    right_interval = ft_interval_create(4.0, 6.0);
    sum_interval = ft_interval_add(left_interval, right_interval);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sum_interval._error_code);
    FT_ASSERT_DOUBLE_EQ(3.0, sum_interval.lower);
    FT_ASSERT_DOUBLE_EQ(8.0, sum_interval.upper);
    return (1);
}

FT_TEST(test_interval_multiply_allows_sign_variations)
{
    ft_interval left_interval;
    ft_interval right_interval;
    ft_interval product_interval;

    left_interval = ft_interval_create(-2.0, 3.0);
    right_interval = ft_interval_create(-4.0, 5.0);
    product_interval = ft_interval_multiply(left_interval, right_interval);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, product_interval._error_code);
    FT_ASSERT_DOUBLE_EQ(-12.0, product_interval.lower);
    FT_ASSERT_DOUBLE_EQ(15.0, product_interval.upper);
    return (1);
}

FT_TEST(test_interval_divide_detects_zero_crossing)
{
    ft_interval numerator_interval;
    ft_interval denominator_interval;
    ft_interval quotient_interval;

    numerator_interval = ft_interval_create(1.0, 2.0);
    denominator_interval = ft_interval_create(-1.0, 1.0);
    quotient_interval = ft_interval_divide(numerator_interval, denominator_interval);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, quotient_interval._error_code);
    return (1);
}

FT_TEST(test_interval_linear_propagation_combines_uncertainties)
{
    ft_interval components[2];
    double sensitivities[2];
    ft_interval combined_interval;

    components[0] = ft_interval_from_measurement(10.0, 0.5);
    components[1] = ft_interval_from_measurement(-2.0, 0.25);
    sensitivities[0] = 1.5;
    sensitivities[1] = -3.0;
    combined_interval = ft_interval_propagate_linear(components, sensitivities, 2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, combined_interval._error_code);
    FT_ASSERT_DOUBLE_EQ((10.0 - 0.5) * 1.5 + (-2.0 + 0.25) * -3.0, combined_interval.lower);
    FT_ASSERT_DOUBLE_EQ((10.0 + 0.5) * 1.5 + (-2.0 - 0.25) * -3.0, combined_interval.upper);
    return (1);
}

FT_TEST(test_interval_midpoint_and_radius_report_geometry)
{
    ft_interval interval;

    interval = ft_interval_create(4.0, 10.0);
    FT_ASSERT_DOUBLE_EQ(7.0, ft_interval_midpoint(interval));
    FT_ASSERT_DOUBLE_EQ(3.0, ft_interval_radius(interval));
    FT_ASSERT_EQ(1, ft_interval_contains(interval, 6.0));
    FT_ASSERT_EQ(0, ft_interval_contains(interval, 11.0));
    return (1);
}

FT_TEST(test_interval_widen_expands_bounds_symmetrically)
{
    ft_interval interval;
    ft_interval widened_interval;

    interval = ft_interval_create(2.0, 4.0);
    widened_interval = ft_interval_widen(interval, 1.5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, widened_interval._error_code);
    FT_ASSERT_DOUBLE_EQ(0.5, widened_interval.lower);
    FT_ASSERT_DOUBLE_EQ(5.5, widened_interval.upper);
    return (1);
}

FT_TEST(test_interval_divide_keeps_signed_bounds_and_subnormal_quotients)
{
    const ft_interval signed_range = ft_interval_create(-1.0, 2.0);
    const ft_interval negative = ft_interval_create(-4.0, -2.0);
    const ft_interval quotient = ft_interval_divide(signed_range, negative);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quotient._error_code);
    FT_ASSERT_EQ(-1.0, quotient.lower);
    FT_ASSERT_EQ(0.5, quotient.upper);

    const double smallest = std::numeric_limits<double>::denorm_min();
    const ft_interval tiny = ft_interval_create(smallest, 2.0 * smallest);
    const ft_interval tiny_quotient = ft_interval_divide(tiny, tiny);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tiny_quotient._error_code);
    FT_ASSERT_EQ(0.5, tiny_quotient.lower);
    FT_ASSERT_EQ(2.0, tiny_quotient.upper);
    return (1);
}

FT_TEST(test_interval_handles_wide_geometry_and_indeterminate_inputs)
{
    const double maximum = std::numeric_limits<double>::max();
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const ft_interval point = ft_interval_create(maximum, maximum);
    const ft_interval wide = ft_interval_create(-maximum, maximum);
    const ft_interval undefined = ft_interval_multiply(ft_interval_create(0.0, 1.0),
        ft_interval_create(1.0, infinity));

    FT_ASSERT_EQ(maximum, ft_interval_midpoint(point));
    FT_ASSERT_EQ(maximum, ft_interval_radius(wide));
    FT_ASSERT_EQ(0, ft_interval_contains(wide, nan));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, undefined._error_code);
    return (1);
}
