#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Math/ft_cubic_spline.hpp"
#include <chrono>
#include <cstdio>

static int32_t benchmark_lagrange()
{
    constexpr uint32_t ITERATIONS = 4096U;
    ft_vector<double> nodes;
    ft_vector<double> values;
    if (nodes.initialize() != FT_ERR_SUCCESS || values.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < 32U)
    {
        const double node = static_cast<double>(index);
        if (nodes.push_back(node) != FT_ERR_SUCCESS || values.push_back(node * node) != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    double checksum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    index = 0U;
    while (index < ITERATIONS)
    {
        double value;
        const double input = 12.01 + static_cast<double>(index % 127U) / 128.0;
        if (math_polynomial_lagrange_interpolate(nodes, values, input, value) != FT_ERR_SUCCESS)
            return (1);
        checksum += value;
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("Lagrange 32 points: %.2f ns/op, checksum %.17g, allocations 0\n",
        elapsed / ITERATIONS, checksum);
    return (0);
}

static int32_t benchmark_spline_move()
{
    constexpr uint32_t ITERATIONS = 2048U;
    constexpr ft_size_t KNOT_COUNT = 1024U;
    ft_vector<double> knots;
    ft_vector<double> values;
    if (knots.initialize() != FT_ERR_SUCCESS || values.initialize() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t index = 0U;
    while (index < KNOT_COUNT)
    {
        const double sample = static_cast<double>(index);
        if (knots.push_back(sample) != FT_ERR_SUCCESS || values.push_back(sample) != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    ft_cubic_spline *original = ft_cubic_spline_build(knots, values);
    if (original == ft_nullptr)
        return (1);
    ft_cubic_spline other;
    ft_cubic_spline *source = original;
    ft_cubic_spline *destination = &other;
    ft_size_t allocations_before;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
    {
        delete original;
        return (1);
    }
    double checksum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    index = 0U;
    while (index < ITERATIONS)
    {
        if (destination->move(*source) != FT_ERR_SUCCESS)
        {
            delete original;
            return (1);
        }
        checksum += ft_cubic_spline_evaluate(*destination, 12.5);
        ft_cubic_spline *previous_source = source;
        source = destination;
        destination = previous_source;
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    ft_size_t allocations_after;
    const int32_t statistics_error = cma_get_stats(&allocations_after, &frees);
    delete original;
    if (statistics_error != FT_ERR_SUCCESS)
        return (1);
    std::printf("Spline move and sample: %.2f ns/op, checksum %.17g, allocations %.2f\n",
        elapsed / ITERATIONS, checksum,
        static_cast<double>(allocations_after - allocations_before) / ITERATIONS);
    return (0);
}

int main()
{
    if (benchmark_lagrange() != 0 || benchmark_spline_move() != 0)
        return (1);
    return (0);
}
