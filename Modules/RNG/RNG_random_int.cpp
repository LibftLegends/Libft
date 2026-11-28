#include "rng.hpp"
#include "rng_internal.hpp"
#include "../Errno/errno.hpp"
#include <limits>
#include <random>

int32_t ft_random_int(void)
{
    ft_init_random_engine();
    if (rng_lock_random_engine_mutex() == FT_ERR_SUCCESS)
    {
        std::uniform_int_distribution<int32_t> distribution(0, std::numeric_limits<int32_t>::max());
        const int32_t random_value = distribution(g_random_engine);
        if (rng_unlock_random_engine_mutex() == FT_ERR_SUCCESS)
            return (random_value);
    }
    std::random_device random_device;
    return (static_cast<int32_t>(random_device()
        & static_cast<uint32_t>(std::numeric_limits<int32_t>::max())));
}
