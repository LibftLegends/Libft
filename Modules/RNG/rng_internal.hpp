#ifndef RNG_INTERNAL_HPP
# define RNG_INTERNAL_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <random>
#include "../PThread/mutex.hpp"

extern std::mt19937 g_random_engine;
extern std::atomic<ft_bool> g_random_engine_seeded;

void ft_seed_random_engine(uint32_t seed_value);
void ft_seed_random_engine_with_entropy(void);
int32_t rng_enable_random_engine_thread_safety(void);
int32_t rng_disable_random_engine_thread_safety(void);
int32_t rng_lock_random_engine_mutex(void);
int32_t rng_unlock_random_engine_mutex(void);
int32_t rng_poisson_rate_error(double lambda_value);
int32_t rng_dice_roll_checked(int32_t number, int32_t faces, int32_t &value);

// Above this rate the legacy exponential loses precision through cancellation.
inline constexpr double RNG_POISSON_PRODUCT_MAX_RATE = 16.0;

inline __attribute__((always_inline)) void ft_init_random_engine(void)
{
    if (g_random_engine_seeded.load(std::memory_order_acquire) == FT_FALSE)
        ft_seed_random_engine_with_entropy();
    return ;
}

inline float rng_sample_gamma(float shape, float scale)
{
    ft_init_random_engine();
    if (!(shape > 0.0f && scale > 0.0f) || !std::isfinite(shape) || !std::isfinite(scale))
        return (0.0f);
    std::gamma_distribution<float> distribution(shape, scale);
    if (rng_lock_random_engine_mutex() != FT_ERR_SUCCESS)
        return (0.0f);
    const float sample = distribution(g_random_engine);
    (void)rng_unlock_random_engine_mutex();
    return (sample);
}

template<typename StopAfterDraw>
int32_t rng_count_float_draws_until(const StopAfterDraw &should_stop)
{
    constexpr int32_t BATCH_CAPACITY = 64;
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    int32_t draw_count = 0;
    while (draw_count < INT32_MAX)
    {
        if (rng_lock_random_engine_mutex() != FT_ERR_SUCCESS)
        {
            draw_count += 1;
            if (should_stop(0.0f))
                return (draw_count);
            continue ;
        }
        int32_t batch_end = INT32_MAX;
        if (draw_count <= INT32_MAX - BATCH_CAPACITY)
            batch_end = draw_count + BATCH_CAPACITY;
        while (draw_count < batch_end)
        {
            draw_count += 1;
            if (should_stop(distribution(g_random_engine)))
            {
                (void)rng_unlock_random_engine_mutex();
                return (draw_count);
            }
        }
        (void)rng_unlock_random_engine_mutex();
    }
    return (0);
}

#endif
