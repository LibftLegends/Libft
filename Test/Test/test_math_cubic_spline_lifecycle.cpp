#include "../test_internal.hpp"
#include "../../Modules/Math/ft_cubic_spline.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_cubic_spline_initialize_destroy_cycle)
{
    ft_cubic_spline spline;

    FT_ASSERT_EQ(FT_CLASS_STATE_UNINITIALISED, spline._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, spline.initialize());
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, spline._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, spline.destroy());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, spline._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, spline.destroy());
    return (1);
}

FT_TEST(test_math_cubic_spline_copy_move_initialise)
{
    ft_cubic_spline source;
    ft_cubic_spline copied;
    ft_cubic_spline moved;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, copied._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.initialize(static_cast<ft_cubic_spline &&>(source)));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, moved._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    return (1);
}

FT_TEST(test_math_cubic_spline_thread_safety_toggle)
{
    ft_cubic_spline spline;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, spline.initialize());
    FT_ASSERT_EQ(FT_FALSE, spline.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, spline.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, spline.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, spline.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, spline.is_thread_safe());
    return (1);
}

FT_TEST(test_math_cubic_spline_move_transfers_heap_storage)
{
    ft_cubic_spline source;
    ft_cubic_spline destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    source._x_values.resize(128U, 9.0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source._x_values.get_error());
    const double *source_storage = source._x_values.begin();

    cma_set_alloc_limit(1U);
    const int32_t error = destination.move(source);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error);
    FT_ASSERT(destination._x_values.begin() == source_storage);
    FT_ASSERT_EQ(static_cast<ft_size_t>(128), destination._x_values.size());
    FT_ASSERT_EQ(9.0, destination._x_values[127]);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_TRUE, source._x_values.empty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source._x_values.push_back(7.0));
    return (1);
}

FT_TEST(test_math_cubic_spline_guarded_move_failure_keeps_source_storage)
{
    ft_cubic_spline source;
    ft_cubic_spline destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    source._x_values.resize(128U, 9.0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source._x_values.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source._x_values.enable_thread_safety());

    cma_set_alloc_limit(1U);
    const int32_t error = destination.move(source);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(static_cast<ft_size_t>(128), source._x_values.size());
    FT_ASSERT_EQ(9.0, source._x_values[127]);
    FT_ASSERT_EQ(FT_TRUE, source._x_values.is_thread_safe());
    return (1);
}
