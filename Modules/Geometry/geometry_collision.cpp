#include "geometry.hpp"
#include "geometry_pair_lock.hpp"

ft_bool    intersect_aabb(const aabb &first, const aabb &second)
{
    const pt_recursive_mutex *lower = first._mutex;
    const pt_recursive_mutex *upper = second._mutex;
    if (geometry_lock_pair(lower, upper) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const ft_bool separated = first._maximum_x < second._minimum_x
        || first._minimum_x > second._maximum_x
        || first._maximum_y < second._minimum_y
        || first._minimum_y > second._maximum_y;
    geometry_unlock_pair(lower, upper);
    return (!separated);
}

ft_bool    intersect_circle(const circle &first, const circle &second)
{
    const pt_recursive_mutex *lower = first._mutex;
    const pt_recursive_mutex *upper = second._mutex;
    if (geometry_lock_pair(lower, upper) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const double delta_x = first._center_x - second._center_x;
    const double delta_y = first._center_y - second._center_y;
    const double radius_sum = first._radius + second._radius;
    geometry_unlock_pair(lower, upper);
    const double distance_squared = delta_x * delta_x + delta_y * delta_y;
    return (!(distance_squared > radius_sum * radius_sum));
}

ft_bool    intersect_sphere(const sphere &first, const sphere &second)
{
    const pt_recursive_mutex *lower = first._mutex;
    const pt_recursive_mutex *upper = second._mutex;
    if (geometry_lock_pair(lower, upper) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const double delta_x = first._center_x - second._center_x;
    const double delta_y = first._center_y - second._center_y;
    const double delta_z = first._center_z - second._center_z;
    const double radius_sum = first._radius + second._radius;
    geometry_unlock_pair(lower, upper);
    const double distance_squared = delta_x * delta_x + delta_y * delta_y
        + delta_z * delta_z;
    return (!(distance_squared > radius_sum * radius_sum));
}
