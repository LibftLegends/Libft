#include <limits>
#include "rng.hpp"
#include "rng_internal.hpp"
#include "../Errno/errno.hpp"

int32_t ft_random_geometric(double success_probability)
{
    ft_init_random_engine();
    if (!(success_probability > 0.0 && success_probability <= 1.0))
        return (FT_ERR_INVALID_ARGUMENT);
    constexpr double PROBABILITY_EPSILON = std::numeric_limits<double>::epsilon();
    if ((1.0 - success_probability) <= PROBABILITY_EPSILON)
        return (1);
    const int32_t trial_count = rng_count_float_draws_until([success_probability](float draw)
    {
        return (static_cast<double>(draw) < success_probability);
    });
    if (trial_count == 0)
        return (FT_ERR_OUT_OF_RANGE);
    return (trial_count);
}
