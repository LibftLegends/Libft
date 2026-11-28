#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Math/ft_dual_number.hpp"
#include <chrono>
#include <cstdio>

static ft_dual_number *benchmark_function(const ft_vector<ft_dual_number> &point,
    void *user_data)
{
    (void)user_data;
    return ((point[0] * point[0]) + point[1]);
}

static int32_t benchmark_gradient(const char *label, ft_size_t dimension, uint32_t iterations)
{
    ft_vector<double> point;
    ft_vector<double> gradient;
    if (point.initialize() != FT_ERR_SUCCESS || gradient.initialize() != FT_ERR_SUCCESS)
        return (1);
    point.resize(dimension, 3.0);
    if (point.get_error() != FT_ERR_SUCCESS)
        return (1);
    if (math_autodiff_gradient(benchmark_function, point, gradient, ft_nullptr,
            ft_nullptr) != FT_ERR_SUCCESS)
        return (1);

    ft_size_t before;
    ft_size_t frees;
    if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
        return (1);
    double checksum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < iterations)
    {
        double value;
        if (math_autodiff_gradient(benchmark_function, point, gradient, &value,
                ft_nullptr) != FT_ERR_SUCCESS)
            return (1);
        checksum += value + gradient[0] + gradient[1] + gradient[dimension - 1U];
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    ft_size_t after;
    if (cma_get_stats(&after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%s: %.2f ns/op, checksum %.17g, allocations %.2f\n",
        label, elapsed / iterations, checksum, static_cast<double>(after - before) / iterations);
    return (0);
}

int32_t main()
{
    if (benchmark_gradient("gradient 4 variables", 4U, 4096U) != 0
        || benchmark_gradient("gradient 64 variables", 64U, 128U) != 0)
        return (1);
    return (0);
}
