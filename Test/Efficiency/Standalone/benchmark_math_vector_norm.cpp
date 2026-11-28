#include "../../../Modules/Math/linear_algebra.hpp"
#include <chrono>
#include <cstdio>

template <typename Operation>
static void benchmark_operation(const char *label, Operation operation)
{
    constexpr uint32_t ITERATIONS = 1048576U;
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
    constexpr uint32_t SAMPLE_COUNT = 128U;
    vector3 samples[SAMPLE_COUNT];
    uint32_t index = 0U;
    while (index < SAMPLE_COUNT)
    {
        const double scale = static_cast<double>(index + 1U);
        if (samples[index].initialize(3.0 * scale, 4.0 * scale, 12.0 * scale)
            != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    const vector3 reference(1.0, 2.0, 3.0);
    benchmark_operation("vector3 dot", [&samples, &reference](uint32_t sample)
    {
        return (samples[sample % SAMPLE_COUNT].dot(reference));
    });
    benchmark_operation("vector3 length", [&samples](uint32_t sample)
    {
        return (samples[sample % SAMPLE_COUNT].length());
    });
    benchmark_operation("vector3 normalize", [&samples](uint32_t sample)
    {
        const vector3 normalized = samples[sample % SAMPLE_COUNT].normalize();
        return (normalized.get_x());
    });
    return (0);
}
