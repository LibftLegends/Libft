#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/Math/ft_cubic_spline.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>

static double benchmark_exp(double value, void *context)
{
    uint64_t *calls = static_cast<uint64_t *>(context);
    *calls += 1U;
    return (std::exp(value));
}

static int32_t benchmark_simpson()
{
    constexpr uint32_t ITERATIONS = 4096U;
    uint64_t calls = 0U;
    double checksum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        double integral = 0.0;
        if (math_integrate_simpson(benchmark_exp, &calls, 0.0, 1.0, 1e-10,
                integral) != FT_ERR_SUCCESS)
            return (1);
        checksum += integral;
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("Simpson: %.2f ns/op, checksum %.17g, callbacks %.0f\n",
        elapsed / ITERATIONS, checksum, static_cast<double>(calls) / ITERATIONS);
    return (0);
}

static int32_t benchmark_quadratic()
{
    constexpr uint32_t ITERATIONS = 1048576U;
    double checksum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        const double root_one = static_cast<double>(iteration % 31U + 3U);
        const double root_two = static_cast<double>(iteration % 29U + 41U);
        double first;
        double second;
        if (math_polynomial_solve_quadratic(1.0, -root_one - root_two,
                root_one * root_two, first, second) != FT_ERR_SUCCESS)
            return (1);
        checksum += first + second;
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("Quadratic: %.2f ns/op, checksum %.17g, callbacks 0\n",
        elapsed / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (benchmark_simpson() != 0 || benchmark_quadratic() != 0)
        return (1);
    return (0);
}
