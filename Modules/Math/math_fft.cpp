#include "math_fft.hpp"
#include "../Basic/basic.hpp"
#include <algorithm>
#include <cmath>
#include <functional>

static constexpr double FFT_TWO_PI = 6.283185307179586476925286766559;
static constexpr double FFT_CONVOLUTION_ZERO_THRESHOLD = 1e-12;

template <ft_size_t Count, typename Operation>
static int32_t math_fft_with_locked_vectors(const ft_vector<double> *(&vectors)[Count],
    Operation operation) noexcept
{
    std::sort(vectors, vectors + Count, std::less<const ft_vector<double> *>{});
    const auto *end = std::unique(vectors, vectors + Count);
    ft_bool acquired[Count];
    ft_size_t index = 0U;
    int32_t error = FT_ERR_SUCCESS;
    while (vectors + index != end)
    {
        error = vectors[index]->lock(&acquired[index]);
        if (error != FT_ERR_SUCCESS)
            break ;
        index += 1U;
    }
    if (error == FT_ERR_SUCCESS)
        error = operation();
    while (index != 0U)
    {
        index -= 1U;
        vectors[index]->unlock(acquired[index]);
    }
    return (error);
}

static void math_fft_bit_reverse(double *real, double *imag, ft_size_t length) noexcept
{
    ft_size_t reversed = 0U;
    ft_size_t index = 1U;
    while (index < length)
    {
        // Advance the reversed index by carrying from its highest bit.
        ft_size_t bit = length >> 1U;
        while ((reversed & bit) != 0U)
        {
            reversed ^= bit;
            bit >>= 1U;
        }
        reversed ^= bit;
        if (index < reversed)
        {
            std::swap(real[index], real[reversed]);
            std::swap(imag[index], imag[reversed]);
        }
        index += 1U;
    }
    return ;
}

static void math_fft_transform(double *real, double *imag,
    ft_size_t length, ft_bool inverse) noexcept
{
    double angle_numerator = -FFT_TWO_PI;
    if (inverse)
        angle_numerator = FFT_TWO_PI;

    math_fft_bit_reverse(real, imag, length);
    ft_size_t half_size = 1U;
    while (half_size < length)
    {
        const ft_size_t step = half_size << 1U;
        const double angle = angle_numerator / static_cast<double>(step);
        const double phase_step_real = std::cos(angle);
        const double phase_step_imag = std::sin(angle);
        ft_size_t block_start = 0U;
        while (block_start < length)
        {
            double phase_real = 1.0;
            double phase_imag = 0.0;
            ft_size_t first = block_start;
            const ft_size_t block_middle = block_start + half_size;
            while (first < block_middle)
            {
                const ft_size_t second = first + half_size;
                const double second_real = real[second];
                const double second_imag = imag[second];
                const double product_real = phase_real * second_real - phase_imag * second_imag;
                const double product_imag = phase_real * second_imag + phase_imag * second_real;
                const double first_real = real[first];
                const double first_imag = imag[first];
                real[second] = first_real - product_real;
                imag[second] = first_imag - product_imag;
                real[first] = first_real + product_real;
                imag[first] = first_imag + product_imag;
                const double next_phase_real = phase_real * phase_step_real
                    - phase_imag * phase_step_imag;
                phase_imag = phase_real * phase_step_imag + phase_imag * phase_step_real;
                phase_real = next_phase_real;
                first += 1U;
            }
            block_start += step;
        }
        half_size = step;
    }
    if (inverse)
    {
        const double scale = 1.0 / static_cast<double>(length);
        ft_size_t index = 0U;
        while (index < length)
        {
            real[index] *= scale;
            imag[index] *= scale;
            index += 1U;
        }
    }
    return ;
}

static void math_fft_copy(double *destination, const double *source, ft_size_t length) noexcept
{
    if (destination != source)
        ft_memcpy(destination, source, length * sizeof(double));
    return ;
}

static void math_fft_copy_components(double *real, double *imag,
    const double *source_real, const double *source_imag, ft_size_t length) noexcept
{
    if (source_real == imag && source_imag == real)
    {
        ft_size_t index = 0U;
        while (index < length)
        {
            std::swap(real[index], imag[index]);
            index += 1U;
        }
    }
    else if (source_imag == real)
    {
        math_fft_copy(imag, source_imag, length);
        math_fft_copy(real, source_real, length);
    }
    else
    {
        math_fft_copy(real, source_real, length);
        math_fft_copy(imag, source_imag, length);
    }
    return ;
}

static int32_t math_fft_dispatch(const ft_vector<double> &input_real,
    const ft_vector<double> &input_imag, ft_vector<double> &output_real,
    ft_vector<double> &output_imag, ft_bool inverse) noexcept
{
    if (&output_real == &output_imag)
        return (FT_ERR_INVALID_ARGUMENT);

    const ft_vector<double> *vectors[] = {&input_real, &input_imag, &output_real, &output_imag};
    return (math_fft_with_locked_vectors(vectors, [&]() -> int32_t
    {
        const ft_size_t length = input_real.size();
        if (length == 0U || (length & (length - 1U)) != 0U
            || input_imag.size() != length)
            return (FT_ERR_INVALID_ARGUMENT);
        output_real.reserve(length);
        if (output_real.get_error() != FT_ERR_SUCCESS)
            return (output_real.get_error());
        output_imag.reserve(length);
        if (output_imag.get_error() != FT_ERR_SUCCESS)
            return (output_imag.get_error());
        output_real.resize(length);
        if (output_real.get_error() != FT_ERR_SUCCESS)
            return (output_real.get_error());
        output_imag.resize(length);
        if (output_imag.get_error() != FT_ERR_SUCCESS)
            return (output_imag.get_error());

        double *real = output_real.begin();
        double *imag = output_imag.begin();
        math_fft_copy_components(real, imag, input_real.begin(), input_imag.begin(), length);
        math_fft_transform(real, imag, length, inverse);
        return (FT_ERR_SUCCESS);
    }));
}

int32_t math_fft(const ft_vector<double> &input_real,
    const ft_vector<double> &input_imag, ft_vector<double> &output_real,
    ft_vector<double> &output_imag) noexcept
{
    return (math_fft_dispatch(input_real, input_imag, output_real, output_imag, FT_FALSE));
}

int32_t math_ifft(const ft_vector<double> &input_real,
    const ft_vector<double> &input_imag, ft_vector<double> &output_real,
    ft_vector<double> &output_imag) noexcept
{
    return (math_fft_dispatch(input_real, input_imag, output_real, output_imag, FT_TRUE));
}

static void math_fft_multiply(double *first_real, double *first_imag,
    const double *second_real, const double *second_imag, ft_size_t length) noexcept
{
    ft_size_t index = 0U;
    while (index < length)
    {
        const double real = first_real[index];
        const double imag = first_imag[index];
        first_real[index] = real * second_real[index] - imag * second_imag[index];
        first_imag[index] = real * second_imag[index] + imag * second_real[index];
        index += 1U;
    }
    return ;
}

int32_t math_convolution(const ft_vector<double> &first,
    const ft_vector<double> &second, ft_vector<double> &result) noexcept
{
    const ft_vector<double> *vectors[] = {&first, &second, &result};
    return (math_fft_with_locked_vectors(vectors, [&]() -> int32_t
    {
        constexpr ft_size_t MAX_FFT_LENGTH = FT_SYSTEM_SIZE_MAX / sizeof(double) / 4U;
        const ft_size_t first_length = first.size();
        const ft_size_t second_length = second.size();
        if (first_length == 0U || second_length == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        if (first_length > MAX_FFT_LENGTH || second_length - 1U > MAX_FFT_LENGTH - first_length)
            return (FT_ERR_NO_MEMORY);

        const ft_size_t result_length = first_length + second_length - 1U;
        ft_size_t fft_length = 1U;
        while (fft_length < result_length)
        {
            if (fft_length > MAX_FFT_LENGTH / 2U)
                return (FT_ERR_NO_MEMORY);
            fft_length <<= 1U;
        }

        ft_vector<double> workspace;
        const int32_t initialization_error = workspace.initialize();
        if (initialization_error != FT_ERR_SUCCESS)
            return (initialization_error);
        workspace.resize(fft_length * 4U);
        if (workspace.get_error() != FT_ERR_SUCCESS)
            return (workspace.get_error());

        double *first_real = workspace.begin();
        double *first_imag = first_real + fft_length;
        double *second_real = first_imag + fft_length;
        double *second_imag = second_real + fft_length;
        math_fft_copy(first_real, first.begin(), first_length);
        math_fft_copy(second_real, second.begin(), second_length);
        math_fft_transform(first_real, first_imag, fft_length, FT_FALSE);
        math_fft_transform(second_real, second_imag, fft_length, FT_FALSE);
        math_fft_multiply(first_real, first_imag, second_real, second_imag, fft_length);
        math_fft_transform(first_real, first_imag, fft_length, FT_TRUE);

        result.resize(result_length);
        if (result.get_error() != FT_ERR_SUCCESS)
            return (result.get_error());
        double *output = result.begin();
        ft_size_t index = 0U;
        while (index < result_length)
        {
            double value = first_real[index];
            if (std::fabs(value) < FFT_CONVOLUTION_ZERO_THRESHOLD)
                value = 0.0;
            output[index] = value;
            index += 1U;
        }
        return (FT_ERR_SUCCESS);
    }));
}
