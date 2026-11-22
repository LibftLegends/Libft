#include "../../../Modules/Template/set.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    constexpr uint32_t SIZE = 8192U;
    constexpr uint32_t ITERATIONS = 12000U;
    ft_set<uint32_t> values(SIZE + 1U);
    if (values.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t value = 1U;
    while (value <= SIZE)
    {
        values.insert(value);
        ++value;
    }
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        values.insert(0U);
        values.remove(0U);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (values.size() != SIZE || values.find(1U) == nullptr || values.find(SIZE) == nullptr)
        return (1);
    std::printf("ordered_set_insert_remove_head ns/pair=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}
