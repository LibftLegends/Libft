#include "../../../Modules/Template/pool.hpp"
#include "../../../Modules/Template/trie.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_pool_growth()
{
    constexpr uint32_t SIZE = 512U;
    constexpr uint32_t ITERATIONS = 16U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        Pool<uint32_t> pool;
        if (pool.initialize() != FT_ERR_SUCCESS)
            return (1);
        uint32_t size = 1U;
        while (size <= SIZE)
        {
            pool.resize(size);
            if (pool.get_error() != FT_ERR_SUCCESS)
                return (1);
            ++size;
        }
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("pool_resize_incremental ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / (SIZE * ITERATIONS));
    return (0);
}

static int32_t measure_trie()
{
    constexpr uint32_t SIZE = 255U;
    constexpr uint32_t BUILDS = 16U;
    constexpr uint32_t LOOKUPS = 300000U;
    uint32_t value = 15U;
    uint32_t iteration = 0U;
    std::chrono::steady_clock::duration lookup_time{};
    const auto start = std::chrono::steady_clock::now();
    while (iteration < BUILDS)
    {
        ft_trie<uint32_t> trie;
        if (trie.initialize() != FT_ERR_SUCCESS)
            return (1);
        uint32_t character = 1U;
        while (character <= SIZE)
        {
            const char key[] = {static_cast<char>((character * 73U) % 256U), 'i', 'd', '\0'};
            if (trie.insert(key, &value) != FT_ERR_SUCCESS)
                return (1);
            ++character;
        }
        if (iteration == BUILDS - 1U)
        {
            const auto lookup_start = std::chrono::steady_clock::now();
            uint32_t lookup = 0U;
            while (lookup < LOOKUPS)
            {
                const char key[] = {static_cast<char>(lookup % SIZE + 1U), 'i', 'd', '\0'};
                const auto *found = trie.search(key);
                if (found == ft_nullptr || found->_value_pointer != &value)
                    return (1);
                ++lookup;
            }
            lookup_time = std::chrono::steady_clock::now() - lookup_start;
        }
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start - lookup_time;
    std::printf("trie_build_and_destroy ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / (SIZE * BUILDS));
    std::printf("trie_lookup ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(lookup_time).count() / LOOKUPS);
    return (0);
}

int main()
{
    if (measure_pool_growth() != 0)
        return (1);
    return (measure_trie());
}
