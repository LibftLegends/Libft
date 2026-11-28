#include "../../../Modules/RNG/deck.hpp"
#include "../../../Modules/RNG/rng_internal.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <chrono>
#include <cstdio>

template<ft_size_t CardCount>
static int32_t measure_shuffle(uint32_t iterations)
{
    uint32_t cards[CardCount];
    ft_deck<uint32_t> deck;
    if (deck.initialize() != FT_ERR_SUCCESS || deck.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t index = 0U;
    while (index < CardCount)
    {
        cards[index] = static_cast<uint32_t>(index);
        if (deck.push_back(&cards[index]) != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    ft_seed_random_engine(91837U);
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (deck.shuffle() != FT_ERR_SUCCESS)
            return (1);
        checksum += *deck[CardCount - 1U];
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%" FT_PRIu64 " cards: %.2f ns per shuffle, checksum %" FT_PRIu64 "\n",
        static_cast<uint64_t>(CardCount), elapsed_ns / iterations, checksum);
    return (0);
}

int32_t main()
{
    if (rng_enable_random_engine_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    const int32_t small_result = measure_shuffle<52U>(7000U);
    const int32_t large_result = measure_shuffle<1024U>(1000U);
    if (rng_disable_random_engine_thread_safety() != FT_ERR_SUCCESS
        || small_result != 0 || large_result != 0)
        return (1);
    return (0);
}
