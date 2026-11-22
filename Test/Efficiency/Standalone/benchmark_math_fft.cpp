#include "../../../Modules/Math/math_fft.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>

int main()
{
    constexpr ft_size_t LENGTH = 65536U;
    constexpr uint32_t ITERATIONS = 12U;
    ft_vector<double> real;
    ft_vector<double> imaginary;
    ft_vector<double> result_real;
    ft_vector<double> result_imaginary;

    if (real.initialize() || imaginary.initialize() || result_real.initialize()
        || result_imaginary.initialize())
        return (1);
    real.reserve(LENGTH);
    imaginary.reserve(LENGTH);
    ft_size_t index = 0U;
    while (index < LENGTH)
    {
        double value = 0.0;
        if (index == 3U)
            value = 1.0;
        real.push_back(value);
        imaginary.push_back(0.0);
        ++index;
    }
    if (real.size() != LENGTH || imaginary.size() != LENGTH
        || math_fft(real, imaginary, result_real, result_imaginary))
        return (2);
    const auto started = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        if (math_fft(real, imaginary, result_real, result_imaginary))
            return (3);
        ++iteration;
    }
    const auto finished = std::chrono::steady_clock::now();
    if (!(std::fabs(result_real[0U] - 1.0) <= 1e-12)
        || !(std::fabs(result_imaginary[LENGTH / 4U] - 1.0) <= 1e-8))
        return (4);
    std::printf("fft_65536 ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(finished - started).count()
        / ITERATIONS);
    return (0);
}
