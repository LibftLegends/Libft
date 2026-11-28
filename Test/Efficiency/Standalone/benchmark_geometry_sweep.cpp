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

int main()
{
    sphere shape;
    geometry_aabb3 box;
    vector3 velocity(5.0, 0.0, 0.0);
    if (shape.initialize(0.0, 0.0, 0.0, 1.0) != FT_ERR_SUCCESS
        || box.minimum.initialize(3.0, -1.0, -1.0) != FT_ERR_SUCCESS
        || box.maximum.initialize(4.0, 1.0, 1.0) != FT_ERR_SUCCESS)
        return (1);
    const auto overlaps = [&shape, &box]()
    {
        return (geometry_sphere_intersect_aabb3(shape, box));
    };
    const auto sweep = [&shape, &velocity, &box]()
    {
        return (geometry_swept_sphere_intersect_aabb3(shape, velocity, box, ft_nullptr));
    };
    const auto sweep_with_hit = [&shape, &velocity, &box]()
    {
        geometry_sweep_hit hit;
        if (!geometry_swept_sphere_intersect_aabb3(shape, velocity, box, &hit))
            return (0.0);
        return (hit.time + hit.point.get_x() + hit.normal.get_x());
    };
    benchmark_operation("sphere box miss", overlaps);
    benchmark_operation("sphere sweep boolean", sweep);
    benchmark_operation("sphere sweep with hit", sweep_with_hit);
    if (shape.enable_thread_safety() != FT_ERR_SUCCESS
        || box.minimum.enable_thread_safety() != FT_ERR_SUCCESS
        || box.maximum.enable_thread_safety() != FT_ERR_SUCCESS
        || velocity.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    benchmark_operation("guarded sphere box miss", overlaps);
    benchmark_operation("guarded sphere sweep with hit", sweep_with_hit);
    return (0);
}
