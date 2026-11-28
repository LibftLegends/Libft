#include "rng_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread_internal.hpp"
#include <atomic>
#include <new>
#include <random>
#include "../PThread/mutex.hpp"

std::mt19937 g_random_engine;
std::atomic<ft_bool> g_random_engine_seeded(FT_FALSE);

static pt_mutex *g_random_engine_mutex = ft_nullptr;

int32_t rng_enable_random_engine_thread_safety(void)
{
    pt_mutex *mutex;

    if (g_random_engine_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex = new (std::nothrow) pt_mutex();
    if (mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    int32_t initialize_error = mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex;
        return (initialize_error);
    }
    g_random_engine_mutex = mutex;
    return (FT_ERR_SUCCESS);
}

int32_t rng_disable_random_engine_thread_safety(void)
{
    if (g_random_engine_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    int32_t destroy_error = g_random_engine_mutex->destroy();
    delete g_random_engine_mutex;
    g_random_engine_mutex = ft_nullptr;
    return (destroy_error);
}

int32_t rng_lock_random_engine_mutex(void)
{
    pt_mutex *mutex = g_random_engine_mutex;

    return (pt_mutex_lock_if_not_null(mutex));
}

int32_t rng_unlock_random_engine_mutex(void)
{
    pt_mutex *mutex = g_random_engine_mutex;

    (void)pt_mutex_unlock_if_not_null(mutex);
    return (FT_ERR_SUCCESS);
}

static void rng_seed_random_engine_unlocked(uint32_t seed_value)
{
    g_random_engine.seed(static_cast<std::mt19937::result_type>(seed_value));
    g_random_engine_seeded.store(FT_TRUE, std::memory_order_release);
    return ;
}

void ft_seed_random_engine(uint32_t seed_value)
{
    if (rng_lock_random_engine_mutex() != FT_ERR_SUCCESS)
        return ;
    rng_seed_random_engine_unlocked(seed_value);
    (void)rng_unlock_random_engine_mutex();
    return ;
}

void ft_seed_random_engine_with_entropy(void)
{
    if (g_random_engine_seeded.load(std::memory_order_acquire) == FT_TRUE)
        return ;
    if (rng_lock_random_engine_mutex() != FT_ERR_SUCCESS)
        return ;
    try
    {
        if (g_random_engine_seeded.load(std::memory_order_relaxed) == FT_FALSE)
        {
            std::random_device random_device;
            rng_seed_random_engine_unlocked(random_device());
        }
    }
    catch (...)
    {
        (void)rng_unlock_random_engine_mutex();
        throw;
    }
    (void)rng_unlock_random_engine_mutex();
    return ;
}
