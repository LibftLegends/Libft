#include "rng.hpp"
#include "rng_internal.hpp"
#include "../Errno/errno.hpp"
#include "../Math/math.hpp"
#include <cmath>

int32_t rng_poisson_rate_error(double lambda_value)
{
    if (!(lambda_value > 0.0) || !std::isfinite(lambda_value))
        return (FT_ERR_INVALID_ARGUMENT);
    if (lambda_value > static_cast<double>(INT32_MAX))
        return (FT_ERR_OUT_OF_RANGE);
    return (FT_ERR_SUCCESS);
}

int32_t ft_random_poisson(double lambda_value)
{
    ft_init_random_engine();
    const int32_t rate_error = rng_poisson_rate_error(lambda_value);
    if (rate_error != FT_ERR_SUCCESS)
        return (rate_error);
    if (lambda_value > RNG_POISSON_PRODUCT_MAX_RATE)
    {
        std::poisson_distribution<int64_t> distribution(lambda_value);
        const int32_t lock_error = rng_lock_random_engine_mutex();
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        const int64_t sample = distribution(g_random_engine);
        (void)rng_unlock_random_engine_mutex();
        if (sample > INT32_MAX)
            return (FT_ERR_OUT_OF_RANGE);
        return (static_cast<int32_t>(sample));
    }
    const double limit_value = math_exp(-lambda_value);
    if (limit_value >= 1.0)
        return (0);
    double product_value = 1.0;
    const int32_t draw_count = rng_count_float_draws_until([limit_value, &product_value](float draw)
    {
        product_value *= static_cast<double>(draw);
        return (product_value <= limit_value);
    });
    if (draw_count == 0)
        return (FT_ERR_OUT_OF_RANGE);
    return (draw_count - 1);
}
