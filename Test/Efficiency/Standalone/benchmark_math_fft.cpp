#include "../../../Modules/Math/math_fft.hpp"
#include <chrono>
#include <cstdio>

static int32_t benchmark_fft(const char *label, ft_size_t length,
    uint32_t iterations, ft_bool thread_safe)
{
    ft_vector<double> input_real;
    ft_vector<double> input_imag;
    ft_vector<double> output_real;
    ft_vector<double> output_imag;
    ft_vector<double> *vectors[] = {&input_real, &input_imag, &output_real, &output_imag};
    ft_size_t vector_index = 0U;
    while (vector_index < sizeof(vectors) / sizeof(vectors[0]))
    {
        ft_vector<double> &vector = *vectors[vector_index];
        if (vector.initialize() != FT_ERR_SUCCESS)
            return (1);
        vector.resize(length);
        if (vector.get_error() != FT_ERR_SUCCESS)
            return (1);
        if (thread_safe && vector.enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
        vector_index += 1U;
    }

    ft_size_t sample = 0U;
    while (sample < length)
    {
        input_real[sample] = static_cast<double>(sample % 37U) * 0.125;
        input_imag[sample] = static_cast<double>(sample % 13U) * 0.25;
        sample += 1U;
    }
    double checksum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < iterations)
    {
        if (math_fft(input_real, input_imag, output_real, output_imag) != FT_ERR_SUCCESS)
            return (1);
        const ft_size_t bin = iteration % length;
        checksum += output_real[bin] + output_imag[bin];
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %.17g\n", label, elapsed / iterations, checksum);
    return (0);
}

int32_t main()
{
    if (benchmark_fft("FFT 16", 16U, 4096U, FT_FALSE) != 0
        || benchmark_fft("FFT 4096", 4096U, 64U, FT_FALSE) != 0
        || benchmark_fft("FFT 4096 thread-safe vectors", 4096U, 32U, FT_TRUE) != 0)
        return (1);
    return (0);
}
