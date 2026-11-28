#include "../test_internal.hpp"
#include "../../Modules/Math/math_fft.hpp"
#include "../../Modules/CMA/CMA.hpp"
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

FT_TEST(test_math_fft_supports_crossed_and_in_place_outputs)
{
    ft_vector<double> real;
    ft_vector<double> imaginary;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, real.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, imaginary.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, real.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, imaginary.enable_thread_safety());
    const double real_samples[] = {1.0, 2.0};
    const double imaginary_samples[] = {3.0, 4.0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, real.append(real_samples, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, imaginary.append(imaginary_samples, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_fft(real, imaginary, imaginary, real));
    FT_ASSERT(std::fabs(imaginary[0] - 3.0) < 1e-12);
    FT_ASSERT(std::fabs(imaginary[1] + 1.0) < 1e-12);
    FT_ASSERT(std::fabs(real[0] - 7.0) < 1e-12);
    FT_ASSERT(std::fabs(real[1] + 1.0) < 1e-12);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_ifft(imaginary, real, imaginary, real));
    FT_ASSERT(std::fabs(imaginary[1] - 2.0) < 1e-12);
    FT_ASSERT(std::fabs(real[1] - 4.0) < 1e-12);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, math_fft(real, imaginary, real, real));
    FT_ASSERT_EQ(FT_TRUE, real.is_thread_safe());
    FT_ASSERT_EQ(FT_TRUE, imaginary.is_thread_safe());
    return (1);
}

FT_TEST(test_math_fft_allocation_failure_keeps_output_samples)
{
    ft_vector<double> real;
    ft_vector<double> imaginary;
    ft_vector<double> output_real;
    ft_vector<double> output_imaginary;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, real.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, imaginary.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output_real.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output_imaginary.initialize());
    real.resize(64U, 1.0);
    imaginary.resize(64U);
    output_real.reserve(64U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output_real.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output_real.push_back(11.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output_imaginary.push_back(12.0));

    cma_set_alloc_limit(1U);
    const int32_t error = math_fft(real, imaginary, output_real, output_imaginary);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), output_real.size());
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), output_imaginary.size());
    FT_ASSERT_EQ(11.0, output_real[0]);
    FT_ASSERT_EQ(12.0, output_imaginary[0]);
    return (1);
}

FT_TEST(test_math_convolution_matches_coefficients_and_supports_alias)
{
    ft_vector<double> first;
    ft_vector<double> second;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize());
    const double first_samples[] = {1.0, 2.0, 3.0};
    const double second_samples[] = {-2.0, 4.0};
    const double expected[] = {-2.0, 0.0, 2.0, 12.0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.append(first_samples, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.append(second_samples, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, math_convolution(first, second, first));
    FT_ASSERT_EQ(static_cast<ft_size_t>(4), first.size());
    ft_size_t index = 0U;
    while (index < 4U)
    {
        FT_ASSERT(std::fabs(first[index] - expected[index]) < 1e-12);
        index += 1U;
    }
    FT_ASSERT_EQ(0.0, first[1]);
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), second.size());
    return (1);
}

FT_TEST(test_math_convolution_allocation_failure_keeps_result)
{
    ft_vector<double> first;
    ft_vector<double> second;
    ft_vector<double> result;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result.initialize());
    first.resize(32U, 1.0);
    second.resize(32U, 2.0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result.push_back(17.0));

    cma_set_alloc_limit(1U);
    const int32_t error = math_convolution(first, second, result);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), result.size());
    FT_ASSERT_EQ(17.0, result[0]);
    return (1);
}
