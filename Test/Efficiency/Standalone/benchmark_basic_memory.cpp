#include "../../../Modules/Basic/basic.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

using Clock = std::chrono::steady_clock;
static volatile uintptr_t observed_result;
static void print_average(const char *name, uint32_t length, uint32_t iterations,
    Clock::time_point started)
{
    const double elapsed = std::chrono::duration<double, std::nano>(
        Clock::now() - started).count();
    std::printf("%s length=%u ns/op=%.2f\n", name, length,
        elapsed / static_cast<double>(iterations));
    return ;
}

static int32_t benchmark_copy_and_search()
{
    constexpr uint32_t CAPACITY = 65536U;
    uint8_t source[CAPACITY];
    uint8_t destination[CAPACITY];
    std::memset(source, 'a', sizeof(source));
    uint32_t iteration = 0U;
    auto started = Clock::now();
    while (iteration < 2000000U)
    {
        const uint32_t offset = iteration & 63U;
        ft_memcpy(destination + offset, source + offset, 8U);
        ++iteration;
    }
    print_average("memcpy", 8U, iteration, started);
    iteration = 0U;
    started = Clock::now();
    while (iteration < 10000U)
    {
        ft_memcpy(destination, source, CAPACITY);
        ++iteration;
    }
    print_average("memcpy", CAPACITY, iteration, started);
    if (destination[CAPACITY - 1U] != 'a')
        return (1);
    const char late_needle[] = "aaaaab";
    iteration = 0U;
    started = Clock::now();
    while (iteration < 1000U)
    {
        const void *expected = ft_nullptr;
        source[CAPACITY - 1U] = 'a';
        if ((iteration & 1U) != 0U)
        {
            source[CAPACITY - 1U] = 'b';
            expected = source + CAPACITY - (sizeof(late_needle) - 1U);
        }
        if (ft_memmem(source, CAPACITY, late_needle, sizeof(late_needle) - 1U)
            != expected)
            return (1);
        ++iteration;
    }
    print_average("memmem_alternating_late_match", CAPACITY, iteration, started);
    iteration = 0U;
    started = Clock::now();
    while (iteration < 1000U)
    {
        if (ft_memmem(source, CAPACITY, "ba", 2U) != ft_nullptr)
            return (1);
        ++iteration;
    }
    print_average("memmem_no_match", CAPACITY, iteration, started);
    return (0);
}

int main()
{
    constexpr uint32_t LENGTHS[] = {8U, 64U, 4096U, 65536U};
    constexpr uint32_t ITERATIONS[] = {2000000U, 2000000U, 100000U, 15000U};
    const char *const NAMES[] = {"memchr_late", "memmove_backward",
        "memmove_forward", "memcmp_equal", "memcmp_late"};
    std::vector<uint8_t> buffer(65568U, 0U);
    std::vector<uint8_t> compare(65568U, 0U);
    uintptr_t observed = 0U;
    uint32_t sample = 0U;

    while (sample < sizeof(LENGTHS) / sizeof(LENGTHS[0]))
    {
        const uint32_t length = LENGTHS[sample];
        const uint32_t loops = ITERATIONS[sample];
        uint32_t pattern = 0U;

        while (pattern < sizeof(NAMES) / sizeof(NAMES[0]))
        {
            uint32_t iteration = 0U;
            std::fill(buffer.begin(), buffer.end(), 0U);
            std::fill(compare.begin(), compare.end(), 0U);
            buffer[length - 1U] = 0xFEU;
            compare[length - 1U] = 0xABU;
            if (pattern == 3U)
                compare[length - 1U] = 0xFEU;
            const auto start = Clock::now();
            while (iteration < loops)
            {
                if (pattern == 0U)
                    observed ^= reinterpret_cast<uintptr_t>(ft_memchr(buffer.data(), 0x1FE, length));
                else if (pattern == 1U)
                    observed ^= reinterpret_cast<uintptr_t>(ft_memmove(buffer.data() + 11, buffer.data(), length));
                else if (pattern == 2U)
                    observed ^= reinterpret_cast<uintptr_t>(ft_memmove(buffer.data(), buffer.data() + 11, length));
                else
                    observed ^= static_cast<uintptr_t>(ft_memcmp(buffer.data(), compare.data(), length));
                ++iteration;
            }
            const auto end = Clock::now();
            std::printf("%s length=%u ns/op=%.2f\n", NAMES[pattern], length,
                std::chrono::duration<double, std::nano>(end - start).count()
                / static_cast<double>(loops));
            ++pattern;
        }
        ++sample;
    }
    observed_result = observed;
    return (benchmark_copy_and_search());
}
