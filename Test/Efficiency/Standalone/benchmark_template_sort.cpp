#include "../../../Modules/Template/algorithm.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

int main()
{
    constexpr uint32_t LENGTHS[] = {64U, 1024U, 8192U};
    constexpr uint32_t ROUNDS[] = {10000U, 1000U, 30U};
    std::mt19937 random(8191U);
    ft_size_t sample = 0U;
    while (sample < sizeof(LENGTHS) / sizeof(LENGTHS[0]))
    {
        const uint32_t length = LENGTHS[sample];
        const uint32_t rounds = ROUNDS[sample];
        std::vector<uint32_t> input(length);
        std::vector<uint32_t> values(length);
        uint32_t index = 0U;
        while (index < length)
        {
            input[index] = static_cast<uint32_t>(random());
            ++index;
        }
        using clock_type = std::chrono::steady_clock;
        clock_type::duration elapsed{};
        uint64_t comparisons = 0U;
        uint32_t iteration = 0U;
        while (iteration < rounds)
        {
            values = input;
            const auto start = clock_type::now();
            ft_sort(values.begin(), values.end(), [&comparisons](uint32_t left, uint32_t right)
            {
                ++comparisons;
                return (left < right);
            });
            elapsed += clock_type::now() - start;
            if (!std::is_sorted(values.begin(), values.end()))
                return (1);
            ++iteration;
        }
        std::printf("sort length=%u ns/op=%.2f comparisons/op=%.2f\n", length,
            std::chrono::duration<double, std::nano>(elapsed).count() / rounds,
            static_cast<double>(comparisons) / rounds);
        ++sample;
    }
    return (0);
}
