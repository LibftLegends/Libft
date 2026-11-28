#include "../../../Modules/Math/math.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>

template <typename Operation, typename Reference>
static int32_t benchmark_operation(const char *label, Operation operation,
    Reference reference, double lower, double upper)
{
    constexpr uint32_t SAMPLE_COUNT = 1024U;
    constexpr uint32_t REPETITIONS = 64U;
    double inputs[SAMPLE_COUNT];
    double max_relative_error = 0.0;
    uint32_t index = 0U;
    while (index < SAMPLE_COUNT)
    {
        const uint32_t permuted_index = (index * 257U) % SAMPLE_COUNT;
        const double fraction = static_cast<double>(permuted_index) / (SAMPLE_COUNT - 1U);
        const double input = lower + fraction * (upper - lower);
        inputs[index] = input;
        const double expected = reference(input);
        const double actual = operation(input);
        if (!std::isfinite(actual))
            return (1);
        double error = std::fabs(actual - expected);
        if (expected != 0.0)
            error /= std::fabs(expected);
        if (error > max_relative_error)
            max_relative_error = error;
        index += 1U;
    }

    double checksum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    uint32_t repetition = 0U;
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
    std::printf("%s: %.2f ns/op, max relative error %.4g, checksum %.9g\n",
        label, elapsed / (SAMPLE_COUNT * REPETITIONS), max_relative_error, checksum);
    return (0);
}

int32_t main()
{
    const auto fmod_four = [](double value) { return (math_fmod(value, 4.0)); };
    const auto reference_fmod = [](double value) { return (std::fmod(value, 4.0)); };
    if (benchmark_operation("exp", math_exp, static_cast<double (*)(double)>(std::exp),
            -16.0, 16.0) != 0
        || benchmark_operation("log", math_log, static_cast<double (*)(double)>(std::log),
            0.05, 1.98) != 0
        || benchmark_operation("fabs", math_fabs, static_cast<double (*)(double)>(std::fabs),
            -64.0, 64.0) != 0
        || benchmark_operation("fmod", fmod_four, reference_fmod, -64.0, 64.0) != 0)
        return (1);
    return (0);
}
