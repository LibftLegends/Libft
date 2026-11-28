#include "rng.hpp"
#include "rng_internal.hpp"
#include "../Errno/errno.hpp"
#include <random>

int32_t ft_random_int_vector(int32_t minimum_value, int32_t maximum_value, int32_t *output_values, ft_size_t output_count)
{
    ft_size_t output_index;
    int32_t lock_error;
    int32_t unlock_error;

    if (output_count > 0 && output_values == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (minimum_value > maximum_value)
        return (FT_ERR_INVALID_ARGUMENT);
    if (output_count == 0)
        return (FT_ERR_SUCCESS);
    ft_init_random_engine();
    std::uniform_int_distribution<int32_t> distribution(minimum_value, maximum_value);
    lock_error = rng_lock_random_engine_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    output_index = 0;
    while (output_index < output_count)
    {
        output_values[output_index] = distribution(g_random_engine);
        output_index += 1;
    }
    unlock_error = rng_unlock_random_engine_mutex();
    if (unlock_error != FT_ERR_SUCCESS)
        return (unlock_error);
    return (FT_ERR_SUCCESS);
}

int32_t ft_random_float_vector(float *output_values, ft_size_t output_count)
{
    ft_size_t output_index;
    int32_t lock_error;
    int32_t unlock_error;

    if (output_count > 0 && output_values == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (output_count == 0)
        return (FT_ERR_SUCCESS);
    ft_init_random_engine();
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    lock_error = rng_lock_random_engine_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    output_index = 0;
    while (output_index < output_count)
    {
        output_values[output_index] = distribution(g_random_engine);
        output_index += 1;
    }
    unlock_error = rng_unlock_random_engine_mutex();
    if (unlock_error != FT_ERR_SUCCESS)
        return (unlock_error);
    return (FT_ERR_SUCCESS);
}
