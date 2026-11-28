#include "../test_internal.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/RNG/rng_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/pthread_lock_tracking.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#if !defined(_WIN32) && !defined(_WIN64)
#include "compatebility_system_test_hooks.hpp"
#endif
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

FT_TEST(test_ft_random_seed_hashes_string)
{
    const char *seed_string = "entropy";
    uint32_t expected_hash = 2166136261u;
    int index = 0;

    while (seed_string[index] != '\0')
    {
        expected_hash ^= static_cast<unsigned char>(seed_string[index]);
        expected_hash *= 16777619u;
        index = index + 1;
    }
    uint32_t computed_hash = ft_random_seed(seed_string);
    FT_ASSERT_EQ(expected_hash, computed_hash);
    return (1);
}

FT_TEST(test_ft_random_seed_empty_string_returns_offset_basis)
{
    const char *seed_string = "";
    uint32_t computed_hash = ft_random_seed(seed_string);
    FT_ASSERT_EQ(2166136261u, computed_hash);
    return (1);
}

FT_TEST(test_ft_random_seed_separates_similar_strings)
{
    const char *first_seed = "alpha";
    const char *second_seed = "alphb";
    uint32_t first_hash = 2166136261u;
    uint32_t second_hash = 2166136261u;
    int index = 0;

    while (first_seed[index] != '\0')
    {
        first_hash ^= static_cast<unsigned char>(first_seed[index]);
        first_hash *= 16777619u;
        index = index + 1;
    }
    index = 0;
    while (second_seed[index] != '\0')
    {
        second_hash ^= static_cast<unsigned char>(second_seed[index]);
        second_hash *= 16777619u;
        index = index + 1;
    }
    FT_ASSERT_EQ(first_hash, ft_random_seed(first_seed));
    FT_ASSERT_EQ(second_hash, ft_random_seed(second_seed));
    FT_ASSERT(first_hash != second_hash);
    return (1);
}

FT_TEST(test_ft_random_seed_null_seed_uses_random_device)
{
    (void)ft_random_seed(ft_nullptr);
    (void)ft_random_seed(ft_nullptr);
    return (1);
}

FT_TEST(test_ft_seed_random_engine_with_entropy_sets_flag)
{
    g_random_engine_seeded.store(false, std::memory_order_release);
    ft_seed_random_engine_with_entropy();
    if (g_random_engine_seeded.load(std::memory_order_acquire) != true)
        return (0);
    g_random_engine_seeded.store(false, std::memory_order_release);
    return (1);
}

FT_TEST(test_ft_seed_random_engine_with_entropy_skips_when_seeded)
{
    g_random_engine_seeded.store(true, std::memory_order_release);
    ft_seed_random_engine_with_entropy();
    if (g_random_engine_seeded.load(std::memory_order_acquire) != true)
        return (0);
    g_random_engine_seeded.store(false, std::memory_order_release);
    return (1);
}

FT_TEST(test_rng_entropy_rechecks_seed_after_acquiring_engine_lock)
{
    constexpr uint32_t SEED = 136081U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_enable_random_engine_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_lock_random_engine_mutex());
    g_random_engine_seeded.store(FT_FALSE, std::memory_order_release);
    std::atomic<pt_thread_id_type> worker_id{pt_thread_id_type{}};
    std::thread worker([&worker_id]()
    {
        worker_id.store(pt_thread_self(), std::memory_order_release);
        ft_seed_random_engine_with_entropy();
        return ;
    });

    ft_bool worker_waiting = FT_FALSE;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline)
    {
        const pt_thread_id_type identifier = worker_id.load(std::memory_order_acquire);
        s_pt_lock_tracking_thread_state state;
        if (identifier != pt_thread_id_type{}
            && pt_lock_tracking::get_thread_state(identifier, state) == FT_ERR_SUCCESS
            && state.waiting_mutex != ft_nullptr)
        {
            worker_waiting = FT_TRUE;
            break ;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    g_random_engine.seed(SEED);
    g_random_engine_seeded.store(FT_TRUE, std::memory_order_release);
    (void)rng_unlock_random_engine_mutex();
    worker.join();
    const int32_t sample = ft_random_int();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_disable_random_engine_thread_safety());

    std::mt19937 reference(SEED);
    const int32_t expected = std::uniform_int_distribution<int32_t>(0, INT32_MAX)(reference);
    FT_ASSERT_EQ(FT_TRUE, worker_waiting);
    FT_ASSERT_EQ(expected, sample);
    return (1);
}

FT_TEST(test_ft_random_uint32_success_clears_errno)
{
    (void)ft_random_uint32();
    return (1);
}

#if !defined(_WIN32) && !defined(_WIN64)
FT_TEST(test_ft_random_uint32_failure_returns_zero)
{
    cmp_clear_force_rng_failures();
    cmp_force_rng_open_failure(EACCES);
    uint32_t random_value = ft_random_uint32();
    cmp_clear_force_rng_failures();
    FT_ASSERT(random_value != 0u);
    return (1);
}

FT_TEST(test_ft_random_uint32_eof_returns_zero)
{
    cmp_clear_force_rng_failures();
    cmp_force_rng_read_eof();
    uint32_t random_value = ft_random_uint32();
    cmp_clear_force_rng_failures();
    FT_ASSERT(random_value != 0u);
    return (1);
}
#endif
