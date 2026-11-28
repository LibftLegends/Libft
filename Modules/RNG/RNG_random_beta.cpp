#include "rng.hpp"
#include "rng_internal.hpp"
#include "../Errno/errno.hpp"
#include <cmath>

float ft_random_beta(float alpha, float beta)
{
    ft_init_random_engine();
    if (!(alpha > 0.0f && beta > 0.0f) || !std::isfinite(alpha) || !std::isfinite(beta))
        return (0.0f);
    std::gamma_distribution<float> alpha_distribution(alpha, 1.0f);
    std::gamma_distribution<float> beta_distribution(beta, 1.0f);
    if (rng_lock_random_engine_mutex() != FT_ERR_SUCCESS)
        return (0.0f);
    const float alpha_sample = alpha_distribution(g_random_engine);
    const float beta_sample = beta_distribution(g_random_engine);
    (void)rng_unlock_random_engine_mutex();
    const float sum = alpha_sample + beta_sample;
    if (sum <= 0.0f)
        return (0.0f);
    float result = alpha_sample / sum;
    if (result < 0.0f)
        result = 0.0f;
    if (result > 1.0f)
        result = 1.0f;
    return (result);
}
