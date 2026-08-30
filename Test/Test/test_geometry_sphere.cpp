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
#include <thread>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Geometry/sphere.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int geometry_expect_sigabrt(void (*operation)())
{
    return (test_expect_sigabrt_signal(operation));
}

struct sphere_move_bidirectional_worker_args
{
    sphere *source;
    sphere *destination;
    std::atomic<bool> *worker_failed;
    int iteration_limit;
};

static void *sphere_move_bidirectional_worker(void *argument)
{
    sphere_move_bidirectional_worker_args *arguments;
    int iteration_index;
    int move_error;

    arguments = static_cast<sphere_move_bidirectional_worker_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    iteration_index = 0;
    while (iteration_index < arguments->iteration_limit
        && arguments->worker_failed->load() == false)
    {
        move_error = arguments->source->move(*arguments->destination);
        if (move_error != FT_ERR_SUCCESS)
            arguments->worker_failed->store(true);
        iteration_index = iteration_index + 1;
    }
    return (ft_nullptr);
}

static void sphere_initialize_twice_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.initialize();
    return ;
}

static void sphere_destroy_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.destroy();
    return ;
}

static void sphere_set_radius_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.set_radius(1.0);
    return ;
}

static void sphere_destroy_twice_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.destroy();
    return ;
}

static void sphere_destructor_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape;
    return ;
}

static void sphere_move_self_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.move(shape);
    return ;
}

static void sphere_initialize_copy_from_uninitialised_source_aborts_operation()
{
    sphere source;
    sphere destination;
    (void)destination.initialize(0.0, 0.0, 0.0, 1.0);

    (void)destination.destroy();
    (void)destination.initialize(source);
    return ;
}

static void sphere_initialize_move_from_uninitialised_source_aborts_operation()
{
    sphere source;
    sphere destination;
    (void)destination.initialize(0.0, 0.0, 0.0, 1.0);

    (void)destination.destroy();
    (void)destination.initialize(static_cast<sphere &&>(source));
    return ;
}

static void sphere_initialize_copy_from_destroyed_source_aborts_operation()
{
    sphere source;
    sphere destination;
    (void)destination.initialize(0.0, 0.0, 0.0, 1.0);

    (void)source.initialize(2.0, 3.0, 4.0, 5.0);
    (void)source.destroy();
    (void)destination.destroy();
    (void)destination.initialize(source);
    return ;
}

static void sphere_initialize_move_from_destroyed_source_aborts_operation()
{
    sphere source;
    sphere destination;
    (void)destination.initialize(0.0, 0.0, 0.0, 1.0);

    (void)source.initialize(2.0, 3.0, 4.0, 5.0);
    (void)source.destroy();
    (void)destination.destroy();
    (void)destination.initialize(static_cast<sphere &&>(source));
    return ;
}

static void sphere_get_center_x_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.get_center_x();
    return ;
}

static void sphere_enable_thread_safety_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.enable_thread_safety();
    return ;
}

static void sphere_initialize_copy_destination_initialised_aborts_operation()
{
    sphere source;
    (void)source.initialize(0.0, 0.0, 0.0, 1.0);
    sphere destination;
    (void)destination.initialize(2.0, 2.0, 2.0, 3.0);

    (void)destination.initialize(source);
    return ;
}

static void sphere_initialize_move_destination_initialised_aborts_operation()
{
    sphere source;
    (void)source.initialize(0.0, 0.0, 0.0, 1.0);
    sphere destination;
    (void)destination.initialize(2.0, 2.0, 2.0, 3.0);

    (void)destination.initialize(static_cast<sphere &&>(source));
    return ;
}

static void sphere_disable_thread_safety_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    shape.disable_thread_safety();
    return ;
}

static void sphere_is_thread_safe_enabled_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.is_thread_safe();
    return ;
}

static void sphere_set_center_z_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.set_center_z(1.0);
    return ;
}

static void sphere_get_radius_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.get_radius();
    return ;
}

static void sphere_set_center_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.set_center(1.0, 2.0, 3.0);
    return ;
}

static void sphere_get_center_z_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.get_center_z();
    return ;
}

static void sphere_enable_thread_safety_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.enable_thread_safety();
    return ;
}

static void sphere_set_center_x_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.set_center_x(1.0);
    return ;
}

static void sphere_get_center_y_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.get_center_y();
    return ;
}

static void sphere_disable_thread_safety_uninitialised_aborts_operation()
{
    sphere shape;

    shape.disable_thread_safety();
    return ;
}

static void sphere_is_thread_safe_enabled_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.is_thread_safe();
    return ;
}

static void sphere_set_center_y_uninitialised_aborts_operation()
{
    sphere shape;

    (void)shape.set_center_y(1.0);
    return ;
}

static void sphere_move_two_uninitialised_aborts_operation()
{
    sphere source;
    sphere destination;

    (void)destination.move(source);
    return ;
}

static void sphere_set_center_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.set_center(1.0, 2.0, 3.0);
    return ;
}

static void sphere_get_center_x_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.get_center_x();
    return ;
}

static void sphere_set_center_y_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.set_center_y(2.0);
    return ;
}

static void sphere_get_radius_destroyed_aborts_second_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.get_radius();
    return ;
}

static void sphere_move_destroyed_source_aborts_operation()
{
    sphere source;
    (void)source.initialize(0.0, 0.0, 0.0, 1.0);
    sphere destination;
    (void)destination.initialize(2.0, 2.0, 2.0, 3.0);

    (void)source.destroy();
    (void)destination.move(source);
    return ;
}

static void sphere_set_center_x_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.set_center_x(1.0);
    return ;
}

static void sphere_set_center_z_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.set_center_z(1.0);
    return ;
}

static void sphere_get_center_y_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.get_center_y();
    return ;
}

static void sphere_get_center_z_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.get_center_z();
    return ;
}

static void sphere_set_radius_destroyed_aborts_operation()
{
    sphere shape;
    (void)shape.initialize(0.0, 0.0, 0.0, 1.0);

    (void)shape.destroy();
    (void)shape.set_radius(2.0);
    return ;
}

static int sphere_move_destroyed_destination_succeeds_operation()
{
    sphere source;
    (void)source.initialize(0.0, 0.0, 0.0, 1.0);
    sphere destination;
    (void)destination.initialize(2.0, 2.0, 2.0, 3.0);
    int move_error;

    (void)destination.destroy();
    move_error = destination.move(source);
    if (move_error != FT_ERR_SUCCESS)
        return (0);
    if (std::fabs(destination.get_center_x()) > 0.000001
        || std::fabs(destination.get_center_y()) > 0.000001
        || std::fabs(destination.get_center_z()) > 0.000001
        || std::fabs(destination.get_radius() - 1.0) > 0.000001)
        return (0);
    if (std::fabs(source.get_center_x()) > 0.000001
        || std::fabs(source.get_center_y()) > 0.000001
        || std::fabs(source.get_center_z()) > 0.000001
        || std::fabs(source.get_radius()) > 0.000001)
        return (0);
    return (1);
}

static void sphere_move_uninitialised_source_aborts_operation()
{
    sphere source;
    sphere destination;
    (void)destination.initialize(2.0, 2.0, 2.0, 3.0);

    (void)destination.move(source);
    return ;
}

static void intersect_sphere_uninitialised_first_aborts_operation()
{
    sphere first;
    sphere second;
    (void)second.initialize(0.0, 0.0, 0.0, 1.0);

    (void)intersect_sphere(first, second);
    return ;
}

static void intersect_sphere_uninitialised_second_aborts_operation()
{
    sphere first;
    (void)first.initialize(0.0, 0.0, 0.0, 1.0);
    sphere second;

    (void)intersect_sphere(first, second);
    return ;
}

static void intersect_sphere_uninitialised_both_aborts_operation()
{
    sphere first;
    sphere second;

    (void)intersect_sphere(first, second);
    return ;
}

static void intersect_sphere_destroyed_first_aborts_operation()
{
    sphere first;
    (void)first.initialize(0.0, 0.0, 0.0, 1.0);
    sphere second;
    (void)second.initialize(0.0, 0.0, 0.0, 1.0);

    (void)first.destroy();
    (void)intersect_sphere(first, second);
    return ;
}

static void intersect_sphere_destroyed_second_aborts_operation()
{
    sphere first;
    (void)first.initialize(0.0, 0.0, 0.0, 1.0);
    sphere second;
    (void)second.initialize(0.0, 0.0, 0.0, 1.0);

    (void)second.destroy();
    (void)intersect_sphere(first, second);
    return ;
}

static void intersect_sphere_destroyed_both_aborts_operation()
{
    sphere first;
    (void)first.initialize(0.0, 0.0, 0.0, 1.0);
    sphere second;
    (void)second.initialize(0.0, 0.0, 0.0, 1.0);

    (void)first.destroy();
    (void)second.destroy();
    (void)intersect_sphere(first, second);
    return ;
}

static void intersect_sphere_destroyed_and_uninitialised_aborts_operation()
{
    sphere first;
    (void)first.initialize(0.0, 0.0, 0.0, 1.0);
    sphere second;

    (void)first.destroy();
    (void)intersect_sphere(first, second);
    return ;
}

FT_TEST(test_sphere_initialize_setters_and_getters)
{
    sphere shape;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.set_center(-3.0, 4.0, -5.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.set_center_x(-2.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.set_center_y(5.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.set_center_z(-6.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.set_radius(7.5));
    FT_ASSERT_DOUBLE_EQ(-2.0, shape.get_center_x());
    FT_ASSERT_DOUBLE_EQ(5.0, shape.get_center_y());
    FT_ASSERT_DOUBLE_EQ(-6.0, shape.get_center_z());
    FT_ASSERT_DOUBLE_EQ(7.5, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_reinitialize_after_destroy)
{
    sphere shape;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(1.0, 2.0, 3.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-4.0, 5.0, -6.0, 7.0));
    FT_ASSERT_DOUBLE_EQ(-4.0, shape.get_center_x());
    FT_ASSERT_DOUBLE_EQ(5.0, shape.get_center_y());
    FT_ASSERT_DOUBLE_EQ(-6.0, shape.get_center_z());
    FT_ASSERT_DOUBLE_EQ(7.0, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_negative_radius_behavior)
{
    sphere shape;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, -2.0));
    FT_ASSERT_DOUBLE_EQ(-2.0, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.set_radius(-9.25));
    FT_ASSERT_DOUBLE_EQ(-9.25, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_large_finite_values_roundtrip)
{
    sphere shape;
    double large_value;

    large_value = std::numeric_limits<double>::max() / 8.0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(
            large_value, -large_value / 2.0, large_value / 4.0, large_value / 16.0));
    FT_ASSERT_DOUBLE_EQ(large_value, shape.get_center_x());
    FT_ASSERT_DOUBLE_EQ(-large_value / 2.0, shape.get_center_y());
    FT_ASSERT_DOUBLE_EQ(large_value / 4.0, shape.get_center_z());
    FT_ASSERT_DOUBLE_EQ(large_value / 16.0, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_smallest_normal_radius_roundtrip)
{
    sphere shape;
    double small_value;

    small_value = std::numeric_limits<double>::min();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, small_value));
    FT_ASSERT_DOUBLE_EQ(small_value, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_infinite_radius_roundtrip)
{
    sphere shape;
    double infinity_value;

    infinity_value = std::numeric_limits<double>::infinity();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, infinity_value));
    FT_ASSERT(std::isinf(shape.get_radius()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_negative_zero_center_x_sign_preserved)
{
    sphere shape;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(1.0, 1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.set_center_x(-0.0));
    FT_ASSERT(std::signbit(shape.get_center_x()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_negative_zero_center_z_sign_preserved)
{
    sphere shape;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(1.0, 1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.set_center_z(-0.0));
    FT_ASSERT(std::signbit(shape.get_center_z()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_set_center_large_values)
{
    sphere shape;
    double large_value;

    large_value = std::numeric_limits<double>::max() / 8.0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.set_center(
            large_value, -large_value, large_value / 2.0));
    FT_ASSERT_DOUBLE_EQ(large_value, shape.get_center_x());
    FT_ASSERT_DOUBLE_EQ(-large_value, shape.get_center_y());
    FT_ASSERT_DOUBLE_EQ(large_value / 2.0, shape.get_center_z());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_reinitialize_with_infinite_radius)
{
    sphere shape;
    double infinity_value;

    infinity_value = std::numeric_limits<double>::infinity();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(1.0, 2.0, 3.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, infinity_value));
    FT_ASSERT(std::isinf(shape.get_radius()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_lifecycle_multiple_reinitialize_cycles)
{
    sphere shape;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(1.0, 2.0, 3.0, 4.0));
    iteration_index = 0;
    while (iteration_index < 6)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(
                -1.0 - iteration_index,
                2.0 + iteration_index,
                -3.0 - iteration_index,
                4.0 + iteration_index));
        FT_ASSERT_DOUBLE_EQ(-1.0 - iteration_index, shape.get_center_x());
        FT_ASSERT_DOUBLE_EQ(2.0 + iteration_index, shape.get_center_y());
        FT_ASSERT_DOUBLE_EQ(-3.0 - iteration_index, shape.get_center_z());
        FT_ASSERT_DOUBLE_EQ(4.0 + iteration_index, shape.get_radius());
        iteration_index = iteration_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_lifecycle_thread_safety_cycles)
{
    sphere shape;
    int iteration_index;

    iteration_index = 0;
    while (iteration_index < 6)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(
                -1.0 - iteration_index,
                2.0 + iteration_index,
                -3.0 - iteration_index,
                4.0 + iteration_index));
        FT_ASSERT_EQ(false, shape.is_thread_safe());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
        FT_ASSERT_EQ(true, shape.is_thread_safe());
        shape.disable_thread_safety();
        FT_ASSERT_EQ(false, shape.is_thread_safe());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
        iteration_index = iteration_index + 1;
    }
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_failure_then_recovery)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    cma_set_alloc_limit(1);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    FT_ASSERT_EQ(true, shape.is_thread_safe());
    shape.disable_thread_safety();
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_repeated_alloc_failures)
{
    sphere shape;
    int error_code;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    cma_set_alloc_limit(1);
    iteration_index = 0;
    while (iteration_index < 3)
    {
        error_code = shape.enable_thread_safety();
        FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
        FT_ASSERT_EQ(false, shape.is_thread_safe());
        iteration_index = iteration_index + 1;
    }
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_failure_then_destroy)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-2.0, -2.0, -2.0, 2.0));
    cma_set_alloc_limit(1);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_failure_after_disable)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-1.0, -1.0, -1.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    shape.disable_thread_safety();
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    cma_set_alloc_limit(1);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_get_mutex_for_testing_alloc_failure_returns_null)
{
    sphere shape;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    cma_set_alloc_limit(1);
    mutex_pointer = shape._mutex;
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, mutex_pointer);
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_get_mutex_for_testing_recovers_after_alloc_failure)
{
    sphere shape;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    cma_set_alloc_limit(1);
    mutex_pointer = shape._mutex;
    FT_ASSERT_EQ(ft_nullptr, mutex_pointer);
    cma_set_alloc_limit(0);
    mutex_pointer = shape._mutex;
    FT_ASSERT_EQ(ft_nullptr, mutex_pointer);
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_failure_reinit_cycle)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-3.0, -3.0, -3.0, 3.0));
    cma_set_alloc_limit(1);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-4.0, -4.0, -4.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_failure_preserves_values)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-5.0, -6.0, -7.0, 8.0));
    cma_set_alloc_limit(1);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_DOUBLE_EQ(-5.0, shape.get_center_x());
    FT_ASSERT_DOUBLE_EQ(-6.0, shape.get_center_y());
    FT_ASSERT_DOUBLE_EQ(-7.0, shape.get_center_z());
    FT_ASSERT_DOUBLE_EQ(8.0, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_two_state_consistent)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-1.0, -1.0, -1.0, 1.0));
    cma_set_alloc_limit(2);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_three_state_consistent)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-1.0, -1.0, -1.0, 1.0));
    cma_set_alloc_limit(3);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_two_then_recover)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-2.0, -2.0, -2.0, 2.0));
    cma_set_alloc_limit(2);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        FT_ASSERT_EQ(true, shape.is_thread_safe());
        shape.disable_thread_safety();
    }
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_three_then_recover)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-2.0, -2.0, -2.0, 2.0));
    cma_set_alloc_limit(3);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        FT_ASSERT_EQ(true, shape.is_thread_safe());
        shape.disable_thread_safety();
    }
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_get_mutex_for_testing_alloc_limit_two_consistent)
{
    sphere shape;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    cma_set_alloc_limit(2);
    mutex_pointer = shape._mutex;
    cma_set_alloc_limit(0);
    if (mutex_pointer == ft_nullptr)
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_get_mutex_for_testing_alloc_limit_three_consistent)
{
    sphere shape;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    cma_set_alloc_limit(3);
    mutex_pointer = shape._mutex;
    cma_set_alloc_limit(0);
    if (mutex_pointer == ft_nullptr)
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_alloc_limit_two_preserves_values)
{
    sphere shape;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-9.0, -8.0, -7.0, 6.0));
    cma_set_alloc_limit(2);
    (void)shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_DOUBLE_EQ(-9.0, shape.get_center_x());
    FT_ASSERT_DOUBLE_EQ(-8.0, shape.get_center_y());
    FT_ASSERT_DOUBLE_EQ(-7.0, shape.get_center_z());
    FT_ASSERT_DOUBLE_EQ(6.0, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_four_state_consistent)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-1.0, -1.0, -1.0, 1.0));
    cma_set_alloc_limit(4);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_five_state_consistent)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-1.0, -1.0, -1.0, 1.0));
    cma_set_alloc_limit(5);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_six_state_consistent)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-1.0, -1.0, -1.0, 1.0));
    cma_set_alloc_limit(6);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_four_then_recover)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-2.0, -2.0, -2.0, 2.0));
    cma_set_alloc_limit(4);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        shape.disable_thread_safety();
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_five_then_recover)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-2.0, -2.0, -2.0, 2.0));
    cma_set_alloc_limit(5);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        shape.disable_thread_safety();
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_get_mutex_for_testing_alloc_limit_four_consistent)
{
    sphere shape;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    cma_set_alloc_limit(4);
    mutex_pointer = shape._mutex;
    cma_set_alloc_limit(0);
    if (mutex_pointer == ft_nullptr)
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_alloc_limit_four_preserves_values)
{
    sphere shape;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-9.0, -8.0, -7.0, 6.0));
    cma_set_alloc_limit(4);
    (void)shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_DOUBLE_EQ(-9.0, shape.get_center_x());
    FT_ASSERT_DOUBLE_EQ(-8.0, shape.get_center_y());
    FT_ASSERT_DOUBLE_EQ(-7.0, shape.get_center_z());
    FT_ASSERT_DOUBLE_EQ(6.0, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_seven_state_consistent)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-1.0, -1.0, -1.0, 1.0));
    cma_set_alloc_limit(7);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_eight_state_consistent)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-1.0, -1.0, -1.0, 1.0));
    cma_set_alloc_limit(8);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_nine_state_consistent)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-1.0, -1.0, -1.0, 1.0));
    cma_set_alloc_limit(9);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_seven_then_recover)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-2.0, -2.0, -2.0, 2.0));
    cma_set_alloc_limit(7);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        shape.disable_thread_safety();
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_alloc_limit_eight_then_recover)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-2.0, -2.0, -2.0, 2.0));
    cma_set_alloc_limit(8);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    if (error_code == FT_ERR_SUCCESS)
    {
        shape.disable_thread_safety();
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_get_mutex_for_testing_alloc_limit_seven_consistent)
{
    sphere shape;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    cma_set_alloc_limit(7);
    mutex_pointer = shape._mutex;
    cma_set_alloc_limit(0);
    if (mutex_pointer == ft_nullptr)
        FT_ASSERT_EQ(false, shape.is_thread_safe());
    else
        FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_alloc_limit_seven_preserves_values)
{
    sphere shape;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(-9.0, -8.0, -7.0, 6.0));
    cma_set_alloc_limit(7);
    (void)shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_DOUBLE_EQ(-9.0, shape.get_center_x());
    FT_ASSERT_DOUBLE_EQ(-8.0, shape.get_center_y());
    FT_ASSERT_DOUBLE_EQ(-7.0, shape.get_center_z());
    FT_ASSERT_DOUBLE_EQ(6.0, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_intersect_sphere_overlap)
{
    sphere first;
    sphere second;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(1.0, 2.0, 3.0, 5.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(4.0, 3.0, 5.0, 4.0));
    FT_ASSERT(intersect_sphere(first, second));
    FT_ASSERT(intersect_sphere(second, first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_sphere_separated)
{
    sphere first;
    sphere second;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(-4.0, -4.0, -4.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(4.0, 4.0, 4.0, 1.0));
    FT_ASSERT_EQ(false, intersect_sphere(first, second));
    FT_ASSERT_EQ(false, intersect_sphere(second, first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_sphere_touching)
{
    sphere first;
    sphere second;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 0.0, 2.5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(5.0, 0.0, 0.0, 2.5));
    FT_ASSERT(intersect_sphere(first, second));
    FT_ASSERT(intersect_sphere(second, first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_intersect_sphere_parallel_access)
{
    sphere first;
    sphere second;
    std::atomic<bool> worker_ready;
    std::atomic<bool> worker_failed(false);
    std::atomic<bool> worker_completed;
    std::thread worker_thread;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 0.0, 6.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(2.0, 2.0, 2.0, 5.0));
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
            if (intersect_sphere(first, second) == false)
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
        if (intersect_sphere(second, first) == false)
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

FT_TEST(test_intersect_sphere_high_load_overlap_two_threads)
{
    sphere first;
    sphere second;
    std::atomic<bool> worker_failed(false);
    std::thread worker_thread;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 0.0, 6.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(2.0, 2.0, 2.0, 5.0));
    worker_failed.store(false);
    worker_thread = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 8192 && worker_failed.load() == false)
        {
            if (intersect_sphere(first, second) == false)
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
        if (intersect_sphere(second, first) == false)
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

FT_TEST(test_intersect_sphere_high_load_overlap_four_threads)
{
    sphere first;
    sphere second;
    std::atomic<bool> worker_failed(false);
    std::thread worker_thread_one;
    std::thread worker_thread_two;
    std::thread worker_thread_three;
    std::thread worker_thread_four;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 0.0, 9.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(1.0, 1.0, 1.0, 8.0));
    worker_failed.store(false);
    worker_thread_one = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 4096 && worker_failed.load() == false)
        {
            if (intersect_sphere(first, second) == false)
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
            if (intersect_sphere(second, first) == false)
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
            if (intersect_sphere(first, second) == false)
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
            if (intersect_sphere(second, first) == false)
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

FT_TEST(test_intersect_sphere_high_load_touching_two_threads)
{
    sphere first;
    sphere second;
    std::atomic<bool> worker_failed(false);
    std::thread worker_thread;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 0.0, 2.5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(5.0, 0.0, 0.0, 2.5));
    worker_failed.store(false);
    worker_thread = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 6144 && worker_failed.load() == false)
        {
            if (intersect_sphere(first, second) == false)
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
        if (intersect_sphere(second, first) == false)
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

FT_TEST(test_intersect_sphere_high_load_separated_two_threads)
{
    sphere first;
    sphere second;
    std::atomic<bool> worker_failed(false);
    std::thread worker_thread;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(-4.0, -4.0, -4.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(4.0, 4.0, 4.0, 1.0));
    worker_failed.store(false);
    worker_thread = std::thread([&first, &second, &worker_failed]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < 6144 && worker_failed.load() == false)
        {
            if (intersect_sphere(first, second) != false)
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
        if (intersect_sphere(second, first) != false)
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

FT_TEST(test_sphere_move_bidirectional_high_load_with_thread_safety)
{
    sphere first;
    sphere second;
    std::atomic<bool> worker_failed(false);
    std::thread worker_thread;
    int iteration_index;
    int iteration_limit;
    int move_error;
    std::chrono::steady_clock::time_point start_time;

    iteration_limit = 1024;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 0.0, 3.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(1.0, 1.0, 1.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.enable_thread_safety());
    worker_failed.store(false);
    start_time = std::chrono::steady_clock::now();
    worker_thread = std::thread([&first, &second, &worker_failed, &iteration_limit, &start_time]() {
        int local_iteration_index;
        int local_move_error;

        local_iteration_index = 0;
        while (local_iteration_index < iteration_limit && worker_failed.load() == false)
        {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time).count() > 2000)
            {
                worker_failed.store(true);
                break;
            }
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

FT_TEST(test_sphere_setters_getters_contention_high_load_two_threads)
{
    struct sphere_contention_test_context
    {
        std::atomic<int> result;
    };
    sphere_contention_test_context context;
    pthread_t test_thread;
    int32_t join_result;
    const long join_timeout_ms = 5000;

    context.result.store(0);
    FT_ASSERT_EQ(0, pt_thread_create(&test_thread, ft_nullptr,
        [](void *argument) -> void *
        {
            sphere_contention_test_context *context_pointer;
            sphere shape;
            std::atomic<bool> worker_failed(false);
            std::thread writer_thread;
            int iteration_index;
            int iteration_limit;
            double center_x;
            double center_y;
            double center_z;
            double radius;
            int set_error;

            context_pointer = static_cast<sphere_contention_test_context *>(argument);
            if (context_pointer == ft_nullptr)
                return (ft_nullptr);
            iteration_limit = 1024;
            if (shape.initialize(0.0, 0.0, 0.0, 1.0) != FT_ERR_SUCCESS)
                return (ft_nullptr);
            if (shape.enable_thread_safety() != FT_ERR_SUCCESS)
                return (ft_nullptr);
            worker_failed.store(false);
            writer_thread = std::thread([&shape, &worker_failed, &iteration_limit]() {
                int local_iteration_index;
                int local_set_error;

                local_iteration_index = 0;
                while (local_iteration_index < iteration_limit && worker_failed.load() == false)
                {
                    if ((local_iteration_index % 2) == 0)
                        local_set_error = shape.set_center(2.0, -2.0, 2.0);
                    else
                        local_set_error = shape.set_center(-3.0, 3.0, -3.0);
                    if (local_set_error != FT_ERR_SUCCESS)
                    {
                        worker_failed.store(true);
                        break;
                    }
                    local_set_error = shape.set_radius(5.0 + (local_iteration_index % 3));
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
            while (iteration_index < iteration_limit && worker_failed.load() == false)
            {
                center_x = shape.get_center_x();
                center_y = shape.get_center_y();
                center_z = shape.get_center_z();
                radius = shape.get_radius();
                if ((std::isfinite(center_x) == false) || (std::isfinite(center_y) == false)
                    || (std::isfinite(center_z) == false) || (std::isfinite(radius) == false))
                {
                    worker_failed.store(true);
                    break;
                }
                iteration_index = iteration_index + 1;
            }
            writer_thread.join();
            if (worker_failed.load() != false)
                return (ft_nullptr);
            set_error = shape.set_center(0.0, 0.0, 0.0);
            if (set_error != FT_ERR_SUCCESS)
                return (ft_nullptr);
            if (shape.destroy() != FT_ERR_SUCCESS)
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

FT_TEST(test_intersect_sphere_high_load_with_mutating_overlap)
{
    struct sphere_intersect_worker_args
    {
        sphere *first;
        sphere *second;
        std::atomic<bool> *worker_failed;
    };
    sphere first;
    sphere second;
    std::atomic<bool> worker_failed(false);
    sphere_intersect_worker_args writer_arguments;
    sphere_intersect_worker_args reader_one_arguments;
    sphere_intersect_worker_args reader_two_arguments;
    pthread_t writer_thread;
    pthread_t reader_thread_one;
    pthread_t reader_thread_two;
    int32_t join_result;
    const long join_timeout_ms = 5000;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 0.0, 15.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(1.0, 1.0, 1.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.enable_thread_safety());
    writer_arguments.first = &first;
    writer_arguments.second = &second;
    writer_arguments.worker_failed = &worker_failed;
    reader_one_arguments.first = &first;
    reader_one_arguments.second = &second;
    reader_one_arguments.worker_failed = &worker_failed;
    reader_two_arguments.first = &second;
    reader_two_arguments.second = &first;
    reader_two_arguments.worker_failed = &worker_failed;
    FT_ASSERT_EQ(0, pt_thread_create(&writer_thread, ft_nullptr,
        [](void *argument) -> void *
        {
            sphere_intersect_worker_args *worker_arguments;
            int local_iteration_index;
            int local_set_error;

            worker_arguments = static_cast<sphere_intersect_worker_args *>(argument);
            local_iteration_index = 0;
            while (local_iteration_index < 4096
                && worker_arguments->worker_failed->load() == false)
            {
                if ((local_iteration_index % 2) == 0)
                    local_set_error = worker_arguments->second->set_center(2.0, 2.0, 2.0);
                else
                    local_set_error = worker_arguments->second->set_center(-1.0, 1.0, -1.0);
                if (local_set_error != FT_ERR_SUCCESS)
                {
                    worker_arguments->worker_failed->store(true);
                    break ;
                }
                local_set_error = worker_arguments->second->set_radius(
                    4.0 + (local_iteration_index % 2));
                if (local_set_error != FT_ERR_SUCCESS)
                {
                    worker_arguments->worker_failed->store(true);
                    break ;
                }
                local_iteration_index = local_iteration_index + 1;
            }
            return (ft_nullptr);
        }, &writer_arguments));
    FT_ASSERT_EQ(0, pt_thread_create(&reader_thread_one, ft_nullptr,
        [](void *argument) -> void *
        {
            sphere_intersect_worker_args *worker_arguments;
            int local_iteration_index;

            worker_arguments = static_cast<sphere_intersect_worker_args *>(argument);
            local_iteration_index = 0;
            while (local_iteration_index < 4096
                && worker_arguments->worker_failed->load() == false)
            {
                if (intersect_sphere(*worker_arguments->first,
                        *worker_arguments->second) == false)
                {
                    worker_arguments->worker_failed->store(true);
                    break ;
                }
                local_iteration_index = local_iteration_index + 1;
            }
            return (ft_nullptr);
        }, &reader_one_arguments));
    FT_ASSERT_EQ(0, pt_thread_create(&reader_thread_two, ft_nullptr,
        [](void *argument) -> void *
        {
            sphere_intersect_worker_args *worker_arguments;
            int local_iteration_index;

            worker_arguments = static_cast<sphere_intersect_worker_args *>(argument);
            local_iteration_index = 0;
            while (local_iteration_index < 4096
                && worker_arguments->worker_failed->load() == false)
            {
                if (intersect_sphere(*worker_arguments->first,
                        *worker_arguments->second) == false)
                {
                    worker_arguments->worker_failed->store(true);
                    break ;
                }
                local_iteration_index = local_iteration_index + 1;
            }
            return (ft_nullptr);
        }, &reader_two_arguments));
    join_result = pt_thread_timed_join(writer_thread, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
        (void)pt_thread_detach(writer_thread);
    FT_ASSERT_EQ(0, join_result);
    join_result = pt_thread_timed_join(reader_thread_one, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
        (void)pt_thread_detach(reader_thread_one);
    FT_ASSERT_EQ(0, join_result);
    join_result = pt_thread_timed_join(reader_thread_two, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
        (void)pt_thread_detach(reader_thread_two);
    FT_ASSERT_EQ(0, join_result);
    FT_ASSERT_EQ(false, worker_failed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_sphere_setters_getters_contention_high_load_four_threads)
{
    sphere shape;
    std::atomic<bool> worker_failed(false);
    std::thread writer_thread;
    std::thread reader_thread_one;
    std::thread reader_thread_two;
    std::thread reader_thread_three;
    int iteration_limit;
    std::chrono::steady_clock::time_point start_time;

    iteration_limit = 768;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    worker_failed.store(false);
    start_time = std::chrono::steady_clock::now();
    writer_thread = std::thread([&shape, &worker_failed, &iteration_limit, &start_time]() {
        int local_iteration_index;
        int local_error_code;

        local_iteration_index = 0;
        while (local_iteration_index < iteration_limit && worker_failed.load() == false)
        {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time).count() > 2000)
            {
                worker_failed.store(true);
                break;
            }
            local_error_code = shape.set_center(
                    static_cast<double>((local_iteration_index % 7) - 3),
                    static_cast<double>((local_iteration_index % 5) - 2),
                    static_cast<double>((local_iteration_index % 9) - 4));
            if (local_error_code != FT_ERR_SUCCESS)
            {
                worker_failed.store(true);
                break;
            }
            local_error_code = shape.set_radius(4.0 + (local_iteration_index % 4));
            if (local_error_code != FT_ERR_SUCCESS)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    reader_thread_one = std::thread([&shape, &worker_failed, &iteration_limit, &start_time]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < iteration_limit && worker_failed.load() == false)
        {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time).count() > 2000)
            {
                worker_failed.store(true);
                break;
            }
            if (std::isfinite(shape.get_center_x()) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    reader_thread_two = std::thread([&shape, &worker_failed, &iteration_limit, &start_time]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < iteration_limit && worker_failed.load() == false)
        {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time).count() > 2000)
            {
                worker_failed.store(true);
                break;
            }
            if (std::isfinite(shape.get_center_y()) == false
                || std::isfinite(shape.get_center_z()) == false)
            {
                worker_failed.store(true);
                break;
            }
            local_iteration_index = local_iteration_index + 1;
        }
        return ;
    });
    reader_thread_three = std::thread([&shape, &worker_failed, &iteration_limit, &start_time]() {
        int local_iteration_index;

        local_iteration_index = 0;
        while (local_iteration_index < iteration_limit && worker_failed.load() == false)
        {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time).count() > 2000)
            {
                worker_failed.store(true);
                break;
            }
            if (std::isfinite(shape.get_radius()) == false)
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
    reader_thread_three.join();
    FT_ASSERT_EQ(false, worker_failed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_copy_and_move_initializers)
{
    sphere source;
    sphere copied_destination;
    sphere moved_destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(7.0, -3.0, 2.0, 9.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_destination.initialize(source));
    FT_ASSERT_DOUBLE_EQ(7.0, copied_destination.get_center_x());
    FT_ASSERT_DOUBLE_EQ(-3.0, copied_destination.get_center_y());
    FT_ASSERT_DOUBLE_EQ(2.0, copied_destination.get_center_z());
    FT_ASSERT_DOUBLE_EQ(9.0, copied_destination.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_destination.initialize(static_cast<sphere &&>(source)));
    FT_ASSERT_DOUBLE_EQ(7.0, moved_destination.get_center_x());
    FT_ASSERT_DOUBLE_EQ(-3.0, moved_destination.get_center_y());
    FT_ASSERT_DOUBLE_EQ(2.0, moved_destination.get_center_z());
    FT_ASSERT_DOUBLE_EQ(9.0, moved_destination.get_radius());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_center_x());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_center_y());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_center_z());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_destination.destroy());
    return (1);
}

FT_TEST(test_sphere_move_method_transfers_values)
{
    sphere source;
    sphere destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(4.0, 5.0, 6.0, 7.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_DOUBLE_EQ(4.0, destination.get_center_x());
    FT_ASSERT_DOUBLE_EQ(5.0, destination.get_center_y());
    FT_ASSERT_DOUBLE_EQ(6.0, destination.get_center_z());
    FT_ASSERT_DOUBLE_EQ(7.0, destination.get_radius());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_center_x());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_center_y());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_center_z());
    FT_ASSERT_DOUBLE_EQ(0.0, source.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_sphere_thread_safety_enable_disable)
{
    sphere shape;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    FT_ASSERT_EQ(true, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    FT_ASSERT_EQ(true, shape.is_thread_safe());
    shape.disable_thread_safety();
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_initialize_from_destroyed_source_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            sphere_initialize_copy_from_destroyed_source_aborts_operation));
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            sphere_initialize_move_from_destroyed_source_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_move_self_initialised_is_noop)
{
    sphere shape;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(4.0, 5.0, 6.0, 7.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.move(shape));
    FT_ASSERT_DOUBLE_EQ(4.0, shape.get_center_x());
    FT_ASSERT_DOUBLE_EQ(5.0, shape.get_center_y());
    FT_ASSERT_DOUBLE_EQ(6.0, shape.get_center_z());
    FT_ASSERT_DOUBLE_EQ(7.0, shape.get_radius());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_mutex_testing_accessor_lifecycle)
{
    sphere shape;
    pt_recursive_mutex *mutex_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    mutex_pointer = shape._mutex;
    FT_ASSERT_EQ(ft_nullptr, mutex_pointer);
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    shape.disable_thread_safety();
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    FT_ASSERT_EQ(ft_nullptr, shape._mutex);
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_allocation_failure)
{
    sphere shape;
    int error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    cma_set_alloc_limit(1);
    error_code = shape.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(false, shape.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_sphere_initialize_twice_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(sphere_initialize_twice_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_destroy_uninitialised_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(sphere_destroy_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_set_radius_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(sphere_set_radius_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_destroy_twice_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(sphere_destroy_twice_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_uninitialised_destructor_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(sphere_destructor_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_self_move_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(sphere_move_self_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_initialize_copy_from_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_initialize_copy_from_uninitialised_source_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_initialize_move_from_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_initialize_move_from_uninitialised_source_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_get_center_x_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_get_center_x_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_enable_thread_safety_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_initialize_copy_destination_initialised_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            sphere_initialize_copy_destination_initialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_initialize_move_destination_initialised_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            sphere_initialize_move_destination_initialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_disable_thread_safety_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_disable_thread_safety_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_is_thread_safe_enabled_destroyed_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            sphere_is_thread_safe_enabled_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_set_center_z_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_set_center_z_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_get_radius_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_get_radius_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_set_center_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_set_center_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_get_center_z_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_get_center_z_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_enable_thread_safety_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_enable_thread_safety_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_set_center_x_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_set_center_x_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_get_center_y_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_get_center_y_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_disable_thread_safety_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_disable_thread_safety_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_is_thread_safe_enabled_uninitialised_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            sphere_is_thread_safe_enabled_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_set_center_y_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_set_center_y_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_move_two_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_move_two_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_set_center_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_set_center_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_get_center_x_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_get_center_x_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_set_center_y_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_set_center_y_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_get_radius_destroyed_aborts_second)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_get_radius_destroyed_aborts_second_operation));
    return (1);
}

FT_TEST(test_sphere_move_destroyed_source_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            sphere_move_destroyed_source_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_set_center_x_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_set_center_x_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_set_center_z_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_set_center_z_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_get_center_y_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_get_center_y_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_get_center_z_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_get_center_z_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_set_radius_destroyed_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_set_radius_destroyed_aborts_operation));
    return (1);
}

FT_TEST(test_sphere_move_destroyed_destination_succeeds)
{
    FT_ASSERT_EQ(1, sphere_move_destroyed_destination_succeeds_operation());
    return (1);
}

FT_TEST(test_sphere_move_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, geometry_expect_sigabrt(
            sphere_move_uninitialised_source_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_sphere_uninitialised_first_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_sphere_uninitialised_first_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_sphere_uninitialised_second_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_sphere_uninitialised_second_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_sphere_uninitialised_both_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_sphere_uninitialised_both_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_sphere_destroyed_first_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_sphere_destroyed_first_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_sphere_destroyed_second_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_sphere_destroyed_second_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_sphere_destroyed_both_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_sphere_destroyed_both_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_sphere_destroyed_and_uninitialised_succeeds)
{
    FT_ASSERT_EQ(0, geometry_expect_sigabrt(
            intersect_sphere_destroyed_and_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_intersect_sphere_high_load_overlap_two_threads_soak_rounds)
{
    sphere first;
    sphere second;
    std::atomic<bool> worker_failed(false);
    std::thread worker_thread;
    int round_index;
    int iteration_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 0.0, 6.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(2.0, 2.0, 2.0, 5.0));
    round_index = 0;
    while (round_index < 4)
    {
        worker_failed.store(false);
        worker_thread = std::thread([&first, &second, &worker_failed]() {
            int local_iteration_index;

            local_iteration_index = 0;
            while (local_iteration_index < 4096 && worker_failed.load() == false)
            {
                if (intersect_sphere(first, second) == false)
                    worker_failed.store(true);
                local_iteration_index = local_iteration_index + 1;
            }
            return ;
        });
        iteration_index = 0;
        while (iteration_index < 4096 && worker_failed.load() == false)
        {
            if (intersect_sphere(second, first) == false)
                worker_failed.store(true);
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

FT_TEST(test_intersect_sphere_high_load_overlap_four_threads_soak_rounds)
{
    sphere first;
    sphere second;
    std::atomic<bool> worker_failed(false);
    std::thread thread_one;
    std::thread thread_two;
    std::thread thread_three;
    std::thread thread_four;
    int round_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 0.0, 9.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(1.0, 1.0, 1.0, 8.0));
    round_index = 0;
    while (round_index < 3)
    {
        worker_failed.store(false);
        thread_one = std::thread([&first, &second, &worker_failed]() {
            int index;
            index = 0;
            while (index < 3072 && worker_failed.load() == false)
            {
                if (intersect_sphere(first, second) == false)
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
                if (intersect_sphere(second, first) == false)
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
                if (intersect_sphere(first, second) == false)
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
                if (intersect_sphere(second, first) == false)
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

FT_TEST(test_intersect_sphere_high_load_touching_two_threads_soak_rounds)
{
    sphere first;
    sphere second;
    std::atomic<bool> worker_failed(false);
    std::thread worker_thread;
    int round_index;
    int index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(0.0, 0.0, 0.0, 2.5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(5.0, 0.0, 0.0, 2.5));
    round_index = 0;
    while (round_index < 4)
    {
        worker_failed.store(false);
        worker_thread = std::thread([&first, &second, &worker_failed]() {
            int iteration_index;
            iteration_index = 0;
            while (iteration_index < 3072 && worker_failed.load() == false)
            {
                if (intersect_sphere(first, second) == false)
                    worker_failed.store(true);
                iteration_index = iteration_index + 1;
            }
            return ;
        });
        index = 0;
        while (index < 3072 && worker_failed.load() == false)
        {
            if (intersect_sphere(second, first) == false)
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

FT_TEST(test_intersect_sphere_high_load_separated_two_threads_soak_rounds)
{
    sphere first;
    sphere second;
    std::atomic<bool> worker_failed(false);
    std::thread worker_thread;
    int round_index;
    int index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(-4.0, -4.0, -4.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(4.0, 4.0, 4.0, 1.0));
    round_index = 0;
    while (round_index < 4)
    {
        worker_failed.store(false);
        worker_thread = std::thread([&first, &second, &worker_failed]() {
            int iteration_index;
            iteration_index = 0;
            while (iteration_index < 3072 && worker_failed.load() == false)
            {
                if (intersect_sphere(first, second) != false)
                    worker_failed.store(true);
                iteration_index = iteration_index + 1;
            }
            return ;
        });
        index = 0;
        while (index < 3072 && worker_failed.load() == false)
        {
            if (intersect_sphere(second, first) != false)
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

FT_TEST(test_sphere_move_bidirectional_high_load_soak_rounds)
{
    sphere *first;
    sphere *second;
    std::atomic<bool> *worker_failed;
    sphere_move_bidirectional_worker_args *thread_one_arguments;
    sphere_move_bidirectional_worker_args *thread_two_arguments;
    pthread_t thread_one;
    pthread_t thread_two;
    int round_index;
    int thread_create_result;
    int thread_join_result;
    long join_timeout_ms;

    first = new (std::nothrow) sphere();
    second = new (std::nothrow) sphere();
    worker_failed = new (std::nothrow) std::atomic<bool>();
    thread_one_arguments = new (std::nothrow) sphere_move_bidirectional_worker_args();
    thread_two_arguments = new (std::nothrow) sphere_move_bidirectional_worker_args();
    FT_ASSERT(first != ft_nullptr);
    FT_ASSERT(second != ft_nullptr);
    FT_ASSERT(worker_failed != ft_nullptr);
    FT_ASSERT(thread_one_arguments != ft_nullptr);
    FT_ASSERT(thread_two_arguments != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->initialize(0.0, 0.0, 0.0, 3.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->initialize(1.0, 1.0, 1.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->enable_thread_safety());
    /* Windows CI can be heavily oversubscribed while this contention test is
       running. Keep the timeout bounded, but do not classify scheduler delay
       as a lock failure. */
#ifdef _WIN32
    join_timeout_ms = 60000;
#else
    join_timeout_ms = 10000;
#endif
    round_index = 0;
    while (round_index < 3)
    {
        worker_failed->store(false);
        thread_one_arguments->source = first;
        thread_one_arguments->destination = second;
        thread_one_arguments->worker_failed = worker_failed;
        thread_one_arguments->iteration_limit = 2048;
        thread_two_arguments->source = second;
        thread_two_arguments->destination = first;
        thread_two_arguments->worker_failed = worker_failed;
        thread_two_arguments->iteration_limit = 2048;
        thread_create_result = pt_thread_create(&thread_one, ft_nullptr,
                sphere_move_bidirectional_worker, thread_one_arguments);
        FT_ASSERT_EQ(0, thread_create_result);
        thread_create_result = pt_thread_create(&thread_two, ft_nullptr,
                sphere_move_bidirectional_worker, thread_two_arguments);
        FT_ASSERT_EQ(0, thread_create_result);
        thread_join_result = pt_thread_timed_join(thread_one, ft_nullptr, join_timeout_ms);
        if (thread_join_result != 0)
            (void)pt_thread_detach(thread_one);
        FT_ASSERT_EQ(0, thread_join_result);
        thread_join_result = pt_thread_timed_join(thread_two, ft_nullptr, join_timeout_ms);
        if (thread_join_result != 0)
            (void)pt_thread_detach(thread_two);
        FT_ASSERT_EQ(0, thread_join_result);
        FT_ASSERT_EQ(false, worker_failed->load());
        round_index = round_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->destroy());
    delete thread_two_arguments;
    delete thread_one_arguments;
    delete worker_failed;
    delete second;
    delete first;
    return (1);
}

struct sphere_soak_worker_args
{
    sphere *shape;
    std::atomic<bool> *worker_failed;
};

struct sphere_intersect_soak_worker_args
{
    sphere *first;
    sphere *second;
    std::atomic<bool> *worker_failed;
};

static void *sphere_soak_writer_worker(void *argument)
{
    sphere_soak_worker_args *worker_args;
    int iteration_index;
    int local_error_code;
    std::chrono::steady_clock::time_point start_time;

    worker_args = static_cast<sphere_soak_worker_args *>(argument);
    iteration_index = 0;
    start_time = std::chrono::steady_clock::now();
    while (iteration_index < 3072 && worker_args->worker_failed->load() == false)
    {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count() > 1500)
            break ;
        local_error_code = worker_args->shape->set_center(
                static_cast<double>((iteration_index % 7) - 3),
                static_cast<double>((iteration_index % 5) - 2),
                static_cast<double>((iteration_index % 9) - 4));
        if (local_error_code != FT_ERR_SUCCESS)
            worker_args->worker_failed->store(true);
        local_error_code = worker_args->shape->set_radius(4.0 + (iteration_index % 4));
        if (local_error_code != FT_ERR_SUCCESS)
            worker_args->worker_failed->store(true);
        iteration_index = iteration_index + 1;
    }
    return (ft_nullptr);
}

static void *sphere_soak_reader_worker(void *argument)
{
    sphere_soak_worker_args *worker_args;
    int index;
    std::chrono::steady_clock::time_point start_time;

    worker_args = static_cast<sphere_soak_worker_args *>(argument);
    index = 0;
    start_time = std::chrono::steady_clock::now();
    while (index < 3072 && worker_args->worker_failed->load() == false)
    {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count() > 1500)
            break ;
        if (std::isfinite(worker_args->shape->get_center_x()) == false
            || std::isfinite(worker_args->shape->get_center_y()) == false
            || std::isfinite(worker_args->shape->get_center_z()) == false
            || std::isfinite(worker_args->shape->get_radius()) == false)
            worker_args->worker_failed->store(true);
        index = index + 1;
    }
    return (ft_nullptr);
}

static void *sphere_intersect_soak_writer_worker(void *argument)
{
    sphere_intersect_soak_worker_args *worker_args;
    int iteration_index;
    int local_error_code;

    worker_args = static_cast<sphere_intersect_soak_worker_args *>(argument);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    iteration_index = 0;
    while (iteration_index < 3072 && worker_args->worker_failed->load() == false)
    {
        if ((iteration_index % 2) == 0)
            local_error_code = worker_args->second->set_center(2.0, 2.0, 2.0);
        else
            local_error_code = worker_args->second->set_center(-1.0, 1.0, -1.0);
        if (local_error_code != FT_ERR_SUCCESS)
            worker_args->worker_failed->store(true);
        local_error_code = worker_args->second->set_radius(4.0 + (iteration_index % 2));
        if (local_error_code != FT_ERR_SUCCESS)
            worker_args->worker_failed->store(true);
        iteration_index = iteration_index + 1;
    }
    return (ft_nullptr);
}

static void *sphere_intersect_soak_reader_worker(void *argument)
{
    sphere_intersect_soak_worker_args *worker_args;
    int iteration_index;

    worker_args = static_cast<sphere_intersect_soak_worker_args *>(argument);
    iteration_index = 0;
    while (iteration_index < 3072 && worker_args->worker_failed->load() == false)
    {
        if (intersect_sphere(*worker_args->first, *worker_args->second) == false
            || intersect_sphere(*worker_args->second, *worker_args->first) == false)
            worker_args->worker_failed->store(true);
        iteration_index = iteration_index + 1;
    }
    return (ft_nullptr);
}

FT_TEST(test_sphere_setters_getters_contention_high_load_soak_rounds)
{
    sphere shape;
    std::atomic<bool> worker_failed(false);
    pthread_t writer_thread;
    pthread_t reader_thread;
    sphere_soak_worker_args writer_arguments;
    sphere_soak_worker_args reader_arguments;
    int round_index;
    long join_timeout_ms;
    int32_t writer_join_result;
    int32_t reader_join_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.enable_thread_safety());
    join_timeout_ms = 5000;
    round_index = 0;
    while (round_index < 1)
    {
        worker_failed.store(false);
        writer_arguments.shape = &shape;
        writer_arguments.worker_failed = &worker_failed;
        reader_arguments.shape = &shape;
        reader_arguments.worker_failed = &worker_failed;
        writer_join_result = pt_thread_create(&writer_thread, ft_nullptr,
                sphere_soak_writer_worker, &writer_arguments);
        FT_ASSERT_EQ(0, writer_join_result);
        reader_join_result = pt_thread_create(&reader_thread, ft_nullptr,
                sphere_soak_reader_worker, &reader_arguments);
        FT_ASSERT_EQ(0, reader_join_result);
        writer_join_result = pt_thread_timed_join(writer_thread, ft_nullptr,
                join_timeout_ms);
        if (writer_join_result != 0)
            (void)pt_thread_detach(writer_thread);
        FT_ASSERT_EQ(0, writer_join_result);
        reader_join_result = pt_thread_timed_join(reader_thread, ft_nullptr,
                join_timeout_ms);
        if (reader_join_result != 0)
            (void)pt_thread_detach(reader_thread);
        FT_ASSERT_EQ(0, reader_join_result);
        FT_ASSERT_EQ(false, worker_failed.load());
        round_index = round_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_intersect_sphere_high_load_mutating_overlap_soak_rounds)
{
    sphere *first;
    sphere *second;
    std::atomic<bool> *worker_failed;
    pthread_t writer_thread;
    pthread_t reader_thread;
    sphere_intersect_soak_worker_args *writer_arguments;
    sphere_intersect_soak_worker_args *reader_arguments;
    int round_index;
    long join_timeout_ms;
    int32_t writer_join_result;
    int32_t reader_join_result;

    first = new (std::nothrow) sphere();
    second = new (std::nothrow) sphere();
    worker_failed = new (std::nothrow) std::atomic<bool>(false);
    writer_arguments = new (std::nothrow) sphere_intersect_soak_worker_args();
    reader_arguments = new (std::nothrow) sphere_intersect_soak_worker_args();
    FT_ASSERT(first != ft_nullptr);
    FT_ASSERT(second != ft_nullptr);
    FT_ASSERT(worker_failed != ft_nullptr);
    FT_ASSERT(writer_arguments != ft_nullptr);
    FT_ASSERT(reader_arguments != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->initialize(0.0, 0.0, 0.0, 15.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->initialize(1.0, 1.0, 1.0, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->enable_thread_safety());
    join_timeout_ms = 5000;
    round_index = 0;
    while (round_index < 3)
    {
        worker_failed->store(false);
        writer_arguments->first = first;
        writer_arguments->second = second;
        writer_arguments->worker_failed = worker_failed;
        reader_arguments->first = first;
        reader_arguments->second = second;
        reader_arguments->worker_failed = worker_failed;
        writer_join_result = pt_thread_create(&writer_thread, ft_nullptr,
                sphere_intersect_soak_writer_worker, writer_arguments);
        FT_ASSERT_EQ(0, writer_join_result);
        reader_join_result = pt_thread_create(&reader_thread, ft_nullptr,
                sphere_intersect_soak_reader_worker, reader_arguments);
        FT_ASSERT_EQ(0, reader_join_result);
        writer_join_result = pt_thread_timed_join(writer_thread, ft_nullptr,
                join_timeout_ms);
        if (writer_join_result != 0)
            (void)pt_thread_detach(writer_thread);
        FT_ASSERT_EQ(0, writer_join_result);
        reader_join_result = pt_thread_timed_join(reader_thread, ft_nullptr,
                join_timeout_ms);
        if (reader_join_result != 0)
            (void)pt_thread_detach(reader_thread);
        FT_ASSERT_EQ(0, reader_join_result);
        FT_ASSERT_EQ(false, worker_failed->load());
        round_index = round_index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->destroy());
    delete reader_arguments;
    delete writer_arguments;
    delete worker_failed;
    delete second;
    delete first;
    return (1);
}
