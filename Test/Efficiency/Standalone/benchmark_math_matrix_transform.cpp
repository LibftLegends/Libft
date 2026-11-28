#include "../../../Modules/Math/linear_algebra.hpp"
#include <chrono>
#include <cstdio>

template <typename Operation>
static void benchmark_operation(const char *label, Operation operation)
{
    constexpr uint32_t ITERATIONS = 262144U;
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
    matrix4 first(2.0, 3.0, 5.0, 7.0,
                  11.0, 13.0, 17.0, 19.0,
                  23.0, 29.0, 31.0, 37.0,
                  41.0, 43.0, 47.0, 53.0);
    matrix4 second(1.0, 0.0, 1.0, 4.0,
                   3.0, 11.0, 7.0, 8.0,
                   9.0, 2.0, 5.0, 10.0,
                   7.0, 6.0, 4.0, 12.0);
    vector4 point(1.0, 2.0, 3.0, 1.0);
    const auto transform = [&first, &point]()
    {
        const vector4 result = first.transform(point);
        return (result.get_x() + result.get_y());
    };
    const auto multiply_then_transform = [&first, &second, &point]()
    {
        const vector4 result = first.multiply(second).transform(point);
        return (result.get_x() + result.get_y());
    };
    benchmark_operation("matrix4 transform", transform);
    benchmark_operation("matrix4 multiply and transform", multiply_then_transform);
    if (first.enable_thread_safety() != FT_ERR_SUCCESS
        || point.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    benchmark_operation("matrix4 guarded transform", transform);
    return (0);
}
