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
        checksum += operation(index);
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
    vector3 source(13.0, 5.0, 7.0);
    vector3 destination;
    benchmark_operation("vector3 move-init pair", [&source, &destination](uint32_t)
    {
        const int32_t first = destination.initialize(static_cast<vector3 &&>(source));
        const int32_t second = source.initialize(static_cast<vector3 &&>(destination));
        return (source.get_x() + first + second);
    });

    matrix4 matrix = matrix4::make_translation(3.0, 5.0, 7.0);
    matrix4 copy;
    const vector4 point(1.0, 2.0, 3.0, 1.0);
    benchmark_operation("matrix4 copy and transform", [&matrix, &copy, &point](uint32_t)
    {
        const int32_t error = copy.initialize(matrix);
        const vector4 transformed = copy.transform(point);
        return (transformed.get_z() + error);
    });
    benchmark_operation("matrix4 construct and transform", [&point](uint32_t sample)
    {
        const double offset = static_cast<double>(sample % 32U);
        const matrix4 transform = matrix4::make_translation(offset, 5.0, 7.0);
        const vector4 transformed = transform.transform(point);
        return (transformed.get_x());
    });
    return (0);
}
