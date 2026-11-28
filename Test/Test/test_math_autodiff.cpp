#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Math/ft_dual_number.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static ft_dual_number *test_autodiff_two_variable_function(
    const ft_vector<ft_dual_number> &input, void *user_data)
{
    (void)user_data;
    const ft_dual_number_proxy square = input[0] * input[0];
    return (square + input[1]);
}

static ft_dual_number *test_autodiff_invalid_log(
    const ft_dual_number &input, void *user_data)
{
    (void)user_data;
    ft_dual_number *logarithm = input.apply_log();
    if (logarithm == ft_nullptr)
        return (ft_nullptr);
    ft_dual_number *result = logarithm->apply_sin();
    delete logarithm;
    (void)input.value();
    return (result);
}

static ft_dual_number *test_autodiff_error_on_second_seed(
    const ft_vector<ft_dual_number> &input, void *user_data)
{
    (void)user_data;
    if (input[1].derivative() != 0.0)
        return (input[1].apply_log());
    return (input[0] + input[1]);
}

FT_TEST(test_math_autodiff_gradient_accepts_same_input_and_output_vector)
{
    ft_vector<double> point;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, point.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, point.enable_thread_safety());
    const double coordinates[] = {3.0, 7.0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, point.append(coordinates, 2U));
    double value = 0.0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_autodiff_gradient(test_autodiff_two_variable_function,
        point, point, &value, ft_nullptr));
    FT_ASSERT_EQ(16.0, value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), point.size());
    FT_ASSERT_EQ(6.0, point[0]);
    FT_ASSERT_EQ(1.0, point[1]);
    return (1);
}

FT_TEST(test_math_autodiff_univariate_propagates_result_error)
{
    double value = 20.0;
    double derivative = 40.0;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, math_autodiff_univariate(test_autodiff_invalid_log,
        -1.0, &value, &derivative, ft_nullptr));
    FT_ASSERT_EQ(20.0, value);
    FT_ASSERT_EQ(40.0, derivative);
    return (1);
}

FT_TEST(test_math_autodiff_gradient_keeps_outputs_on_callback_and_allocation_errors)
{
    ft_vector<double> point;
    ft_vector<double> gradient;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, point.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gradient.initialize());
    const double coordinates[] = {2.0, -1.0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, point.append(coordinates, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gradient.push_back(11.0));
    double value = 20.0;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, math_autodiff_gradient(
        test_autodiff_error_on_second_seed, point, gradient, &value, ft_nullptr));
    FT_ASSERT_EQ(20.0, value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), gradient.size());
    FT_ASSERT_EQ(11.0, gradient[0]);

    point.resize(32U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, point.get_error());
    cma_set_alloc_limit(1U);
    const int32_t error = math_autodiff_gradient(test_autodiff_two_variable_function,
        point, gradient, &value, ft_nullptr);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error);
    FT_ASSERT_EQ(20.0, value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), gradient.size());
    FT_ASSERT_EQ(11.0, gradient[0]);
    return (1);
}
