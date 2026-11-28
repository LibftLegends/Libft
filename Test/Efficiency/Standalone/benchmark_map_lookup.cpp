#include "../../../Modules/Template/map.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_lookup(uint32_t size)
{
    constexpr uint32_t ITERATIONS = 100000U;
    ft_map<uint32_t, uint32_t> values(size);
    if (values.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t key = 0U;
    while (key < size)
    {
        values.insert(key, key);
        ++key;
    }
    uint32_t iteration = 0U;
    uint32_t hits = 0U;
    const auto *end = values.end();
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        key = (iteration * 173U) % (size * 2U);
        if (values.find(key) != end)
            ++hits;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (hits == 0U || hits == ITERATIONS)
        return (1);
    std::printf("map_lookup size=%u ns/op=%.2f\n", size,
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}

int main()
{
    if (measure_lookup(8U) != 0)
        return (1);
    return (measure_lookup(4096U));
}
