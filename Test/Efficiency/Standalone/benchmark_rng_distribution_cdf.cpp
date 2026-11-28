#include "../../../Modules/RNG/rng.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>

struct cdf_arguments
{
    double first_parameter;
    double second_parameter;
    double value;
};

using cdf_function = double (*)(double, double, double);

template<ft_size_t CaseCount>
static int32_t measure_cdf(const char *label, cdf_function calculate,
    const cdf_arguments (&cases)[CaseCount])
{
    constexpr uint32_t ITERATIONS = 3000U;
    double checksum = 0.0;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const cdf_arguments &arguments = cases[iteration % CaseCount];
        checksum += calculate(arguments.first_parameter, arguments.second_parameter, arguments.value);
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns per CDF, checksum %a\n", label, elapsed_ns / ITERATIONS, checksum);
    ft_size_t index = 0;
    while (index < CaseCount)
    {
        const cdf_arguments &arguments = cases[index];
        const double result = calculate(arguments.first_parameter, arguments.second_parameter,
            arguments.value);
        if (!std::isfinite(result) || result < 0.0 || result > 1.0)
            return (1);
        std::printf("%s result: %a\n", label, result);
        index += 1;
    }
    return (0);
}

int32_t main()
{
    constexpr cdf_arguments GAMMA_CASES[] = {
        {2.5, 1.5, 3.0}, {0.5, 2.0, 1.0}, {1.0, 2.0, 0.0000000000001},
        {30.0, 1.0, 28.0}, {45.0, 1.0, 70.0}, {1.5, 1.0, 0.0}
    };
    constexpr cdf_arguments BETA_CASES[] = {
        {2.0, 5.0, 0.4}, {0.5, 0.5, 0.5}, {0.5, 2.0, 0.9999999999999999},
        {1.0, 2.0, 0.0000000000001}, {62.0, 67.0, 0.5}, {3.0, 4.0, 1.0}
    };
    const int32_t gamma_result = measure_cdf("gamma", rng_gamma_cdf, GAMMA_CASES);
    const int32_t beta_result = measure_cdf("beta", rng_beta_cdf, BETA_CASES);
    if (gamma_result != 0 || beta_result != 0)
        return (1);
    return (0);
}
