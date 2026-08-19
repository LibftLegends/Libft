#include "../test_internal.hpp"
#include "../../Modules/Geometry/geometry.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include <atomic>
#include <cmath>
#include <chrono>
#include <csignal>
#include <limits>
#include <new>
#include <thread>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Geometry/aabb.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int geometry_expect_sigabrt(void (*operation)())
{
    return (test_expect_sigabrt_signal(operation));
}

static void aabb_initialize_twice_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.initialize();
    return ;
}

static void aabb_destroy_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.destroy();
    return ;
}

static void aabb_set_bounds_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.set_bounds(0.0, 0.0, 1.0, 1.0);
    return ;
}

static void aabb_destroy_twice_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.destroy();
    return ;
}

static void aabb_destructor_uninitialised_aborts_operation()
{
    aabb box;

    (void)box;
    return ;
}

static void aabb_move_self_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.move(box);
    return ;
}

static void aabb_initialize_copy_from_uninitialised_source_aborts_operation()
{
    aabb source;
    aabb destination;
    (void)destination.initialize(0.0, 0.0, 1.0, 1.0);

    (void)destination.destroy();
    (void)destination.initialize(source);
    return ;
}

static void aabb_initialize_move_from_uninitialised_source_aborts_operation()
{
    aabb source;
    aabb destination;
    (void)destination.initialize(0.0, 0.0, 1.0, 1.0);

    (void)destination.destroy();
    (void)destination.initialize(static_cast<aabb &&>(source));
    return ;
}

static void aabb_initialize_copy_from_destroyed_source_aborts_operation()
{
    aabb source;
    aabb destination;
    (void)destination.initialize(0.0, 0.0, 1.0, 1.0);

    (void)source.initialize(2.0, 3.0, 4.0, 5.0);
    (void)source.destroy();
    (void)destination.destroy();
    (void)destination.initialize(source);
    return ;
}

static void aabb_initialize_move_from_destroyed_source_aborts_operation()
{
    aabb source;
    aabb destination;
    (void)destination.initialize(0.0, 0.0, 1.0, 1.0);

    (void)source.initialize(2.0, 3.0, 4.0, 5.0);
    (void)source.destroy();
    (void)destination.destroy();
    (void)destination.initialize(static_cast<aabb &&>(source));
    return ;
}

static void aabb_get_minimum_x_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.get_minimum_x();
    return ;
}

static void aabb_enable_thread_safety_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.enable_thread_safety();
    return ;
}

static void aabb_initialize_copy_destination_initialised_aborts_operation()
{
    aabb source;
    (void)source.initialize(0.0, 0.0, 1.0, 1.0);
    aabb destination;
    (void)destination.initialize(2.0, 2.0, 3.0, 3.0);

    (void)destination.initialize(source);
    return ;
}

static void aabb_initialize_move_destination_initialised_aborts_operation()
{
    aabb source;
    (void)source.initialize(0.0, 0.0, 1.0, 1.0);
    aabb destination;
    (void)destination.initialize(2.0, 2.0, 3.0, 3.0);

    (void)destination.initialize(static_cast<aabb &&>(source));
    return ;
}

static void aabb_disable_thread_safety_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    box.disable_thread_safety();
    return ;
}

static void aabb_is_thread_safe_enabled_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.is_thread_safe();
    return ;
}

static void aabb_set_minimum_x_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.set_minimum_x(-1.0);
    return ;
}

static void aabb_get_maximum_y_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.get_maximum_y();
    return ;
}

static void aabb_set_maximum_x_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.set_maximum_x(1.0);
    return ;
}

static void aabb_get_minimum_y_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.get_minimum_y();
    return ;
}

static void aabb_enable_thread_safety_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.enable_thread_safety();
    return ;
}

static void aabb_set_maximum_y_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.set_maximum_y(1.0);
    return ;
}

static void aabb_get_maximum_x_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.get_maximum_x();
    return ;
}

static void aabb_disable_thread_safety_uninitialised_aborts_operation()
{
    aabb box;

    box.disable_thread_safety();
    return ;
}

static void aabb_is_thread_safe_enabled_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.is_thread_safe();
    return ;
}

static void aabb_set_minimum_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.set_minimum(0.0, 0.0);
    return ;
}

static void aabb_set_minimum_y_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.set_minimum_y(0.0);
    return ;
}

static void aabb_set_maximum_uninitialised_aborts_operation()
{
    aabb box;

    (void)box.set_maximum(1.0, 1.0);
    return ;
}

static void aabb_move_two_uninitialised_aborts_operation()
{
    aabb source;
    aabb destination;

    (void)destination.move(source);
    return ;
}

static void aabb_set_bounds_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.set_bounds(0.0, 0.0, 1.0, 1.0);
    return ;
}

static void aabb_get_minimum_x_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.get_minimum_x();
    return ;
}

static void aabb_move_destroyed_source_aborts_operation()
{
    aabb source;
    (void)source.initialize(0.0, 0.0, 1.0, 1.0);
    aabb destination;
    (void)destination.initialize(2.0, 2.0, 3.0, 3.0);

    (void)source.destroy();
    (void)destination.move(source);
    return ;
}

static void aabb_set_minimum_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.set_minimum(0.0, 0.0);
    return ;
}

static void aabb_set_minimum_x_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.set_minimum_x(0.0);
    return ;
}

static void aabb_set_minimum_y_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.set_minimum_y(0.0);
    return ;
}

static void aabb_set_maximum_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.set_maximum(1.0, 1.0);
    return ;
}

static void aabb_set_maximum_x_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.set_maximum_x(1.0);
    return ;
}

static void aabb_set_maximum_y_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.set_maximum_y(1.0);
    return ;
}

static void aabb_get_minimum_y_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.get_minimum_y();
    return ;
}

static void aabb_get_maximum_x_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.get_maximum_x();
    return ;
}

static void aabb_get_maximum_y_destroyed_aborts_operation()
{
    aabb box;
    (void)box.initialize(0.0, 0.0, 1.0, 1.0);

    (void)box.destroy();
    (void)box.get_maximum_y();
    return ;
}

static int aabb_move_destroyed_destination_succeeds_operation()
{
    aabb source;
    (void)source.initialize(0.0, 0.0, 1.0, 1.0);
    aabb destination;
    (void)destination.initialize(2.0, 2.0, 3.0, 3.0);
    int move_error;

    (void)destination.destroy();
    move_error = destination.move(source);
    if (move_error != FT_ERR_SUCCESS)
        return (0);
    if (std::fabs(destination.get_minimum_x()) > 0.000001
        || std::fabs(destination.get_minimum_y()) > 0.000001
        || std::fabs(destination.get_maximum_x() - 1.0) > 0.000001
        || std::fabs(destination.get_maximum_y() - 1.0) > 0.000001)
        return (0);
    if (std::fabs(source.get_minimum_x()) > 0.000001
        || std::fabs(source.get_minimum_y()) > 0.000001
        || std::fabs(source.get_maximum_x()) > 0.000001
        || std::fabs(source.get_maximum_y()) > 0.000001)
        return (0);
    return (1);
}

static void aabb_move_uninitialised_source_aborts_operation()
{
    aabb source;
    aabb destination;
    (void)destination.initialize(2.0, 2.0, 3.0, 3.0);

    (void)destination.move(source);
    return ;
}

static void intersect_aabb_uninitialised_first_aborts_operation()
{
    aabb first;
    aabb second;
    (void)second.initialize(0.0, 0.0, 1.0, 1.0);

    (void)intersect_aabb(first, second);
    return ;
}

static void intersect_aabb_uninitialised_second_aborts_operation()
{
    aabb first;
    (void)first.initialize(0.0, 0.0, 1.0, 1.0);
    aabb second;

    (void)intersect_aabb(first, second);
    return ;
}

static void intersect_aabb_uninitialised_both_aborts_operation()
{
    aabb first;
    aabb second;

    (void)intersect_aabb(first, second);
    return ;
}

static void intersect_aabb_destroyed_first_aborts_operation()
{
    aabb first;
    (void)first.initialize(0.0, 0.0, 1.0, 1.0);
    aabb second;
    (void)second.initialize(0.0, 0.0, 1.0, 1.0);

    (void)first.destroy();
    (void)intersect_aabb(first, second);
    return ;
}

static void intersect_aabb_destroyed_second_aborts_operation()
{
    aabb first;
    (void)first.initialize(0.0, 0.0, 1.0, 1.0);
    aabb second;
    (void)second.initialize(0.0, 0.0, 1.0, 1.0);

    (void)second.destroy();
    (void)intersect_aabb(first, second);
    return ;
}

static void intersect_aabb_destroyed_both_aborts_operation()
{
    aabb first;
    (void)first.initialize(0.0, 0.0, 1.0, 1.0);
    aabb second;
    (void)second.initialize(0.0, 0.0, 1.0, 1.0);

    (void)first.destroy();
    (void)second.destroy();
    (void)intersect_aabb(first, second);
    return ;
}

static void intersect_aabb_destroyed_and_uninitialised_aborts_operation()
{
    aabb first;
    (void)first.initialize(0.0, 0.0, 1.0, 1.0);
    aabb second;

    (void)first.destroy();
    (void)intersect_aabb(first, second);
    return ;
}

FT_TEST(test_aabb_initialize_setters_and_getters)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_bounds(-3.0, -2.0, 6.0, 9.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_minimum(-2.0, -1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_maximum(7.0, 10.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_minimum_x(-1.5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_minimum_y(-0.5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_maximum_x(8.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_maximum_y(11.0));
    FT_ASSERT_DOUBLE_EQ(-1.5, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-0.5, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(8.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(11.0, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_invalid_bounds_preserve_state)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_bounds(-2.0, -3.0, 4.0, 6.0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, box.set_bounds(8.0, -3.0, 4.0, 6.0));
    FT_ASSERT_DOUBLE_EQ(-2.0, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-3.0, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(4.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(6.0, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_invalid_minimum_maximum_setters_preserve_state)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-2.0, -3.0, 4.0, 6.0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, box.set_minimum_x(5.0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, box.set_minimum_y(7.0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, box.set_maximum_x(-3.0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, box.set_maximum_y(-4.0));
    FT_ASSERT_DOUBLE_EQ(-2.0, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-3.0, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(4.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(6.0, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_reinitialize_after_destroy)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(1.0, 2.0, 3.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-7.0, -8.0, 9.0, 10.0));
    FT_ASSERT_DOUBLE_EQ(-7.0, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-8.0, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(9.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(10.0, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_constructor_invalid_bounds_allows_reinitialize)
{
    aabb box;
    (void)box.initialize(5.0, 0.0, 1.0, 1.0);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 2.0, 2.0));
    FT_ASSERT_DOUBLE_EQ(0.0, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(0.0, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(2.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(2.0, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_large_finite_bounds_roundtrip)
{
    aabb box;
    double large_value;

    large_value = std::numeric_limits<double>::max() / 4.0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(
            -large_value, -large_value / 2.0, large_value / 2.0, large_value));
    FT_ASSERT_DOUBLE_EQ(-large_value, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-large_value / 2.0, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(large_value / 2.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(large_value, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_smallest_normal_bounds_roundtrip)
{
    aabb box;
    double small_value;

    small_value = std::numeric_limits<double>::min();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(
            -small_value, -small_value, small_value, small_value));
    FT_ASSERT_DOUBLE_EQ(-small_value, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-small_value, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(small_value, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(small_value, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_infinite_maximum_roundtrip)
{
    aabb box;
    double infinity_value;

    infinity_value = std::numeric_limits<double>::infinity();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, infinity_value, infinity_value));
    FT_ASSERT(std::isinf(box.get_maximum_x()));
    FT_ASSERT(std::isinf(box.get_maximum_y()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_set_minimum_x_negative_infinity)
{
    aabb box;
    double negative_infinity;

    negative_infinity = -std::numeric_limits<double>::infinity();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_minimum_x(negative_infinity));
    FT_ASSERT(std::isinf(box.get_minimum_x()));
    FT_ASSERT(std::signbit(box.get_minimum_x()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_set_maximum_x_infinity)
{
    aabb box;
    double infinity_value;

    infinity_value = std::numeric_limits<double>::infinity();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_maximum_x(infinity_value));
    FT_ASSERT(std::isinf(box.get_maximum_x()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_negative_zero_minimum_x_sign_preserved)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_minimum_x(-0.0));
    FT_ASSERT(std::signbit(box.get_minimum_x()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_negative_zero_maximum_y_sign_preserved)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.set_maximum_y(-0.0));
    FT_ASSERT(std::signbit(box.get_maximum_y()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_lifecycle_multiple_reinitialize_cycles)
{
    aabb box;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(1.0, 2.0, 3.0, 4.0));
    iteration_index = 0;
    while (iteration_index < 6)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(
                -1.0 - iteration_index,
                -2.0 - iteration_index,
                3.0 + iteration_index,
                4.0 + iteration_index));
        FT_ASSERT_DOUBLE_EQ(-1.0 - iteration_index, box.get_minimum_x());
        FT_ASSERT_DOUBLE_EQ(-2.0 - iteration_index, box.get_minimum_y());
        FT_ASSERT_DOUBLE_EQ(3.0 + iteration_index, box.get_maximum_x());
        FT_ASSERT_DOUBLE_EQ(4.0 + iteration_index, box.get_maximum_y());
        iteration_index = iteration_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_lifecycle_thread_safety_cycles)
{
    aabb box;
    int iteration_index;

    iteration_index = 0;
    while (iteration_index < 6)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(
                -1.0 - iteration_index,
                -2.0 - iteration_index,
                3.0 + iteration_index,
                4.0 + iteration_index));
        FT_ASSERT_EQ(false, box.is_thread_safe());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
        FT_ASSERT_EQ(true, box.is_thread_safe());
        box.disable_thread_safety();
        FT_ASSERT_EQ(false, box.is_thread_safe());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
        iteration_index = iteration_index + 1;
    }
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_failure_then_recovery)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    cma_set_alloc_limit(1);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    FT_ASSERT_EQ(true, box.is_thread_safe());
    box.disable_thread_safety();
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_repeated_alloc_failures)
{
    aabb box;
    int error_code;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    cma_set_alloc_limit(1);
    iteration_index = 0;
    while (iteration_index < 3)
    {
        error_code = box.enable_thread_safety();
        FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
        FT_ASSERT_EQ(false, box.is_thread_safe());
        iteration_index = iteration_index + 1;
    }
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_failure_then_destroy)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-2.0, -2.0, 2.0, 2.0));
    cma_set_alloc_limit(1);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_failure_after_disable)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    box.disable_thread_safety();
    FT_ASSERT_EQ(false, box.is_thread_safe());
    cma_set_alloc_limit(1);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_get_mutex_for_testing_alloc_failure_returns_null)
{
    aabb box;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    cma_set_alloc_limit(1);
    mutex_pointer = box._mutex;
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, mutex_pointer);
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_get_mutex_for_testing_recovers_after_alloc_failure)
{
    aabb box;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    cma_set_alloc_limit(1);
    mutex_pointer = box._mutex;
    FT_ASSERT_EQ(ft_nullptr, mutex_pointer);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    mutex_pointer = box._mutex;
    FT_ASSERT_NEQ(ft_nullptr, mutex_pointer);
    FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_failure_reinit_cycle)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-3.0, -3.0, 3.0, 3.0));
    cma_set_alloc_limit(1);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-4.0, -4.0, 4.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_failure_preserves_bounds)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-5.0, -6.0, 7.0, 8.0));
    cma_set_alloc_limit(1);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_DOUBLE_EQ(-5.0, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-6.0, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(7.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(8.0, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_two_state_consistent)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    cma_set_alloc_limit(2);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, box.is_thread_safe());
    else
        FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_three_state_consistent)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    cma_set_alloc_limit(3);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, box.is_thread_safe());
    else
        FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_two_then_recover)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-2.0, -2.0, 2.0, 2.0));
    cma_set_alloc_limit(2);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        FT_ASSERT_EQ(true, box.is_thread_safe());
        box.disable_thread_safety();
    }
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_three_then_recover)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-2.0, -2.0, 2.0, 2.0));
    cma_set_alloc_limit(3);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        FT_ASSERT_EQ(true, box.is_thread_safe());
        box.disable_thread_safety();
    }
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_get_mutex_for_testing_alloc_limit_two_consistent)
{
    aabb box;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    cma_set_alloc_limit(2);
    mutex_pointer = box._mutex;
    cma_set_alloc_limit(0);
    if (mutex_pointer == ft_nullptr)
        FT_ASSERT_EQ(false, box.is_thread_safe());
    else
        FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_get_mutex_for_testing_alloc_limit_three_consistent)
{
    aabb box;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    cma_set_alloc_limit(3);
    mutex_pointer = box._mutex;
    cma_set_alloc_limit(0);
    if (mutex_pointer == ft_nullptr)
        FT_ASSERT_EQ(false, box.is_thread_safe());
    else
        FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_alloc_limit_two_preserves_bounds)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-9.0, -8.0, 7.0, 6.0));
    cma_set_alloc_limit(2);
    (void)box.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_DOUBLE_EQ(-9.0, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-8.0, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(7.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(6.0, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_four_state_consistent)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    cma_set_alloc_limit(4);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, box.is_thread_safe());
    else
        FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_five_state_consistent)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    cma_set_alloc_limit(5);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, box.is_thread_safe());
    else
        FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_six_state_consistent)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    cma_set_alloc_limit(6);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, box.is_thread_safe());
    else
        FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_four_then_recover)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-2.0, -2.0, 2.0, 2.0));
    cma_set_alloc_limit(4);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        box.disable_thread_safety();
        FT_ASSERT_EQ(false, box.is_thread_safe());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_five_then_recover)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-2.0, -2.0, 2.0, 2.0));
    cma_set_alloc_limit(5);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        box.disable_thread_safety();
        FT_ASSERT_EQ(false, box.is_thread_safe());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_get_mutex_for_testing_alloc_limit_four_consistent)
{
    aabb box;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    cma_set_alloc_limit(4);
    mutex_pointer = box._mutex;
    cma_set_alloc_limit(0);
    if (mutex_pointer == ft_nullptr)
        FT_ASSERT_EQ(false, box.is_thread_safe());
    else
        FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_alloc_limit_four_preserves_bounds)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-9.0, -8.0, 7.0, 6.0));
    cma_set_alloc_limit(4);
    (void)box.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_DOUBLE_EQ(-9.0, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-8.0, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(7.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(6.0, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_seven_state_consistent)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    cma_set_alloc_limit(7);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, box.is_thread_safe());
    else
        FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_eight_state_consistent)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    cma_set_alloc_limit(8);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, box.is_thread_safe());
    else
        FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_nine_state_consistent)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    cma_set_alloc_limit(9);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, box.is_thread_safe());
    else
        FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_seven_then_recover)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-2.0, -2.0, 2.0, 2.0));
    cma_set_alloc_limit(7);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        box.disable_thread_safety();
        FT_ASSERT_EQ(false, box.is_thread_safe());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_alloc_limit_eight_then_recover)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-2.0, -2.0, 2.0, 2.0));
    cma_set_alloc_limit(8);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        box.disable_thread_safety();
        FT_ASSERT_EQ(false, box.is_thread_safe());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_get_mutex_for_testing_alloc_limit_seven_consistent)
{
    aabb box;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    cma_set_alloc_limit(7);
    mutex_pointer = box._mutex;
    cma_set_alloc_limit(0);
    if (mutex_pointer == ft_nullptr)
        FT_ASSERT_EQ(false, box.is_thread_safe());
    else
        FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_alloc_limit_seven_preserves_bounds)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-9.0, -8.0, 7.0, 6.0));
    cma_set_alloc_limit(7);
    (void)box.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_DOUBLE_EQ(-9.0, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-8.0, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(7.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(6.0, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_overlap)
{
    aabb first;
    aabb second;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 5.0, 5.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(3.0, 3.0, 8.0, 8.0));
    FT_ASSERT(intersect_aabb(first, second));
    FT_ASSERT(intersect_aabb(second, first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_separated)
{
    aabb first;
    aabb second;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(-2.0, -2.0, 2.0, 2.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(3.0, -1.0, 6.0, 1.0));
    FT_ASSERT_EQ(false, intersect_aabb(first, second));
    FT_ASSERT_EQ(false, intersect_aabb(second, first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_touching_edge)
{
    aabb first;
    aabb second;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 4.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(4.0, 1.0, 7.0, 3.0));
    FT_ASSERT(intersect_aabb(first, second));
    FT_ASSERT(intersect_aabb(second, first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_parallel_access)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_ready;
    std::atomic<bool> worker_failed;
    std::atomic<bool> worker_completed;
    std::thread worker_thread;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 10.0, 10.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(2.0, 2.0, 8.0, 8.0));
    worker_ready.store(false);
    worker_failed.store(false);
    worker_completed.store(false);
    worker_thread = std::thread([&first, &second, &worker_ready, &worker_failed,
                &worker_completed]() {
        int local_iteration_index;

        worker_ready.store(true);
        local_iteration_index = 0;
        while (local_iteration_index < 256 && worker_failed.load() == false)
        {
            if (intersect_aabb(first, second) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        worker_completed.store(true);
        return ;
    });
    while (worker_ready.load() == false)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    iteration_index = 0;
    while (iteration_index < 256)
    {
        if (intersect_aabb(second, first) == false)
        {
            worker_failed.store(true);
            break;
        }
        iteration_index = iteration_index + 1;
    }
    worker_thread.join();
    FT_ASSERT(worker_completed.load());
    FT_ASSERT_EQ(false, worker_failed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_high_load_overlap_two_threads)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread worker_thread;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 10.0, 10.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(2.0, 2.0, 8.0, 8.0));
    worker_failed.store(false);
    worker_thread = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 8192 && worker_failed.load() == false)
        {
            if (intersect_aabb(first, second) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    iteration_index = 0;
    while (iteration_index < 8192 && worker_failed.load() == false)
    {
        if (intersect_aabb(second, first) == false)
        {
            worker_failed.store(true);
            break;
        }
        iteration_index = iteration_index + 1;
    }
    worker_thread.join();
    FT_ASSERT_EQ(false, worker_failed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_high_load_overlap_four_threads)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread worker_thread_one;
    std::thread worker_thread_two;
    std::thread worker_thread_three;
    std::thread worker_thread_four;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 16.0, 16.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(1.0, 1.0, 15.0, 15.0));
    worker_failed.store(false);
    worker_thread_one = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 4096 && worker_failed.load() == false)
        {
            if (intersect_aabb(first, second) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    worker_thread_two = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 4096 && worker_failed.load() == false)
        {
            if (intersect_aabb(second, first) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    worker_thread_three = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 4096 && worker_failed.load() == false)
        {
            if (intersect_aabb(first, second) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    worker_thread_four = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 4096 && worker_failed.load() == false)
        {
            if (intersect_aabb(second, first) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    worker_thread_one.join();
    worker_thread_two.join();
    worker_thread_three.join();
    worker_thread_four.join();
    FT_ASSERT_EQ(false, worker_failed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_high_load_touching_two_threads)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread worker_thread;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 4.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(4.0, 1.0, 9.0, 3.0));
    worker_failed.store(false);
    worker_thread = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 6144 && worker_failed.load() == false)
        {
            if (intersect_aabb(first, second) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    iteration_index = 0;
    while (iteration_index < 6144 && worker_failed.load() == false)
    {
        if (intersect_aabb(second, first) == false)
        {
            worker_failed.store(true);
            break;
        }
        iteration_index = iteration_index + 1;
    }
    worker_thread.join();
    FT_ASSERT_EQ(false, worker_failed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_high_load_separated_two_threads)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread worker_thread;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(-2.0, -2.0, 2.0, 2.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(3.0, -1.0, 6.0, 1.0));
    worker_failed.store(false);
    worker_thread = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 6144 && worker_failed.load() == false)
        {
            if (intersect_aabb(first, second) != false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    iteration_index = 0;
    while (iteration_index < 6144 && worker_failed.load() == false)
    {
        if (intersect_aabb(second, first) != false)
        {
            worker_failed.store(true);
            break;
        }
        iteration_index = iteration_index + 1;
    }
    worker_thread.join();
    FT_ASSERT_EQ(false, worker_failed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_aabb_move_bidirectional_high_load_with_thread_safety)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread worker_thread;
    int iteration_index;
    int iteration_limit;
    int move_error;

    iteration_limit = 1024;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(-3.0, -3.0, 3.0, 3.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(-4.0, -4.0, 4.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.enable_thread_safety());
    worker_failed.store(false);
    std::chrono::steady_clock::time_point start_time;
    start_time = std::chrono::steady_clock::now();
    worker_thread = std::thread([&first, &second, &worker_failed, &iteration_limit]() {
        int local_iteration_index;
        int local_move_error;

        local_iteration_index = 0;
        while (local_iteration_index < iteration_limit && worker_failed.load() == false)
        {
            local_move_error = first.move(second);
            if (local_move_error != FT_ERR_SUCCESS)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    iteration_index = 0;
    while (iteration_index < iteration_limit && worker_failed.load() == false)
    {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count() > 2000)
        {
            worker_failed.store(true);
            break;
        }
        move_error = second.move(first);
        if (move_error != FT_ERR_SUCCESS)
        {
            worker_failed.store(true);
            break;
        }
        iteration_index = iteration_index + 1;
    }
    worker_thread.join();
    FT_ASSERT_EQ(false, worker_failed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_aabb_set_bounds_getters_contention_high_load_two_threads)
{
    struct aabb_contention_test_context
    {
        std::atomic<int> result;
    };
    aabb_contention_test_context context;
    pthread_t test_thread;
    int32_t join_result;
    const long join_timeout_ms = 5000;

    context.result.store(0);
    FT_ASSERT_EQ(0, pt_thread_create(&test_thread, ft_nullptr,
        [](void *argument) -> void *
        {
            aabb_contention_test_context *context_pointer;
            aabb box;
            std::atomic<bool> worker_failed;
            std::thread writer_thread;
            int iteration_index;
            double minimum_x;
            double minimum_y;
            double maximum_x;
            double maximum_y;
            int set_error;

            context_pointer = static_cast<aabb_contention_test_context *>(argument);
            if (context_pointer == ft_nullptr)
                return (ft_nullptr);
            if (box.initialize(-1.0, -1.0, 1.0, 1.0) != FT_ERR_SUCCESS)
            {
                context_pointer->result.store(0);
                return (ft_nullptr);
            }
            if (box.enable_thread_safety() != FT_ERR_SUCCESS)
            {
                context_pointer->result.store(0);
                return (ft_nullptr);
            }
            worker_failed.store(false);
            writer_thread = std::thread([&box, &worker_failed]() {
                int local_iteration_index;
                int local_set_error;

                local_iteration_index = 0;
                while (local_iteration_index < 4096 && worker_failed.load() == false)
                {
                    if ((local_iteration_index % 2) == 0)
                        local_set_error = box.set_bounds(-2.0, -2.0, 2.0, 2.0);
                    else
                        local_set_error = box.set_bounds(-3.0, -1.0, 3.0, 1.0);
                    if (local_set_error != FT_ERR_SUCCESS)
                    {
                        worker_failed.store(true);
                        break;
                    }
                    local_iteration_index = local_iteration_index + 1;
                }
                return ;
            });
            iteration_index = 0;
            while (iteration_index < 4096 && worker_failed.load() == false)
            {
                minimum_x = box.get_minimum_x();
                minimum_y = box.get_minimum_y();
                maximum_x = box.get_maximum_x();
                maximum_y = box.get_maximum_y();
                if (minimum_x > maximum_x || minimum_y > maximum_y)
                {
                    worker_failed.store(true);
                    break;
                }
                iteration_index = iteration_index + 1;
            }
            writer_thread.join();
            if (worker_failed.load() != false)
            {
                context_pointer->result.store(0);
                return (ft_nullptr);
            }
            set_error = box.set_bounds(-4.0, -4.0, 4.0, 4.0);
            if (set_error != FT_ERR_SUCCESS)
            {
                context_pointer->result.store(0);
                return (ft_nullptr);
            }
            if (box.destroy() != FT_ERR_SUCCESS)
            {
                context_pointer->result.store(0);
                return (ft_nullptr);
            }
            context_pointer->result.store(1);
            return (ft_nullptr);
        }, &context));
    join_result = pt_thread_timed_join(test_thread, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
        (void)pt_thread_detach(test_thread);
    FT_ASSERT_EQ(0, join_result);
    FT_ASSERT_EQ(1, context.result.load());
    return (1);
}

FT_TEST(test_intersect_aabb_high_load_with_mutating_overlap)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread writer_thread;
    std::thread reader_thread_one;
    std::thread reader_thread_two;
    int iteration_limit;

    iteration_limit = 1024;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(-10.0, -10.0, 10.0, 10.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(-2.0, -2.0, 2.0, 2.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.enable_thread_safety());
    worker_failed.store(false);
    writer_thread = std::thread([&second, &worker_failed, &iteration_limit]() {
        int local_iteration_index;
        int local_set_error;

        local_iteration_index = 0;
        while (local_iteration_index < iteration_limit && worker_failed.load() == false)
        {
            if ((local_iteration_index % 2) == 0)
                local_set_error = second.set_bounds(-3.0, -3.0, 3.0, 3.0);
            else
                local_set_error = second.set_bounds(-4.0, -1.0, 4.0, 1.0);
            if (local_set_error != FT_ERR_SUCCESS)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    reader_thread_one = std::thread([&first, &second, &worker_failed, &iteration_limit]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < iteration_limit && worker_failed.load() == false)
        {
            if (intersect_aabb(first, second) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    reader_thread_two = std::thread([&first, &second, &worker_failed, &iteration_limit]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < iteration_limit && worker_failed.load() == false)
        {
            if (intersect_aabb(second, first) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    writer_thread.join();
    reader_thread_one.join();
    reader_thread_two.join();
    FT_ASSERT_EQ(false, worker_failed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_aabb_move_ring_high_load_three_threads)
{
    struct s_aabb_ring_worker_args
    {
        aabb               *source;
        aabb               *destination;
        std::atomic<bool>  *worker_failed;
        int                iteration_limit;
    };
    aabb first;
    aabb second;
    aabb third;
    std::atomic<bool> worker_failed;
    s_aabb_ring_worker_args thread_one_args;
    s_aabb_ring_worker_args thread_two_args;
    s_aabb_ring_worker_args thread_three_args;
    pthread_t thread_one;
    pthread_t thread_two;
    pthread_t thread_three;
    int thread_create_result;
    int join_result;
    long join_timeout_ms;
    void *(*ring_worker)(void *);

    ring_worker = [](void *argument) -> void *
    {
        s_aabb_ring_worker_args *worker_args;
        int local_iteration_index;
        int local_move_error;

        worker_args = static_cast<s_aabb_ring_worker_args *>(argument);
        local_iteration_index = 0;
        while (local_iteration_index < worker_args->iteration_limit
            && worker_args->worker_failed->load() == false)
        {
            local_move_error = worker_args->source->move(*worker_args->destination);
            if (local_move_error != FT_ERR_SUCCESS)
            {
                worker_args->worker_failed->store(true);
                break ;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return (ft_nullptr);
    };
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(-1.0, -1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(-2.0, -2.0, 2.0, 2.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, third.initialize(-3.0, -3.0, 3.0, 3.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, third.enable_thread_safety());
    worker_failed.store(false);
    thread_one_args.source = &first;
    thread_one_args.destination = &second;
    thread_one_args.worker_failed = &worker_failed;
    thread_one_args.iteration_limit = 2048;
    thread_two_args.source = &second;
    thread_two_args.destination = &third;
    thread_two_args.worker_failed = &worker_failed;
    thread_two_args.iteration_limit = 2048;
    thread_three_args.source = &third;
    thread_three_args.destination = &first;
    thread_three_args.worker_failed = &worker_failed;
    thread_three_args.iteration_limit = 2048;
    thread_create_result = pt_thread_create(&thread_one, ft_nullptr,
            ring_worker, &thread_one_args);
    FT_ASSERT_EQ(0, thread_create_result);
    thread_create_result = pt_thread_create(&thread_two, ft_nullptr,
            ring_worker, &thread_two_args);
    FT_ASSERT_EQ(0, thread_create_result);
    thread_create_result = pt_thread_create(&thread_three, ft_nullptr,
            ring_worker, &thread_three_args);
    FT_ASSERT_EQ(0, thread_create_result);
    join_timeout_ms = 30000;
    join_result = pt_thread_timed_join(thread_one, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
    {
        worker_failed.store(true);
        (void)pt_thread_cancel(thread_one);
    }
    FT_ASSERT_EQ(0, join_result);
    join_result = pt_thread_timed_join(thread_two, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
    {
        worker_failed.store(true);
        (void)pt_thread_cancel(thread_two);
    }
    FT_ASSERT_EQ(0, join_result);
    join_result = pt_thread_timed_join(thread_three, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
    {
        worker_failed.store(true);
        (void)pt_thread_cancel(thread_three);
    }
    FT_ASSERT_EQ(0, join_result);
    FT_ASSERT_EQ(false, worker_failed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, third.destroy());
    return (1);
}

FT_TEST(test_aabb_set_bounds_getters_contention_high_load_four_threads)
{
    struct s_aabb_contention_worker_args
    {
        aabb               *box;
        std::atomic<bool>  *worker_failed;
        int                iteration_limit;
    };
    aabb *box;
    std::atomic<bool> *worker_failed;
    s_aabb_contention_worker_args *writer_args;
    s_aabb_contention_worker_args *reader_args_one;
    s_aabb_contention_worker_args *reader_args_two;
    s_aabb_contention_worker_args *reader_args_three;
    pthread_t writer_thread;
    pthread_t reader_thread_one;
    pthread_t reader_thread_two;
    pthread_t reader_thread_three;
    int thread_create_result;
    int join_result;
    long join_timeout_ms;
    void *(*writer_worker)(void *);
    void *(*reader_worker_one)(void *);
    void *(*reader_worker_two)(void *);
    void *(*reader_worker_three)(void *);

    writer_worker = [](void *argument) -> void *
    {
        s_aabb_contention_worker_args *worker_args;
        int local_iteration_index;
        int local_set_error;

        worker_args = static_cast<s_aabb_contention_worker_args *>(argument);
        local_iteration_index = 0;
        while (local_iteration_index < worker_args->iteration_limit
            && worker_args->worker_failed->load() == false)
        {
            if ((local_iteration_index % 2) == 0)
                local_set_error = worker_args->box->set_bounds(-2.0, -2.0, 2.0, 2.0);
            else
                local_set_error = worker_args->box->set_bounds(-3.0, -1.0, 3.0, 1.0);
            if (local_set_error != FT_ERR_SUCCESS)
            {
                worker_args->worker_failed->store(true);
                break ;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return (ft_nullptr);
    };
    reader_worker_one = [](void *argument) -> void *
    {
        s_aabb_contention_worker_args *worker_args;
        int local_iteration_index;

        worker_args = static_cast<s_aabb_contention_worker_args *>(argument);
        local_iteration_index = 0;
        while (local_iteration_index < worker_args->iteration_limit
            && worker_args->worker_failed->load() == false)
        {
            if (worker_args->box->get_minimum_x() > worker_args->box->get_maximum_x())
            {
                worker_args->worker_failed->store(true);
                break ;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return (ft_nullptr);
    };
    reader_worker_two = [](void *argument) -> void *
    {
        s_aabb_contention_worker_args *worker_args;
        int local_iteration_index;

        worker_args = static_cast<s_aabb_contention_worker_args *>(argument);
        local_iteration_index = 0;
        while (local_iteration_index < worker_args->iteration_limit
            && worker_args->worker_failed->load() == false)
        {
            if (worker_args->box->get_minimum_y() > worker_args->box->get_maximum_y())
            {
                worker_args->worker_failed->store(true);
                break ;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return (ft_nullptr);
    };
    reader_worker_three = [](void *argument) -> void *
    {
        s_aabb_contention_worker_args *worker_args;
        int local_iteration_index;
        double minimum_x;
        double maximum_x;

        worker_args = static_cast<s_aabb_contention_worker_args *>(argument);
        local_iteration_index = 0;
        while (local_iteration_index < worker_args->iteration_limit
            && worker_args->worker_failed->load() == false)
        {
            minimum_x = worker_args->box->get_minimum_x();
            maximum_x = worker_args->box->get_maximum_x();
            if (minimum_x > maximum_x)
            {
                worker_args->worker_failed->store(true);
                break ;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return (ft_nullptr);
    };
    box = new (std::nothrow) aabb();
    worker_failed = new (std::nothrow) std::atomic<bool>();
    writer_args = new (std::nothrow) s_aabb_contention_worker_args();
    reader_args_one = new (std::nothrow) s_aabb_contention_worker_args();
    reader_args_two = new (std::nothrow) s_aabb_contention_worker_args();
    reader_args_three = new (std::nothrow) s_aabb_contention_worker_args();
    FT_ASSERT(box != ft_nullptr);
    FT_ASSERT(worker_failed != ft_nullptr);
    FT_ASSERT(writer_args != ft_nullptr);
    FT_ASSERT(reader_args_one != ft_nullptr);
    FT_ASSERT(reader_args_two != ft_nullptr);
    FT_ASSERT(reader_args_three != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box->initialize(-1.0, -1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box->enable_thread_safety());
    worker_failed->store(false);
    writer_args->box = box;
    writer_args->worker_failed = worker_failed;
    writer_args->iteration_limit = 3072;
    reader_args_one->box = box;
    reader_args_one->worker_failed = worker_failed;
    reader_args_one->iteration_limit = 3072;
    reader_args_two->box = box;
    reader_args_two->worker_failed = worker_failed;
    reader_args_two->iteration_limit = 3072;
    reader_args_three->box = box;
    reader_args_three->worker_failed = worker_failed;
    reader_args_three->iteration_limit = 3072;
    thread_create_result = pt_thread_create(&writer_thread, ft_nullptr,
            writer_worker, writer_args);
    FT_ASSERT_EQ(0, thread_create_result);
    thread_create_result = pt_thread_create(&reader_thread_one, ft_nullptr,
            reader_worker_one, reader_args_one);
    FT_ASSERT_EQ(0, thread_create_result);
    thread_create_result = pt_thread_create(&reader_thread_two, ft_nullptr,
            reader_worker_two, reader_args_two);
    FT_ASSERT_EQ(0, thread_create_result);
    thread_create_result = pt_thread_create(&reader_thread_three, ft_nullptr,
            reader_worker_three, reader_args_three);
    FT_ASSERT_EQ(0, thread_create_result);
    join_timeout_ms = 5000;
    join_result = pt_thread_timed_join(writer_thread, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
    {
        worker_failed->store(true);
        (void)pt_thread_cancel(writer_thread);
    }
    FT_ASSERT_EQ(0, join_result);
    join_result = pt_thread_timed_join(reader_thread_one, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
    {
        worker_failed->store(true);
        (void)pt_thread_cancel(reader_thread_one);
    }
    FT_ASSERT_EQ(0, join_result);
    join_result = pt_thread_timed_join(reader_thread_two, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
    {
        worker_failed->store(true);
        (void)pt_thread_cancel(reader_thread_two);
    }
    FT_ASSERT_EQ(0, join_result);
    join_result = pt_thread_timed_join(reader_thread_three, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
    {
        worker_failed->store(true);
        (void)pt_thread_cancel(reader_thread_three);
    }
    FT_ASSERT_EQ(0, join_result);
    FT_ASSERT_EQ(false, worker_failed->load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box->destroy());
    delete reader_args_three;
    delete reader_args_two;
    delete reader_args_one;
    delete writer_args;
    delete worker_failed;
    delete box;
    return (1);
}

FT_TEST(test_aabb_copy_and_move_initializers)
{
    aabb source;
    aabb copied_destination;
    aabb moved_destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(-5.0, -4.0, 12.0, 14.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_destination.initialize(source));
    FT_ASSERT_DOUBLE_EQ(-5.0, copied_destination.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-4.0, copied_destination.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(12.0, copied_destination.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(14.0, copied_destination.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_destination.initialize(static_cast<aabb &&>(source)));
    FT_ASSERT_DOUBLE_EQ(-5.0, moved_destination.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-4.0, moved_destination.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(12.0, moved_destination.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(14.0, moved_destination.get_maximum_y());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_destination.destroy());
    return (1);
}

FT_TEST(test_aabb_move_method_transfers_values)
{
    aabb source;
    aabb destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(-3.0, -2.0, 9.0, 13.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_DOUBLE_EQ(-3.0, destination.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-2.0, destination.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(9.0, destination.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(13.0, destination.get_maximum_y());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_aabb_thread_safety_enable_disable)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    FT_ASSERT_EQ(true, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    FT_ASSERT_EQ(true, box.is_thread_safe());
    box.disable_thread_safety();
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_initialize_from_destroyed_source_aborts)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            aabb_initialize_copy_from_destroyed_source_aborts_operation));
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            aabb_initialize_move_from_destroyed_source_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_move_self_initialised_is_noop)
{
    aabb box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-3.0, -2.0, 9.0, 13.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.move(box));
    FT_ASSERT_DOUBLE_EQ(-3.0, box.get_minimum_x());
    FT_ASSERT_DOUBLE_EQ(-2.0, box.get_minimum_y());
    FT_ASSERT_DOUBLE_EQ(9.0, box.get_maximum_x());
    FT_ASSERT_DOUBLE_EQ(13.0, box.get_maximum_y());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_mutex_testing_accessor_lifecycle)
{
    aabb box;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    mutex_pointer = box._mutex;
    FT_ASSERT_EQ(ft_nullptr, mutex_pointer);
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    mutex_pointer = box._mutex;
    FT_ASSERT_NEQ(ft_nullptr, mutex_pointer);
    FT_ASSERT_EQ(true, box.is_thread_safe());
    box.disable_thread_safety();
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    FT_ASSERT_EQ(ft_nullptr, box._mutex);
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_allocation_failure)
{
    aabb box;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(0.0, 0.0, 1.0, 1.0));
    cma_set_alloc_limit(1);
    error_code = box.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(false, box.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_aabb_initialize_twice_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(aabb_initialize_twice_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_destroy_uninitialised_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(aabb_destroy_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_bounds_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(aabb_set_bounds_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_destroy_twice_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(aabb_destroy_twice_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_uninitialised_destructor_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(aabb_destructor_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_self_move_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(aabb_move_self_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_initialize_copy_from_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_initialize_copy_from_uninitialised_source_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_initialize_move_from_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_initialize_move_from_uninitialised_source_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_get_minimum_x_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_get_minimum_x_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_enable_thread_safety_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_initialize_copy_destination_initialised_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            aabb_initialize_copy_destination_initialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_initialize_move_destination_initialised_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            aabb_initialize_move_destination_initialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_disable_thread_safety_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_disable_thread_safety_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_is_thread_safe_enabled_destroyed_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            aabb_is_thread_safe_enabled_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_minimum_x_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_minimum_x_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_get_maximum_y_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_get_maximum_y_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_maximum_x_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_maximum_x_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_get_minimum_y_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_get_minimum_y_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_enable_thread_safety_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_enable_thread_safety_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_maximum_y_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_maximum_y_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_get_maximum_x_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_get_maximum_x_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_disable_thread_safety_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_disable_thread_safety_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_is_thread_safe_enabled_uninitialised_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            aabb_is_thread_safe_enabled_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_minimum_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_minimum_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_minimum_y_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_minimum_y_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_maximum_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_maximum_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_move_two_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_move_two_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_bounds_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_bounds_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_get_minimum_x_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_get_minimum_x_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_move_destroyed_source_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            aabb_move_destroyed_source_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_minimum_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_minimum_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_minimum_x_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_minimum_x_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_minimum_y_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_minimum_y_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_maximum_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_maximum_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_maximum_x_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_maximum_x_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_set_maximum_y_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_set_maximum_y_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_get_minimum_y_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_get_minimum_y_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_get_maximum_x_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_get_maximum_x_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_get_maximum_y_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_get_maximum_y_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_aabb_move_destroyed_destination_succeeds)
{
    FT_ASSERT_EQ(1, aabb_move_destroyed_destination_succeeds_operation());
    return (1);
}

FT_TEST(test_aabb_move_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            aabb_move_uninitialised_source_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_aabb_uninitialised_first_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_aabb_uninitialised_first_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_aabb_uninitialised_second_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_aabb_uninitialised_second_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_aabb_uninitialised_both_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_aabb_uninitialised_both_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_aabb_destroyed_first_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_aabb_destroyed_first_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_aabb_destroyed_second_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_aabb_destroyed_second_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_aabb_destroyed_both_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_aabb_destroyed_both_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_aabb_destroyed_and_uninitialised_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_aabb_destroyed_and_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_aabb_high_load_overlap_two_threads_soak_rounds)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread worker_thread;
    int round_index;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 10.0, 10.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(2.0, 2.0, 8.0, 8.0));
    round_index = 0;
    while (round_index < 4)
    {
        worker_failed.store(false);
        worker_thread = std::thread([&first, &second, &worker_failed]() {
            int local_iteration_index;

            local_iteration_index = 0;
            while (local_iteration_index < 4096 && worker_failed.load() == false)
            {
                if (intersect_aabb(first, second) == false)
                {
                    worker_failed.store(true);
                    break;
                }
                local_iteration_index = local_iteration_index + 1;
            }
            return ;
        });
        iteration_index = 0;
        while (iteration_index < 4096 && worker_failed.load() == false)
        {
            if (intersect_aabb(second, first) == false)
            {
                worker_failed.store(true);
                break;
            }
            iteration_index = iteration_index + 1;
        }
        worker_thread.join();
        FT_ASSERT_EQ(false, worker_failed.load());
        round_index = round_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_high_load_overlap_four_threads_soak_rounds)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread thread_one;
    std::thread thread_two;
    std::thread thread_three;
    std::thread thread_four;
    int round_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 12.0, 12.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(1.0, 1.0, 11.0, 11.0));
    round_index = 0;
    while (round_index < 3)
    {
        worker_failed.store(false);
        thread_one = std::thread([&first, &second, &worker_failed]() {
            int index;

            index = 0;
            while (index < 3072 && worker_failed.load() == false)
            {
                if (intersect_aabb(first, second) == false)
                    worker_failed.store(true);
                index = index + 1;
            }
            return ;
        });
        thread_two = std::thread([&first, &second, &worker_failed]() {
            int index;

            index = 0;
            while (index < 3072 && worker_failed.load() == false)
            {
                if (intersect_aabb(second, first) == false)
                    worker_failed.store(true);
                index = index + 1;
            }
            return ;
        });
        thread_three = std::thread([&first, &second, &worker_failed]() {
            int index;

            index = 0;
            while (index < 3072 && worker_failed.load() == false)
            {
                if (intersect_aabb(first, second) == false)
                    worker_failed.store(true);
                index = index + 1;
            }
            return ;
        });
        thread_four = std::thread([&first, &second, &worker_failed]() {
            int index;

            index = 0;
            while (index < 3072 && worker_failed.load() == false)
            {
                if (intersect_aabb(second, first) == false)
                    worker_failed.store(true);
                index = index + 1;
            }
            return ;
        });
        thread_one.join();
        thread_two.join();
        thread_three.join();
        thread_four.join();
        FT_ASSERT_EQ(false, worker_failed.load());
        round_index = round_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_high_load_touching_two_threads_soak_rounds)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread worker_thread;
    int round_index;
    int index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 4.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(4.0, 1.0, 7.0, 3.0));
    round_index = 0;
    while (round_index < 4)
    {
        worker_failed.store(false);
        worker_thread = std::thread([&first, &second, &worker_failed]() {
            int iteration_index;

            iteration_index = 0;
            while (iteration_index < 3072 && worker_failed.load() == false)
            {
                if (intersect_aabb(first, second) == false)
                    worker_failed.store(true);
                iteration_index = iteration_index + 1;
            }
            return ;
        });
        index = 0;
        while (index < 3072 && worker_failed.load() == false)
        {
            if (intersect_aabb(second, first) == false)
                worker_failed.store(true);
            index = index + 1;
        }
        worker_thread.join();
        FT_ASSERT_EQ(false, worker_failed.load());
        round_index = round_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_high_load_separated_two_threads_soak_rounds)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread worker_thread;
    int round_index;
    int index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(-2.0, -2.0, 2.0, 2.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(3.0, -1.0, 6.0, 1.0));
    round_index = 0;
    while (round_index < 4)
    {
        worker_failed.store(false);
        worker_thread = std::thread([&first, &second, &worker_failed]() {
            int iteration_index;

            iteration_index = 0;
            while (iteration_index < 3072 && worker_failed.load() == false)
            {
                if (intersect_aabb(first, second) != false)
                    worker_failed.store(true);
                iteration_index = iteration_index + 1;
            }
            return ;
        });
        index = 0;
        while (index < 3072 && worker_failed.load() == false)
        {
            if (intersect_aabb(second, first) != false)
                worker_failed.store(true);
            index = index + 1;
        }
        worker_thread.join();
        FT_ASSERT_EQ(false, worker_failed.load());
        round_index = round_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_aabb_move_bidirectional_high_load_soak_rounds)
{
    struct aabb_move_soak_test_context
    {
        std::atomic<int> result;
    };
    aabb_move_soak_test_context context;
    pthread_t test_thread;
    int32_t join_result;
    const long join_timeout_ms = 5000;

    context.result.store(0);
    FT_ASSERT_EQ(0, pt_thread_create(&test_thread, ft_nullptr,
        [](void *argument) -> void *
        {
            aabb_move_soak_test_context *context_pointer;
            aabb first;
            aabb second;
            std::atomic<bool> worker_failed;
            std::thread worker_thread;
            int round_index;
            int index;
            int move_error;

            context_pointer = static_cast<aabb_move_soak_test_context *>(argument);
            if (context_pointer == ft_nullptr)
                return (ft_nullptr);
            if (first.initialize(-3.0, -3.0, 3.0, 3.0) != FT_ERR_SUCCESS)
                return (ft_nullptr);
            if (second.initialize(-4.0, -4.0, 4.0, 4.0) != FT_ERR_SUCCESS)
                return (ft_nullptr);
            if (first.enable_thread_safety() != FT_ERR_SUCCESS)
                return (ft_nullptr);
            if (second.enable_thread_safety() != FT_ERR_SUCCESS)
                return (ft_nullptr);
            round_index = 0;
            while (round_index < 3)
            {
                worker_failed.store(false);
                worker_thread = std::thread([&first, &second, &worker_failed]() {
                    int iteration_index;
                    int local_move_error;

                    iteration_index = 0;
                    while (iteration_index < 2048 && worker_failed.load() == false)
                    {
                        local_move_error = first.move(second);
                        if (local_move_error != FT_ERR_SUCCESS)
                            worker_failed.store(true);
                        iteration_index = iteration_index + 1;
                    }
                    return ;
                });
                index = 0;
                while (index < 2048 && worker_failed.load() == false)
                {
                    move_error = second.move(first);
                    if (move_error != FT_ERR_SUCCESS)
                        worker_failed.store(true);
                    index = index + 1;
                }
                worker_thread.join();
                if (worker_failed.load() != false)
                    return (ft_nullptr);
                round_index = round_index + 1;
            }
            if (first.destroy() != FT_ERR_SUCCESS)
                return (ft_nullptr);
            if (second.destroy() != FT_ERR_SUCCESS)
                return (ft_nullptr);
            context_pointer->result.store(1);
            return (ft_nullptr);
        }, &context));
    join_result = pt_thread_timed_join(test_thread, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
        (void)pt_thread_detach(test_thread);
    FT_ASSERT_EQ(0, join_result);
    FT_ASSERT_EQ(1, context.result.load());
    return (1);
}

FT_TEST(test_aabb_set_bounds_getters_contention_high_load_soak_rounds)
{
    aabb box;
    std::atomic<bool> worker_failed;
    std::thread writer_thread;
    int round_index;
    int index;
    int iteration_limit;

    iteration_limit = 768;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.initialize(-1.0, -1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.enable_thread_safety());
    round_index = 0;
    while (round_index < 3)
    {
        worker_failed.store(false);
        writer_thread = std::thread([&box, &worker_failed, &iteration_limit]() {
            int iteration_index;
            int local_set_error;

            iteration_index = 0;
            while (iteration_index < iteration_limit && worker_failed.load() == false)
            {
                if ((iteration_index % 2) == 0)
                    local_set_error = box.set_bounds(-2.0, -2.0, 2.0, 2.0);
                else
                    local_set_error = box.set_bounds(-3.0, -1.0, 3.0, 1.0);
                if (local_set_error != FT_ERR_SUCCESS)
                    worker_failed.store(true);
                iteration_index = iteration_index + 1;
            }
            return ;
        });
        index = 0;
        while (index < iteration_limit && worker_failed.load() == false)
        {
            if (box.get_minimum_x() > box.get_maximum_x()
                || box.get_minimum_y() > box.get_maximum_y())
                worker_failed.store(true);
            index = index + 1;
        }
        writer_thread.join();
        FT_ASSERT_EQ(false, worker_failed.load());
        round_index = round_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, box.destroy());
    return (1);
}

FT_TEST(test_intersect_aabb_high_load_mutating_overlap_soak_rounds)
{
    aabb first;
    aabb second;
    std::atomic<bool> worker_failed;
    std::thread writer_thread;
    std::thread reader_thread;
    int round_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(-10.0, -10.0, 10.0, 10.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(-2.0, -2.0, 2.0, 2.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.enable_thread_safety());
    round_index = 0;
    while (round_index < 3)
    {
        worker_failed.store(false);
        writer_thread = std::thread([&second, &worker_failed]() {
            int iteration_index;
            int local_set_error;

            iteration_index = 0;
            while (iteration_index < 3072 && worker_failed.load() == false)
            {
                if ((iteration_index % 2) == 0)
                    local_set_error = second.set_bounds(-3.0, -3.0, 3.0, 3.0);
                else
                    local_set_error = second.set_bounds(-4.0, -1.0, 4.0, 1.0);
                if (local_set_error != FT_ERR_SUCCESS)
                    worker_failed.store(true);
                iteration_index = iteration_index + 1;
                std::this_thread::yield();
            }
            return ;
        });
        reader_thread = std::thread([&first, &second, &worker_failed]() {
            int iteration_index;

            iteration_index = 0;
            while (iteration_index < 3072 && worker_failed.load() == false)
            {
                if (intersect_aabb(first, second) == false
                    || intersect_aabb(second, first) == false)
                    worker_failed.store(true);
                iteration_index = iteration_index + 1;
                std::this_thread::yield();
            }
            return ;
        });
        writer_thread.join();
        reader_thread.join();
        FT_ASSERT_EQ(false, worker_failed.load());
        round_index = round_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}
