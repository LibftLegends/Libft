#include "../test_internal.hpp"
#include "../../Modules/Math/math_fft.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cmath>

FT_TEST(test_math_fft_matches_shifted_impulse_spectrum)
{
    const ft_size_t length = 16U;
    const double pi = 3.14159265358979323846;
    const double tolerance = 1e-12;
    ft_vector<double> input_real;
    ft_vector<double> input_imag;
    ft_vector<double> output_real;
    ft_vector<double> output_imag;
    ft_vector<double> reconstructed_real;
    ft_vector<double> reconstructed_imag;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, input_real.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input_imag.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output_real.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output_imag.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reconstructed_real.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reconstructed_imag.initialize());
    ft_size_t index = 0U;
    while (index < length)
    {
        double sample = 0.0;

        if (index == 3U)
            sample = 1.0;
        input_real.push_back(sample);
        input_imag.push_back(0.0);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_fft(input_real, input_imag,
        output_real, output_imag));
    FT_ASSERT_EQ(length, output_real.size());
    FT_ASSERT_EQ(length, output_imag.size());
    index = 0U;
    while (index < length)
    {
        const double angle = -6.0 * pi * static_cast<double>(index)
            / static_cast<double>(length);
        FT_ASSERT(std::fabs(output_real[index] - std::cos(angle))
            < tolerance);
        FT_ASSERT(std::fabs(output_imag[index] - std::sin(angle))
            < tolerance);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_ifft(output_real, output_imag,
        reconstructed_real, reconstructed_imag));
    FT_ASSERT_EQ(length, reconstructed_real.size());
    FT_ASSERT_EQ(length, reconstructed_imag.size());
    index = 0U;
    while (index < length)
    {
        FT_ASSERT(std::fabs(reconstructed_real[index] - input_real[index])
            < tolerance);
        FT_ASSERT(std::fabs(reconstructed_imag[index]) < tolerance);
        index += 1U;
    }
    return (1);
}
