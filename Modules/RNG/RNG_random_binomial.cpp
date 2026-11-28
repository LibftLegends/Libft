#include <limits>

#include "rng.hpp"
#include "rng_internal.hpp"
#include "../Errno/errno.hpp"

int32_t ft_random_binomial(int32_t trial_count, double success_probability)
{
    ft_init_random_engine();
    if (trial_count < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (success_probability < 0.0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (trial_count == 0)
        return (0);
    if (success_probability > 1.0)
        return (FT_ERR_INVALID_ARGUMENT);
    constexpr double PROBABILITY_EPSILON = std::numeric_limits<double>::epsilon();
    if (success_probability <= PROBABILITY_EPSILON)
        return (0);
    if ((1.0 - success_probability) <= PROBABILITY_EPSILON)
        return (trial_count);
    if (trial_count == 1)
        return (static_cast<int32_t>(static_cast<double>(ft_random_float()) < success_probability));
    constexpr int32_t BATCH_CAPACITY = 128;
    float draws[BATCH_CAPACITY];
    int32_t remaining_trials = trial_count;
    int32_t success_count = 0;
    while (remaining_trials > 0)
    {
        int32_t batch_count = remaining_trials;
        if (batch_count > BATCH_CAPACITY)
            batch_count = BATCH_CAPACITY;
        const ft_bool sampled = (ft_random_float_vector(draws,
            static_cast<ft_size_t>(batch_count)) == FT_ERR_SUCCESS);
        int32_t draw_index = 0;
        while (draw_index < batch_count)
        {
            float draw;
            if (sampled != FT_FALSE)
                draw = draws[draw_index];
            else
                draw = ft_random_float();
            if (static_cast<double>(draw) < success_probability)
                success_count += 1;
            draw_index += 1;
        }
        remaining_trials -= batch_count;
    }
    return (success_count);
}
