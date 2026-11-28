#include "../../../Modules/RNG/rng.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    constexpr ft_size_t WORD_COUNT = 16383U;
    constexpr uint32_t ITERATIONS = 1800U;
    uint32_t words[WORD_COUNT];
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (rng_stream_seed_sequence(0x283469A9U, iteration, words, WORD_COUNT) != FT_ERR_SUCCESS)
            return (1);
        checksum += words[iteration % WORD_COUNT];
        checksum += words[WORD_COUNT - 1U];
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("stream split: %.3f ns per word, checksum %" FT_PRIu64 "\n",
        elapsed_ns / (ITERATIONS * WORD_COUNT), checksum);
    return (0);
}
