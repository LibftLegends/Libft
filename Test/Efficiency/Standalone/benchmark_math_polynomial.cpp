#include "../../../Modules/Math/ft_cubic_spline.hpp"
#include <chrono>
#include <cstdio>

template <typename Operation>
static int32_t benchmark_operation(const char *label, uint32_t iterations,
    Operation operation)
{
    double checksum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0U;
    while (index < iterations)
    {
        double value;
        if (!operation(index, value))
            return (1);
        checksum += value;
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %.17g\n", label, elapsed / iterations, checksum);
    return (0);
}

int main()
{
    constexpr ft_size_t COUNT = 1024U;
    ft_vector<double> knots;
    ft_vector<double> samples;
    ft_vector<double> coefficients;
    if (knots.initialize() != FT_ERR_SUCCESS || samples.initialize() != FT_ERR_SUCCESS
        || coefficients.initialize() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t index = 0U;
    while (index < COUNT)
    {
        const double knot = static_cast<double>(index);
        if (knots.push_back(knot) != FT_ERR_SUCCESS
            || samples.push_back(knot * 3.0 - 7.0) != FT_ERR_SUCCESS
            || coefficients.push_back(static_cast<double>(index % 8U) / 64.0) != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    const int32_t build_status = benchmark_operation("build 1024-knot spline", 32U,
        [&knots, &samples](uint32_t, double &value)
    {
        ft_cubic_spline *spline = ft_cubic_spline_build(knots, samples);
        if (spline == ft_nullptr)
            return (FT_FALSE);
        value = ft_cubic_spline_evaluate(*spline, 12.5);
        delete spline;
        return (FT_TRUE);
    });
    if (build_status != 0)
        return (build_status);
    ft_cubic_spline *spline = ft_cubic_spline_build(knots, samples);
    if (spline == ft_nullptr)
        return (1);
    const int32_t spline_status = benchmark_operation("evaluate 1024-knot spline", 16384U,
        [spline](uint32_t sample, double &value)
    {
        const double input = static_cast<double>((sample * 129U) % 1023U) + 0.5;
        value = ft_cubic_spline_evaluate(*spline, input);
        return (static_cast<ft_bool>(value == input * 3.0 - 7.0));
    });
    delete spline;
    if (spline_status != 0)
        return (spline_status);
    return (benchmark_operation("evaluate 1024 coefficients", 4096U,
        [&coefficients](uint32_t sample, double &value)
    {
        const double input = 0.875 + static_cast<double>(sample % 11U) / 128.0;
        return (static_cast<ft_bool>(math_polynomial_evaluate(coefficients, input, value)
            == FT_ERR_SUCCESS));
    }));
}
