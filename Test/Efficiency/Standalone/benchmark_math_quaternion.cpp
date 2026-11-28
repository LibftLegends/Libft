#include "../../../Modules/Math/quaternion.hpp"
#include <chrono>
#include <cstdio>

template <typename Operation>
static void benchmark_operation(const char *label, Operation operation)
{
    constexpr uint32_t ITERATIONS = 524288U;
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
    quaternion first(1.0, 2.0, 3.0, 4.0);
    quaternion second(5.0, 6.0, 7.0, 8.0);
    const auto multiply = [&first, &second]()
    {
        const quaternion result = first.multiply(second);
        return (result.get_w() + result.get_x() + result.get_y() + result.get_z());
    };
    benchmark_operation("quaternion multiply", multiply);
    benchmark_operation("quaternion normalize", [&first]()
    {
        const quaternion result = first.normalize();
        return (result.get_w());
    });
    quaternion source(13.0, 2.0, 3.0, 4.0);
    quaternion destination;
    benchmark_operation("quaternion move pair", [&source, &destination]()
    {
        const int32_t first_error = destination.move(source);
        const int32_t second_error = source.move(destination);
        return (source.get_w() + first_error + second_error);
    });
    if (first.enable_thread_safety() != FT_ERR_SUCCESS
        || second.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    benchmark_operation("quaternion guarded multiply", multiply);
    return (0);
}
