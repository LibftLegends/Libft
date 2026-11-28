#include "../test_internal.hpp"
#include "../../Modules/Geometry/geometry.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <limits>

extern std::atomic<int32_t> pt_recursive_mutex_lock_override_error_code;
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/pthread_lock_tracking.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

static int32_t initialize_vector_or_fail(vector3 &value, double x_component,
    double y_component, double z_component)
{
    return (value.initialize(x_component, y_component, z_component));
}

static int32_t initialize_ray_or_fail(geometry_ray &ray, double origin_x,
    double origin_y, double origin_z, double direction_x, double direction_y,
    double direction_z)
{
    if (initialize_vector_or_fail(ray.origin, origin_x, origin_y, origin_z)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    return (initialize_vector_or_fail(ray.direction, direction_x, direction_y,
        direction_z));
}

static int32_t initialize_aabb3_or_fail(geometry_aabb3 &box, double minimum_x,
    double minimum_y, double minimum_z, double maximum_x, double maximum_y,
    double maximum_z)
{
    if (initialize_vector_or_fail(box.minimum, minimum_x, minimum_y, minimum_z)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    return (initialize_vector_or_fail(box.maximum, maximum_x, maximum_y,
        maximum_z));
}

static int32_t initialize_plane_or_fail(geometry_plane &plane, double normal_x,
    double normal_y, double normal_z, double distance)
{
    if (initialize_vector_or_fail(plane.normal, normal_x, normal_y, normal_z)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    plane.distance = distance;
    return (FT_ERR_SUCCESS);
}

static int32_t initialize_unit_cube_frustum_or_fail(geometry_frustum &frustum)
{
    if (initialize_plane_or_fail(frustum.planes[0], 1.0, 0.0, 0.0, 1.0)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    if (initialize_plane_or_fail(frustum.planes[1], -1.0, 0.0, 0.0, 1.0)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    if (initialize_plane_or_fail(frustum.planes[2], 0.0, 1.0, 0.0, 1.0)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    if (initialize_plane_or_fail(frustum.planes[3], 0.0, -1.0, 0.0, 1.0)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    if (initialize_plane_or_fail(frustum.planes[4], 0.0, 0.0, 1.0, 1.0)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    return (initialize_plane_or_fail(frustum.planes[5], 0.0, 0.0, -1.0,
        1.0));
}

FT_TEST(test_geometry_ray_intersect_plane)
{
    geometry_ray ray;
    geometry_plane plane;
    double distance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_ray_or_fail(ray, 0.0, 0.0, 0.0,
        1.0, 0.0, 0.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_plane_or_fail(plane, 1.0, 0.0,
        0.0, -5.0));
    distance = 0.0;
    FT_ASSERT_EQ(FT_TRUE, geometry_ray_intersect_plane(ray, plane, &distance));
    FT_ASSERT_DOUBLE_EQ(5.0, distance);
    return (1);
}

FT_TEST(test_geometry_ray_intersect_triangle)
{
    geometry_ray ray;
    vector3 vertex_a;
    vector3 vertex_b;
    vector3 vertex_c;
    geometry_raycast_hit hit;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_ray_or_fail(ray, 0.25, 0.25, 0.0,
        0.0, 0.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_vector_or_fail(vertex_a, 0.0,
        0.0, 5.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_vector_or_fail(vertex_b, 1.0,
        0.0, 5.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_vector_or_fail(vertex_c, 0.0,
        1.0, 5.0));
    FT_ASSERT_EQ(FT_TRUE, geometry_ray_intersect_triangle(ray, vertex_a,
        vertex_b, vertex_c, &hit));
    FT_ASSERT_DOUBLE_EQ(5.0, hit.distance);
    FT_ASSERT_DOUBLE_EQ(0.25, hit.point.get_x());
    FT_ASSERT_DOUBLE_EQ(0.25, hit.point.get_y());
    FT_ASSERT_DOUBLE_EQ(5.0, hit.point.get_z());
    return (1);
}

FT_TEST(test_geometry_ray_intersect_aabb3)
{
    geometry_ray ray;
    geometry_aabb3 box;
    geometry_raycast_hit hit;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_ray_or_fail(ray, -2.0, 0.0, 0.0,
        1.0, 0.0, 0.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(box, -1.0, -1.0,
        -1.0, 1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_TRUE, geometry_ray_intersect_aabb3(ray, box, &hit));
    FT_ASSERT_DOUBLE_EQ(1.0, hit.distance);
    FT_ASSERT_DOUBLE_EQ(-1.0, hit.point.get_x());
    FT_ASSERT_DOUBLE_EQ(-1.0, hit.normal.get_x());
    return (1);
}

FT_TEST(test_geometry_sphere_intersect_aabb3)
{
    sphere shape;
    geometry_aabb3 box;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(2.0, 0.0, 0.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(box, -1.0, -1.0,
        -1.0, 1.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_TRUE, geometry_sphere_intersect_aabb3(shape, box));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_geometry_swept_sphere_intersect_aabb3)
{
    sphere shape;
    vector3 velocity;
    geometry_aabb3 box;
    geometry_sweep_hit hit;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_vector_or_fail(velocity, 5.0,
        0.0, 0.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(box, 3.0, -1.0,
        -1.0, 4.0, 1.0, 1.0));
    FT_ASSERT_EQ(FT_TRUE, geometry_swept_sphere_intersect_aabb3(shape,
        velocity, box, &hit));
    FT_ASSERT_DOUBLE_EQ(0.4, hit.time);
    FT_ASSERT_DOUBLE_EQ(2.0, hit.point.get_x());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_geometry_swept_sphere_intersect_aabb3_initial_overlap)
{
    sphere shape;
    vector3 velocity;
    geometry_aabb3 box;
    geometry_sweep_hit hit;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_vector_or_fail(velocity, 5.0,
        0.0, 0.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(box, -0.5, -0.5,
        -0.5, 0.5, 0.5, 0.5));
    FT_ASSERT_EQ(FT_TRUE, geometry_swept_sphere_intersect_aabb3(shape,
        velocity, box, &hit));
    FT_ASSERT_DOUBLE_EQ(0.0, hit.time);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.destroy());
    return (1);
}

FT_TEST(test_geometry_frustum_checks)
{
    geometry_frustum frustum;
    geometry_aabb3 inside_box;
    geometry_aabb3 outside_box;
    vector3 point;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_unit_cube_frustum_or_fail(frustum));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_vector_or_fail(point, 0.0, 0.0,
        0.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(inside_box, -0.5,
        -0.5, -0.5, 0.5, 0.5, 0.5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(outside_box, 2.0,
        2.0, 2.0, 3.0, 3.0, 3.0));
    FT_ASSERT_EQ(FT_TRUE, geometry_frustum_contains_point(frustum, point));
    FT_ASSERT_EQ(FT_TRUE, geometry_frustum_intersect_aabb3(frustum,
        inside_box));
    FT_ASSERT_EQ(FT_FALSE, geometry_frustum_intersect_aabb3(frustum,
        outside_box));
    return (1);
}

FT_TEST(test_geometry_frustum_propagates_snapshot_failure)
{
    geometry_frustum frustum;
    geometry_aabb3 inside;
    vector3 point;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_unit_cube_frustum_or_fail(frustum));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_vector_or_fail(point, 0.0, 0.0, 0.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(inside, -0.5, -0.5,
        -0.5, 0.5, 0.5, 0.5));
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS),
        frustum.planes[1].normal.enable_thread_safety());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_INTERNAL);
    const ft_bool contains_point = geometry_frustum_contains_point(frustum, point);
    const ft_bool intersects_box = geometry_frustum_intersect_aabb3(frustum, inside);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_FALSE, contains_point);
    FT_ASSERT_EQ(FT_FALSE, intersects_box);
    FT_ASSERT_EQ(FT_TRUE, geometry_frustum_contains_point(frustum, point));
    FT_ASSERT_EQ(FT_TRUE, geometry_frustum_intersect_aabb3(frustum, inside));
    return (1);
}

FT_TEST(test_geometry_ray_snapshot_failure_does_not_publish_a_hit)
{
    geometry_ray ray;
    geometry_plane plane;
    geometry_aabb3 box;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_ray_or_fail(ray, -2.0, 0.0, 0.0,
        1.0, 0.0, 0.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_plane_or_fail(plane, 1.0, 0.0, 0.0, 0.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(box, -1.0, -1.0,
        -1.0, 1.0, 1.0, 1.0));
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), ray.origin.enable_thread_safety());
    geometry_raycast_hit hit;
    hit.distance = 42.0;
    double distance = 43.0;
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_INTERNAL);
    const ft_bool plane_hit = geometry_ray_intersect_plane(ray, plane, &distance);
    const ft_bool box_hit = geometry_ray_intersect_aabb3(ray, box, &hit);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_FALSE, plane_hit);
    FT_ASSERT_EQ(FT_FALSE, box_hit);
    FT_ASSERT_EQ(43.0, distance);
    FT_ASSERT_EQ(42.0, hit.distance);
    FT_ASSERT_EQ(FT_TRUE, geometry_ray_intersect_aabb3(ray, box, &hit));
    FT_ASSERT_EQ(1.0, hit.distance);
    return (1);
}

FT_TEST(test_geometry_triangle_reports_reverse_winding_normal)
{
    geometry_ray ray;
    const vector3 first(0.0, 0.0, 5.0);
    const vector3 second(0.0, 1.0, 5.0);
    const vector3 third(1.0, 0.0, 5.0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_ray_or_fail(ray, 0.25, 0.25, 0.0,
        0.0, 0.0, 1.0));
    geometry_raycast_hit hit;
    FT_ASSERT_EQ(FT_TRUE, geometry_ray_intersect_triangle(ray, first, second, third, &hit));
    FT_ASSERT_EQ(5.0, hit.distance);
    FT_ASSERT_EQ(-1.0, hit.normal.get_z());
    return (1);
}

FT_TEST(test_geometry_sweep_honors_the_time_limit)
{
    sphere shape;
    geometry_aabb3 box;
    const vector3 too_short(0.5, 0.0, 0.0);
    const vector3 reaches_boundary(1.0, 0.0, 0.0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(box, 2.0, -1.0,
        -1.0, 3.0, 1.0, 1.0));
    geometry_sweep_hit hit;
    hit.time = 42.0;
    FT_ASSERT_EQ(FT_FALSE, geometry_swept_sphere_intersect_aabb3(shape, too_short, box, &hit));
    FT_ASSERT_EQ(42.0, hit.time);
    FT_ASSERT_EQ(FT_TRUE, geometry_swept_sphere_intersect_aabb3(
        shape, reaches_boundary, box, &hit));
    FT_ASSERT_EQ(1.0, hit.time);
    FT_ASSERT_EQ(1.0, hit.point.get_x());
    FT_ASSERT_EQ(-1.0, hit.normal.get_x());
    return (1);
}

FT_TEST(test_geometry_sphere_box_overlap_handles_extreme_radii)
{
    geometry_aabb3 point_box;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(point_box, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0));
    sphere large;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, large.initialize(1e308, 0.0, 0.0, 1e307));
    FT_ASSERT_EQ(FT_FALSE, geometry_sphere_intersect_aabb3(large, point_box));

    const double smallest = std::numeric_limits<double>::denorm_min();
    sphere tiny;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tiny.initialize(2.0 * smallest, 0.0, 0.0, smallest));
    FT_ASSERT_EQ(FT_FALSE, geometry_sphere_intersect_aabb3(tiny, point_box));
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), tiny.set_center_x(smallest));
    FT_ASSERT_EQ(FT_TRUE, geometry_sphere_intersect_aabb3(tiny, point_box));
    return (1);
}

FT_TEST(test_geometry_sphere_queries_detect_snapshot_failure)
{
    sphere shape;
    geometry_aabb3 box;
    const vector3 velocity(1.0, 0.0, 0.0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shape.initialize(0.0, 0.0, 0.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_aabb3_or_fail(box, -0.5, -0.5,
        -0.5, 0.5, 0.5, 0.5));
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), shape.enable_thread_safety());
    geometry_sweep_hit hit;
    hit.time = 42.0;
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_INTERNAL);
    const ft_bool overlaps = geometry_sphere_intersect_aabb3(shape, box);
    const ft_bool sweeps = geometry_swept_sphere_intersect_aabb3(shape, velocity, box, &hit);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_FALSE, overlaps);
    FT_ASSERT_EQ(FT_FALSE, sweeps);
    FT_ASSERT_EQ(42.0, hit.time);
    FT_ASSERT_EQ(FT_TRUE, geometry_swept_sphere_intersect_aabb3(shape, velocity, box, &hit));
    FT_ASSERT_EQ(0.0, hit.time);
    return (1);
}

template <typename Shape>
static int32_t check_geometry_move_mutex_cleanup()
{
    Shape source;
    Shape destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    const pt_recursive_mutex *source_mutex = source._mutex;

    pt_lock_tracking_notify_acquired_override_error_code.store(FT_ERR_NO_MEMORY);
    const uint32_t move_error = destination.move(source);
    pt_lock_tracking_notify_acquired_override_error_code.store(FT_ERR_SUCCESS);
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_NO_MEMORY), move_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    FT_ASSERT_EQ(source_mutex, source._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(ft_nullptr, destination._mutex);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(ft_nullptr, destination._mutex);
    return (1);
}

FT_TEST(test_geometry_move_cleans_up_residual_pair_mutexes)
{
    FT_ASSERT_EQ(1, check_geometry_move_mutex_cleanup<aabb>());
    FT_ASSERT_EQ(1, check_geometry_move_mutex_cleanup<circle>());
    FT_ASSERT_EQ(1, check_geometry_move_mutex_cleanup<sphere>());
    return (1);
}
