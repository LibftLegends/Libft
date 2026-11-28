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
    std::printf("%s: %.2f ns/op, checksum %.17g\n", label, elapsed / ITERATIONS, checksum);
    return ;
}

int32_t main()
{
    const matrix2 small(4.0, 7.0, 2.0, 6.0);
    const vector2 point_small(1.0, 3.0);
    matrix4 dense(11.0, 2.0, 3.0, 4.0,
                  3.0, 13.0, 5.0, 2.0,
                  4.0, 5.0, 17.0, 3.0,
                  2.0, 1.0, 4.0, 19.0);
    const matrix4 translation = matrix4::make_translation(3.0, 5.0, 7.0);
    const vector4 point(1.0, 2.0, 3.0, 1.0);

    benchmark_operation("matrix2 inverse and transform", [&small, &point_small]()
    {
        const vector2 result = small.invert().transform(point_small);
        return (result.get_x());
    });
    benchmark_operation("matrix4 translation inverse and transform", [&translation, &point]()
    {
        const vector4 result = translation.invert().transform(point);
        return (result.get_x() + result.get_y());
    });
    const auto invert_dense = [&dense, &point]()
    {
        const vector4 result = dense.invert().transform(point);
        return (result.get_x() + result.get_y());
    };
    benchmark_operation("matrix4 dense inverse and transform", invert_dense);
    if (dense.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    benchmark_operation("matrix4 guarded dense inverse and transform", invert_dense);
    return (0);
}
