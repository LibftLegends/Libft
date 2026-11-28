#include "../../../Modules/Geometry/geometry_3d.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <chrono>
#include <cstdio>

template <typename Operation>
static void benchmark_operation(const char *label, Operation operation)
{
    constexpr uint32_t ITERATIONS = 262144U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0U;
    while (index < ITERATIONS)
    {
        checksum += operation();
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %" FT_PRIu64 "\n", label,
        elapsed / ITERATIONS, checksum);
    return ;
}

int32_t main()
{
    const double normals[6][3] = {
        {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}, {0.0, -1.0, 0.0},
        {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}
    };
    geometry_frustum frustum;
    geometry_aabb3 box;
    vector3 point(0.25, -0.25, 0.0);
    if (box.minimum.initialize(-0.5, -0.5, -0.5) != FT_ERR_SUCCESS
        || box.maximum.initialize(0.5, 0.5, 0.5) != FT_ERR_SUCCESS)
        return (1);
    ft_size_t index = 0U;
    while (index < 6U)
    {
        geometry_plane &plane = frustum.planes[index];
        if (plane.normal.initialize(normals[index][0], normals[index][1], normals[index][2])
            != FT_ERR_SUCCESS)
            return (1);
        plane.distance = 1.0;
        index += 1U;
    }
    const auto test_point = [&frustum, &point]()
    {
        return (geometry_frustum_contains_point(frustum, point));
    };
    const auto test_box = [&frustum, &box]()
    {
        return (geometry_frustum_intersect_aabb3(frustum, box));
    };
    benchmark_operation("frustum point", test_point);
    benchmark_operation("frustum box", test_box);

    index = 0U;
    while (index < 6U)
    {
        if (frustum.planes[index].normal.enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    if (point.enable_thread_safety() != FT_ERR_SUCCESS
        || box.minimum.enable_thread_safety() != FT_ERR_SUCCESS
        || box.maximum.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    benchmark_operation("frustum guarded point", test_point);
    benchmark_operation("frustum guarded box", test_box);
    return (0);
}
