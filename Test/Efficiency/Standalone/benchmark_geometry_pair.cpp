#include "../../../Modules/Geometry/geometry.hpp"
#include <chrono>
#include <cstdio>

template <typename Shape>
static void benchmark_pair(const char *label, const Shape &first,
    const Shape &second, const Shape &separated,
    ft_bool (*intersect)(const Shape &, const Shape &))
{
    constexpr uint32_t ITERATIONS = 262144U;
    uint32_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        checksum += intersect(first, second);
        checksum += intersect(first, separated);
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %u\n", label,
        elapsed / (2U * ITERATIONS), checksum);
    return ;
}

int32_t main()
{
    aabb first_box;
    aabb second_box;
    aabb separated_box;
    circle first_circle;
    circle second_circle;
    circle separated_circle;
    sphere first_sphere;
    sphere second_sphere;
    sphere separated_sphere;
    if (first_box.initialize(0.0, 0.0, 2.0, 2.0) != FT_ERR_SUCCESS
        || second_box.initialize(1.0, 1.0, 3.0, 3.0) != FT_ERR_SUCCESS
        || separated_box.initialize(4.0, 4.0, 6.0, 6.0) != FT_ERR_SUCCESS
        || first_circle.initialize(0.0, 0.0, 2.0) != FT_ERR_SUCCESS
        || second_circle.initialize(1.0, 1.0, 2.0) != FT_ERR_SUCCESS
        || separated_circle.initialize(4.0, 4.0, 2.0) != FT_ERR_SUCCESS
        || first_sphere.initialize(0.0, 0.0, 0.0, 2.0) != FT_ERR_SUCCESS
        || second_sphere.initialize(1.0, 1.0, 1.0, 2.0) != FT_ERR_SUCCESS
        || separated_sphere.initialize(4.0, 4.0, 4.0, 2.0) != FT_ERR_SUCCESS)
        return (1);
    benchmark_pair("aabb pair", first_box, second_box, separated_box, intersect_aabb);
    benchmark_pair("circle pair", first_circle, second_circle, separated_circle,
        intersect_circle);
    benchmark_pair("sphere pair", first_sphere, second_sphere, separated_sphere,
        intersect_sphere);
    if (first_sphere.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    benchmark_pair("mixed sphere pair", first_sphere, second_sphere, separated_sphere,
        intersect_sphere);
    if (second_sphere.enable_thread_safety() != FT_ERR_SUCCESS
        || separated_sphere.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    benchmark_pair("guarded sphere pair", first_sphere, second_sphere, separated_sphere,
        intersect_sphere);
    return (0);
}
