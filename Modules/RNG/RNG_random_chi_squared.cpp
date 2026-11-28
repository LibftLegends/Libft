#include "rng.hpp"
#include "rng_internal.hpp"

float ft_random_chi_squared(float degrees_of_freedom)
{
    const float sample_value = rng_sample_gamma(degrees_of_freedom * 0.5f, 2.0f);
    if (sample_value < 0.0f)
        return (0.0f);
    return (sample_value);
}
