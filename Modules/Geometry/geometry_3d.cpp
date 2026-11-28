#include "geometry.hpp"
#include "../Math/math_vector_norm.hpp"

#include <cmath>
#include <utility>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static constexpr double GEOMETRY_EPSILON = 1e-7;

static double geometry_min_double(double first_value, double second_value)
{
    if (first_value < second_value)
    {
        return (first_value);
    }
    return (second_value);
}

static double geometry_max_double(double first_value, double second_value)
{
    if (first_value > second_value)
    {
        return (first_value);
    }
    return (second_value);
}

static double geometry_clamp_double(double value, double minimum_value,
    double maximum_value)
{
    if (value < minimum_value)
    {
        return (minimum_value);
    }
    if (value > maximum_value)
    {
        return (maximum_value);
    }
    return (value);
}

static ft_bool geometry_snapshot_vector(const vector3 &vector, double (&components)[3])
{
    return (vector.get_components(components[0], components[1], components[2])
        == FT_ERR_SUCCESS);
}

static double geometry_dot(const double (&first)[3], const double (&second)[3])
{
    return (first[0] * second[0] + first[1] * second[1] + first[2] * second[2]);
}

static void geometry_subtract(const double (&first)[3], const double (&second)[3],
    double (&result)[3])
{
    result[0] = first[0] - second[0];
    result[1] = first[1] - second[1];
    result[2] = first[2] - second[2];
    return ;
}

static void geometry_cross(const double (&first)[3], const double (&second)[3],
    double (&result)[3])
{
    result[0] = first[1] * second[2] - first[2] * second[1];
    result[1] = first[2] * second[0] - first[0] * second[2];
    result[2] = first[0] * second[1] - first[1] * second[0];
    return ;
}

static ft_bool geometry_ray_axis_interval(double origin, double direction,
    double minimum, double maximum, double &near_distance, double &far_distance)
{
    if (std::fabs(direction) < GEOMETRY_EPSILON)
        return (!(origin < minimum || origin > maximum));

    const double inverse_direction = 1.0 / direction;
    double first = (minimum - origin) * inverse_direction;
    double second = (maximum - origin) * inverse_direction;
    if (first > second)
        std::swap(first, second);
    near_distance = geometry_max_double(near_distance, first);
    far_distance = geometry_min_double(far_distance, second);
    return (!(near_distance > far_distance));
}

static void geometry_ray_point(const double (&origin)[3], const double (&direction)[3],
    double distance, double (&position)[3])
{
    ft_size_t axis = 0U;
    while (axis < 3U)
    {
        position[axis] = origin[axis] + direction[axis] * distance;
        axis += 1U;
    }
    return ;
}

static ft_bool geometry_store_ray_hit(geometry_raycast_hit &hit, double distance,
    const double (&position)[3], const double (&normal)[3])
{
    if (hit.point.initialize(position[0], position[1], position[2]) != FT_ERR_SUCCESS
        || hit.normal.initialize(normal[0], normal[1], normal[2]) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    hit.distance = distance;
    return (FT_TRUE);
}

static void geometry_box_hit_normal(const double (&position)[3], const double (&direction)[3],
    const double (&minimum)[3], const double (&maximum)[3], double (&normal)[3])
{
    ft_size_t axis = 0U;
    while (axis < 3U)
    {
        if (std::fabs(position[axis] - minimum[axis]) < GEOMETRY_EPSILON)
        {
            normal[axis] = -1.0;
            return ;
        }
        if (std::fabs(position[axis] - maximum[axis]) < GEOMETRY_EPSILON)
        {
            normal[axis] = 1.0;
            return ;
        }
        axis += 1U;
    }
    if (std::fabs(direction[0]) >= GEOMETRY_EPSILON)
        normal[0] = -direction[0] / std::fabs(direction[0]);
    return ;
}

ft_bool geometry_ray_intersect_plane(const geometry_ray &ray,
    const geometry_plane &plane, double *distance)
{
    double direction[3];
    double normal[3];
    if (!geometry_snapshot_vector(ray.direction, direction)
        || !geometry_snapshot_vector(plane.normal, normal))
        return (FT_FALSE);
    const double denominator = geometry_dot(direction, normal);
    if (std::fabs(denominator) < GEOMETRY_EPSILON)
        return (FT_FALSE);

    double origin[3];
    if (!geometry_snapshot_vector(ray.origin, origin))
        return (FT_FALSE);
    const double intersection = -(geometry_dot(origin, normal) + plane.distance) / denominator;
    if (intersection < 0.0)
        return (FT_FALSE);
    if (distance != ft_nullptr)
        *distance = intersection;
    return (FT_TRUE);
}

ft_bool geometry_ray_intersect_triangle(const geometry_ray &ray,
    const vector3 &vertex_a, const vector3 &vertex_b, const vector3 &vertex_c,
    geometry_raycast_hit *hit)
{
    double direction[3];
    double first[3];
    double second[3];
    double third[3];
    if (!geometry_snapshot_vector(ray.direction, direction)
        || !geometry_snapshot_vector(vertex_a, first)
        || !geometry_snapshot_vector(vertex_b, second)
        || !geometry_snapshot_vector(vertex_c, third))
        return (FT_FALSE);

    double edge_ab[3];
    double edge_ac[3];
    double direction_cross_ac[3];
    geometry_subtract(second, first, edge_ab);
    geometry_subtract(third, first, edge_ac);
    geometry_cross(direction, edge_ac, direction_cross_ac);
    const double determinant = geometry_dot(edge_ab, direction_cross_ac);
    if (std::fabs(determinant) < GEOMETRY_EPSILON)
        return (FT_FALSE);

    double origin[3];
    if (!geometry_snapshot_vector(ray.origin, origin))
        return (FT_FALSE);
    double from_vertex[3];
    geometry_subtract(origin, first, from_vertex);
    const double inverse = 1.0 / determinant;
    const double barycentric_u = geometry_dot(from_vertex, direction_cross_ac) * inverse;
    if (barycentric_u < 0.0 || barycentric_u > 1.0)
        return (FT_FALSE);

    double origin_cross_ab[3];
    geometry_cross(from_vertex, edge_ab, origin_cross_ab);
    const double barycentric_v = geometry_dot(direction, origin_cross_ab) * inverse;
    if (barycentric_v < 0.0 || barycentric_u + barycentric_v > 1.0)
        return (FT_FALSE);
    const double distance = geometry_dot(edge_ac, origin_cross_ab) * inverse;
    if (distance < 0.0)
        return (FT_FALSE);
    if (hit == ft_nullptr)
        return (FT_TRUE);

    double normal[3];
    geometry_cross(edge_ab, edge_ac, normal);
    if (!math_vector_normalize(normal))
        return (FT_FALSE);
    double position[3];
    geometry_ray_point(origin, direction, distance, position);
    return (geometry_store_ray_hit(*hit, distance, position, normal));
}

static ft_bool geometry_ray_box_interval(const double (&origin)[3], const double (&direction)[3],
    const double (&minimum)[3], const double (&maximum)[3], double limit, double &near_distance)
{
    near_distance = 0.0;
    double far_distance = limit;
    ft_size_t axis = 0U;
    while (axis < 3U)
    {
        if (!geometry_ray_axis_interval(origin[axis], direction[axis],
                minimum[axis], maximum[axis], near_distance, far_distance))
            return (FT_FALSE);
        axis += 1U;
    }
    return (FT_TRUE);
}

ft_bool geometry_ray_intersect_aabb3(const geometry_ray &ray,
    const geometry_aabb3 &box, geometry_raycast_hit *hit)
{
    double origin[3];
    double direction[3];
    double minimum[3];
    double maximum[3];
    if (!geometry_snapshot_vector(ray.origin, origin)
        || !geometry_snapshot_vector(ray.direction, direction)
        || !geometry_snapshot_vector(box.minimum, minimum)
        || !geometry_snapshot_vector(box.maximum, maximum))
        return (FT_FALSE);

    double near_distance;
    if (!geometry_ray_box_interval(origin, direction, minimum, maximum, 1.0e30, near_distance))
        return (FT_FALSE);
    if (hit == ft_nullptr)
        return (FT_TRUE);

    double position[3];
    double normal[3]{};
    geometry_ray_point(origin, direction, near_distance, position);
    geometry_box_hit_normal(position, direction, minimum, maximum, normal);
    return (geometry_store_ray_hit(*hit, near_distance, position, normal));
}

static ft_bool geometry_sphere_box_overlap(const double (&center)[3], double radius,
    const double (&minimum)[3], const double (&maximum)[3])
{
    double offset[3];
    ft_size_t axis = 0U;
    while (axis < 3U)
    {
        const double closest = geometry_clamp_double(center[axis], minimum[axis], maximum[axis]);
        offset[axis] = center[axis] - closest;
        axis += 1U;
    }
    const double squared_radius = radius * radius;
    if (std::isnormal(squared_radius))
        return (geometry_dot(offset, offset) <= squared_radius);
    return (math_vector_length(offset) <= std::fabs(radius));
}

ft_bool geometry_sphere_intersect_aabb3(const sphere &shape,
    const geometry_aabb3 &box)
{
    double center[3];
    double radius;
    double minimum[3];
    double maximum[3];
    if (!shape.snapshot(center, radius) || !geometry_snapshot_vector(box.minimum, minimum)
        || !geometry_snapshot_vector(box.maximum, maximum))
        return (FT_FALSE);
    return (geometry_sphere_box_overlap(center, radius, minimum, maximum));
}

ft_bool geometry_swept_sphere_intersect_aabb3(const sphere &shape,
    const vector3 &velocity, const geometry_aabb3 &box, geometry_sweep_hit *hit)
{
    double center[3];
    double radius;
    double minimum[3];
    double maximum[3];
    if (!shape.snapshot(center, radius) || !geometry_snapshot_vector(box.minimum, minimum)
        || !geometry_snapshot_vector(box.maximum, maximum))
        return (FT_FALSE);
    double time = 0.0;
    double position[3] = {center[0], center[1], center[2]};
    double normal[3]{};
    if (!geometry_sphere_box_overlap(center, radius, minimum, maximum))
    {
        double direction[3];
        if (!geometry_snapshot_vector(velocity, direction))
            return (FT_FALSE);
        ft_size_t axis = 0U;
        while (axis < 3U)
        {
            minimum[axis] -= radius;
            maximum[axis] += radius;
            axis += 1U;
        }
        if (!geometry_ray_box_interval(center, direction, minimum, maximum, 1.0, time))
            return (FT_FALSE);
        if (hit == ft_nullptr)
            return (FT_TRUE);
        geometry_ray_point(center, direction, time, position);
        geometry_box_hit_normal(position, direction, minimum, maximum, normal);
    }
    if (hit == ft_nullptr)
        return (FT_TRUE);
    if (hit->point.initialize(position[0], position[1], position[2]) != FT_ERR_SUCCESS
        || hit->normal.initialize(normal[0], normal[1], normal[2]) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    hit->time = time;
    return (FT_TRUE);
}

ft_bool geometry_frustum_contains_point(const geometry_frustum &frustum,
    const vector3 &point)
{
    double coordinates[3];
    if (!geometry_snapshot_vector(point, coordinates))
        return (FT_FALSE);
    ft_size_t plane_index = 0U;
    while (plane_index < sizeof(frustum.planes) / sizeof(frustum.planes[0]))
    {
        const geometry_plane &plane = frustum.planes[plane_index];
        double normal[3];
        if (!geometry_snapshot_vector(plane.normal, normal))
            return (FT_FALSE);
        const double distance = geometry_dot(coordinates, normal) + plane.distance;
        if (distance < 0.0)
            return (FT_FALSE);
        plane_index += 1U;
    }
    return (FT_TRUE);
}

ft_bool geometry_frustum_intersect_aabb3(const geometry_frustum &frustum,
    const geometry_aabb3 &box)
{
    double minimum[3];
    double maximum[3];
    if (!geometry_snapshot_vector(box.minimum, minimum)
        || !geometry_snapshot_vector(box.maximum, maximum))
        return (FT_FALSE);

    ft_size_t plane_index = 0U;
    while (plane_index < sizeof(frustum.planes) / sizeof(frustum.planes[0]))
    {
        const geometry_plane &plane = frustum.planes[plane_index];
        double normal[3];
        if (!geometry_snapshot_vector(plane.normal, normal))
            return (FT_FALSE);
        double vertex[3];
        ft_size_t axis = 0U;
        while (axis < 3U)
        {
            vertex[axis] = minimum[axis];
            if (normal[axis] >= 0.0)
                vertex[axis] = maximum[axis];
            axis += 1U;
        }
        const double distance = geometry_dot(vertex, normal) + plane.distance;
        if (distance < 0.0)
            return (FT_FALSE);
        plane_index += 1U;
    }
    return (FT_TRUE);
}
