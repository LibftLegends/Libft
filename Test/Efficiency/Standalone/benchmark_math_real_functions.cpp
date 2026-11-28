#include "../../../Modules/Math/math.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>

static int32_t benchmark_operation(const char *label,
    double (*operation)(double), double (*reference)(double),
    double range_start, double range_end)
{
    constexpr uint32_t SAMPLE_COUNT = 1024U;
    constexpr uint32_t REPETITIONS = 128U;
    double inputs[SAMPLE_COUNT];
    uint32_t index = 0U;
    double max_error = 0.0;
    while (index < SAMPLE_COUNT)
    {
        const uint32_t permuted_index = (index * 257U) % SAMPLE_COUNT;
        const double fraction = static_cast<double>(permuted_index)
            / (SAMPLE_COUNT - 1U);
        inputs[index] = range_start + fraction * (range_end - range_start);
        const double error = std::fabs(operation(inputs[index]) - reference(inputs[index]));
        if (!std::isfinite(error))
            return (1);
        if (error > max_error)
            max_error = error;
        index += 1U;
    }
    double checksum = 0.0;
    uint32_t repetition = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (repetition < REPETITIONS)
    {
        index = 0U;
        while (index < SAMPLE_COUNT)
        {
            checksum += operation(inputs[index]);
            index += 1U;
        }
        repetition += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, max error %.4g, checksum %.9g\n",
        label, elapsed / (SAMPLE_COUNT * REPETITIONS), max_error, checksum);
    return (0);
}

int32_t main()
{
    if (benchmark_operation("sin", ft_sin, std::sin, -6.28, 6.28) != 0
        || benchmark_operation("cos", math_cos, std::cos, -6.28, 6.28) != 0
        || benchmark_operation("tan", ft_tan, std::tan, -1.55, 1.55) != 0
        || benchmark_operation("acos", math_acos, std::acos, -0.999, 0.999) != 0
        || benchmark_operation("sqrt", math_sqrt, std::sqrt, 0.01, 1000.0) != 0)
        return (1);
    return (0);
}
