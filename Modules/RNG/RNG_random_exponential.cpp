#include "rng.hpp"
#include "rng_internal.hpp"
#include "../Math/math.hpp"

float ft_random_exponential(float lambda_value)
{
    if (lambda_value <= 0.0f)
    {
        ft_init_random_engine();
        return (0.0f);
    }
    constexpr float MIN_UNIFORM = 0.0000000001f;
    float uniform_value = ft_random_float();
    if (uniform_value < MIN_UNIFORM)
        uniform_value = MIN_UNIFORM;
    const double log_value = math_log(uniform_value);
    return (static_cast<float>(-log_value) / lambda_value);
}
