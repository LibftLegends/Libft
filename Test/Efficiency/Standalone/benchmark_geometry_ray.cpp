#include "../../../Modules/Geometry/geometry_3d.hpp"
#include <chrono>
#include <cstdio>

template <typename Operation>
static void benchmark_operation(const char *label, Operation operation)
{
    constexpr uint32_t ITERATIONS = 131072U;
    double checksum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0U;
    while (index < ITERATIONS)
    {
        checksum += operation();
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %.17g\n", label,
        elapsed / ITERATIONS, checksum);
    return ;
}

int32_t main()
{
    geometry_ray ray;
    geometry_aabb3 box;
    geometry_plane plane;
    if (ray.origin.initialize(0.25, 0.25, 0.0) != FT_ERR_SUCCESS
        || ray.direction.initialize(0.0, 0.0, 1.0) != FT_ERR_SUCCESS
        || box.minimum.initialize(0.0, 0.0, 5.0) != FT_ERR_SUCCESS
        || box.maximum.initialize(1.0, 1.0, 6.0) != FT_ERR_SUCCESS
        || plane.normal.initialize(0.0, 0.0, 1.0) != FT_ERR_SUCCESS)
        return (1);
    plane.distance = -5.0;
    const vector3 first(0.0, 0.0, 5.0);
    const vector3 second(1.0, 0.0, 5.0);
    const vector3 third(0.0, 1.0, 5.0);
    benchmark_operation("ray plane", [&ray, &plane]()
    {
        double distance = 0.0;
        if (!geometry_ray_intersect_plane(ray, plane, &distance))
            return (0.0);
        return (distance);
    });
    benchmark_operation("ray triangle with hit", [&ray, &first, &second, &third]()
    {
        geometry_raycast_hit hit;
        if (!geometry_ray_intersect_triangle(ray, first, second, third, &hit))
            return (0.0);
        return (hit.distance + hit.normal.get_z());
    });
    const auto test_box = [&ray, &box]()
    {
        geometry_raycast_hit hit;
        if (!geometry_ray_intersect_aabb3(ray, box, &hit))
            return (0.0);
        return (hit.distance + hit.normal.get_z());
    };
    benchmark_operation("ray box with hit", test_box);
    if (ray.origin.enable_thread_safety() != FT_ERR_SUCCESS
        || ray.direction.enable_thread_safety() != FT_ERR_SUCCESS
        || box.minimum.enable_thread_safety() != FT_ERR_SUCCESS
        || box.maximum.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    benchmark_operation("guarded ray box with hit", test_box);
    return (0);
}
