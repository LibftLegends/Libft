#include "../test_internal.hpp"
#include "../../Modules/Math/ft_dual_number.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <atomic>
#include <cmath>
#include <limits>

extern std::atomic<int32_t> pt_recursive_mutex_lock_override_error_code;
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_dual_number_initialize_destroy_cycle)
{
    ft_dual_number value;

    FT_ASSERT_EQ(FT_CLASS_STATE_UNINITIALISED, value._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, value._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, value._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    return (1);
}

FT_TEST(test_math_dual_number_copy_and_move_initialise)
{
    ft_dual_number source;
    ft_dual_number copied;
    ft_dual_number moved;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(3.5, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, copied._initialised_state);
    FT_ASSERT_EQ(source.value(), copied.value());
    FT_ASSERT_EQ(source.derivative(), copied.derivative());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.initialize(static_cast<ft_dual_number &&>(source)));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, moved._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    return (1);
}

FT_TEST(test_math_dual_number_thread_safety_toggle)
{
    ft_dual_number value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(2.0, 0.0));
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_math_dual_number_proxy_chain_propagates_error)
{
    ft_dual_number numerator;
    ft_dual_number denominator;
    ft_dual_number *result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, numerator.initialize(4.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, denominator.initialize(0.0, 0.0));
    result = static_cast<ft_dual_number *>(numerator / denominator);
    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_DIVIDE_BY_ZERO, result->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->destroy());
    delete result;
    return (1);
}

FT_TEST(test_math_dual_number_quotient_handles_extreme_derivative_scales)
{
    const ft_dual_number numerator(1e200, 3e200);
    const ft_dual_number denominator(1e200, 2e200);
    ft_dual_number *large = numerator / denominator;
    FT_ASSERT(large != ft_nullptr);
    const double quotient = large->value();
    const double derivative = large->derivative();
    delete large;
    FT_ASSERT_EQ(1.0, quotient);
    FT_ASSERT(std::fabs(derivative - 1.0) < 1e-14);

    const double smallest = std::numeric_limits<double>::denorm_min();
    const ft_dual_number tiny(smallest, 0.0);
    const ft_dual_number divisor(0.25, 0.25);
    ft_dual_number *small = tiny / divisor;
    FT_ASSERT(small != ft_nullptr);
    const double tiny_derivative = small->derivative();
    delete small;
    FT_ASSERT_EQ(-4.0 * smallest, tiny_derivative);
    return (1);
}

FT_TEST(test_math_dual_number_move_and_thread_safety_preserve_math_errors)
{
    const ft_dual_number input(-1.0, 1.0);
    ft_dual_number *failed = input.apply_log();
    FT_ASSERT(failed != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, failed->enable_thread_safety());
    ft_dual_number moved;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.initialize(static_cast<ft_dual_number &&>(*failed)));
    FT_ASSERT_EQ(FT_TRUE, failed->is_thread_safe());
    FT_ASSERT_EQ(0.0, failed->value());
    delete failed;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.disable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, (moved + input).get_error());
    return (1);
}

FT_TEST(test_math_dual_number_move_lock_failure_leaves_source_intact)
{
    ft_dual_number source(7.0, 2.0);
    ft_dual_number destination(9.0, 3.0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_INTERNAL);
    const int32_t error = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_INTERNAL, error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(7.0, source.value());
    FT_ASSERT_EQ(2.0, source.derivative());
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());
    return (1);
}
