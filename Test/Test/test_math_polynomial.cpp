#include "../test_internal.hpp"
#include "../../Modules/Math/ft_cubic_spline.hpp"
#include "../../Modules/Math/math.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cmath>
#include <limits>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static double test_math_polynomial_sine(double value, void *user_data)
{
    (void)user_data;
    return (std::sin(value));
}

static double test_math_polynomial_square(double value, void *user_data)
{
    (void)user_data;
    return (value * value);
}

FT_TEST(test_polynomial_newton_finds_root)
{
    ft_vector<double> coefficients;
    double root;
    int32_t result_code;
    double tolerance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, coefficients.initialize());
    coefficients.push_back(-2.0);
    coefficients.push_back(0.0);
    coefficients.push_back(1.0);
    tolerance = 0.000001;
    result_code = math_polynomial_find_root_newton(coefficients,
            1.0,
            tolerance,
            32,
            root);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_code);
    FT_ASSERT(std::fabs(root * root - 2.0) < tolerance);
    return (1);
}

FT_TEST(test_polynomial_quadratic_solver_returns_roots)
{
    double root_one;
    double root_two;
    int32_t result_code;

    result_code = math_polynomial_solve_quadratic(1.0, -5.0, 6.0,
            root_one,
            root_two);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_code);
    FT_ASSERT(std::fabs((root_one * root_one) - 5.0 * root_one + 6.0) < 0.000001);
    FT_ASSERT(std::fabs((root_two * root_two) - 5.0 * root_two + 6.0) < 0.000001);
    return (1);
}

FT_TEST(test_polynomial_lagrange_interpolation_recovers_parabola)
{
    ft_vector<double> x_values;
    ft_vector<double> y_values;
    double interpolated;
    int32_t result_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, x_values.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, y_values.initialize());
    x_values.push_back(-1.0);
    x_values.push_back(0.0);
    x_values.push_back(1.0);
    y_values.push_back(1.0);
    y_values.push_back(0.0);
    y_values.push_back(1.0);
    result_code = math_polynomial_lagrange_interpolate(x_values,
            y_values,
            2.0,
            interpolated);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_code);
    FT_ASSERT(std::fabs(interpolated - 4.0) < 0.000001);
    return (1);
}

FT_TEST(test_polynomial_bezier_scalar_midpoint)
{
    ft_vector<double> control_points;
    double value;
    int32_t result_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, control_points.initialize());
    control_points.push_back(0.0);
    control_points.push_back(2.0);
    control_points.push_back(4.0);
    result_code = math_bezier_evaluate(control_points, 0.5, value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_code);
    FT_ASSERT(std::fabs(value - 2.0) < 0.000001);
    return (1);
}

FT_TEST(test_polynomial_cubic_spline_tracks_curve)
{
    ft_vector<double> x_values;
    ft_vector<double> y_values;
    ft_cubic_spline spline;
    ft_cubic_spline *built_spline;
    double evaluated;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, x_values.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, y_values.initialize());
    x_values.push_back(0.0);
    x_values.push_back(1.0);
    x_values.push_back(2.0);
    x_values.push_back(3.0);
    y_values.push_back(0.0);
    y_values.push_back(1.0);
    y_values.push_back(8.0);
    y_values.push_back(27.0);
    built_spline = ft_cubic_spline_build(x_values, y_values);
    FT_ASSERT(built_spline != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, spline.initialize(*built_spline));
    delete built_spline;
    evaluated = ft_cubic_spline_evaluate(spline, 1.5);
    FT_ASSERT(std::fabs(evaluated - 3.375) < 0.01);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, spline.destroy());
    return (1);
}

FT_TEST(test_polynomial_trapezoidal_integral)
{
    double result;
    int32_t status;

    status = math_integrate_trapezoidal(test_math_polynomial_square,
            ft_nullptr,
            0.0,
            1.0,
            128,
            result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status);
    FT_ASSERT(std::fabs(result - (1.0 / 3.0)) < 0.001);
    return (1);
}

FT_TEST(test_polynomial_simpson_integral)
{
    double result;
    int32_t status;
    double pi_value;

    pi_value = std::acos(-1.0);
    status = math_integrate_simpson(test_math_polynomial_sine,
            ft_nullptr,
            0.0,
            pi_value,
            0.000001,
            result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status);
    FT_ASSERT(std::fabs(result - 2.0) < 0.0001);
    return (1);
}

FT_TEST(test_polynomial_evaluate_reads_coefficients_before_writing_alias)
{
    ft_vector<double> coefficients;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, coefficients.initialize());
    const double values[] = {2.0, 3.0, 4.0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, coefficients.append(values, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, coefficients.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_polynomial_evaluate(coefficients, 2.0, coefficients[0]));
    FT_ASSERT_EQ(24.0, coefficients[0]);
    return (1);
}

FT_TEST(test_polynomial_newton_accepts_stationary_root)
{
    ft_vector<double> coefficients;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, coefficients.initialize());
    const double values[] = {0.0, 0.0, 1.0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, coefficients.append(values, 3U));
    double root = 7.0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_polynomial_find_root_newton(coefficients,
        0.0, 1e-12, 8U, root));
    FT_ASSERT_EQ(0.0, root);
    return (1);
}

FT_TEST(test_polynomial_spline_clamps_and_reproduces_nonuniform_cubic)
{
    ft_vector<double> knots;
    ft_vector<double> values;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, knots.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    const double input[] = {-3.0, -1.0, 0.0, 2.0, 7.0};
    const double output[] = {-27.0, -1.0, 0.0, 8.0, 343.0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, knots.append(input, 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.append(output, 5U));
    ft_cubic_spline *spline = ft_cubic_spline_build(knots, values);
    FT_ASSERT(spline != ft_nullptr);
    const int32_t mutex_status = spline->enable_thread_safety();
    const double below = ft_cubic_spline_evaluate(*spline, -50.0);
    const double above = ft_cubic_spline_evaluate(*spline, 50.0);
    const double inside = ft_cubic_spline_evaluate(*spline, 3.5);
    delete spline;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_status);
    FT_ASSERT(std::fabs(below + 27.0) < 1e-12);
    FT_ASSERT(std::fabs(above - 343.0) < 1e-12);
    FT_ASSERT(std::fabs(inside - 42.875) < 1e-12);
    return (1);
}

FT_TEST(test_polynomial_spline_returns_null_on_workspace_allocation_failure)
{
    ft_vector<double> knots;
    ft_vector<double> values;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, knots.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    ft_size_t index = 0U;
    while (index < 64U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, knots.push_back(static_cast<double>(index)));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, values.push_back(0.0));
        index += 1U;
    }
    cma_set_alloc_limit(64U * sizeof(double));
    ft_cubic_spline *spline = ft_cubic_spline_build(knots, values);
    cma_set_alloc_limit(0U);
    const ft_bool returned_null = (spline == ft_nullptr);
    delete spline;
    FT_ASSERT_EQ(FT_TRUE, returned_null);
    return (1);
}

static double test_math_polynomial_counting_square(double value, void *user_data)
{
    uint32_t *calls = static_cast<uint32_t *>(user_data);
    *calls += 1U;
    return (value * value);
}

static double test_math_polynomial_nan_at_left(double value, void *user_data)
{
    (void)user_data;
    if (value == 0.0)
        return (std::numeric_limits<double>::quiet_NaN());
    return (value);
}

FT_TEST(test_polynomial_simpson_reuses_samples_and_keeps_orientation)
{
    uint32_t calls = 0U;
    double result = 0.0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_integrate_simpson(test_math_polynomial_counting_square,
        &calls, 1.0, 0.0, 1e-10, result));
    FT_ASSERT(std::fabs(result + 1.0 / 3.0) < 1e-14);
    FT_ASSERT_EQ(5U, calls);
    return (1);
}

FT_TEST(test_polynomial_integrators_reject_nan_endpoints)
{
    double result = 42.0;
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, math_integrate_trapezoidal(
        test_math_polynomial_nan_at_left, ft_nullptr, 0.0, 1.0, 8U, result));
    FT_ASSERT_EQ(0.0, result);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, math_integrate_simpson(
        test_math_polynomial_nan_at_left, ft_nullptr, 0.0, 1.0, 1e-6, result));
    FT_ASSERT_EQ(0.0, result);
    return (1);
}

FT_TEST(test_polynomial_quadratic_scales_extreme_discriminants)
{
    double first = 0.0;
    double second = 0.0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_polynomial_solve_quadratic(
        1e300, -5e300, 6e300, first, second));
    FT_ASSERT(std::fabs(first - 3.0) < 1e-12);
    FT_ASSERT(std::fabs(second - 2.0) < 1e-12);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_polynomial_solve_quadratic(
        1e-300, -5e-300, 6e-300, first, second));
    FT_ASSERT(std::fabs(first - 3.0) < 1e-12);
    FT_ASSERT(std::fabs(second - 2.0) < 1e-12);

    const double smallest = std::numeric_limits<double>::denorm_min();
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, math_polynomial_solve_quadratic(
        smallest, 0.0, smallest, first, second));
    FT_ASSERT_EQ(0.0, first);
    FT_ASSERT_EQ(0.0, second);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_polynomial_solve_quadratic(
        1e308, 1e308, -1e308, first, second));
    FT_ASSERT(std::fabs(first + 1.618033988749895) < 1e-12);
    FT_ASSERT(std::fabs(second - 0.618033988749895) < 1e-12);
    return (1);
}

FT_TEST(test_polynomial_lagrange_accepts_aliased_input_and_rejects_duplicate_knots)
{
    ft_vector<double> points;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, points.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, points.enable_thread_safety());
    const double identity[] = {0.0, 1.0, 2.0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, points.append(identity, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_polynomial_lagrange_interpolate(points, points,
        0.5, points[2]));
    FT_ASSERT_EQ(0.5, points[2]);

    points[2] = 1.0;
    double result = 99.0;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, math_polynomial_lagrange_interpolate(points,
        points, 0.5, result));
    FT_ASSERT_EQ(0.0, result);
    return (1);
}

FT_TEST(test_polynomial_vector_bezier_evaluates_two_axes_and_reports_no_memory)
{
    ft_vector<vector2> points;
    vector2 point;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, points.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, point.initialize(1.0, -4.0));
    ft_size_t index = 0U;
    while (index < 12U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, points.push_back(point));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, point.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, point.initialize(7.0, 8.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, points.push_back(point));
    vector2 result;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_bezier_evaluate_vector2(points, 1.0, result));
    FT_ASSERT_EQ(7.0, result.get_x());
    FT_ASSERT_EQ(8.0, result.get_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_bezier_evaluate_vector2(points, 0.5, result));
    FT_ASSERT_EQ(1.0 + 6.0 / 4096.0, result.get_x());
    FT_ASSERT_EQ(-4.0 + 12.0 / 4096.0, result.get_y());

    cma_set_alloc_limit(1U);
    const int32_t error = math_bezier_evaluate_vector2(points, 0.5, result);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error);
    FT_ASSERT_EQ(1.0 + 6.0 / 4096.0, result.get_x());
    return (1);
}
