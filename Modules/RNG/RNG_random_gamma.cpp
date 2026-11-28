#include "rng.hpp"
#include "rng_internal.hpp"

float ft_random_gamma(float shape, float scale)
{
    return (rng_sample_gamma(shape, scale));
}
