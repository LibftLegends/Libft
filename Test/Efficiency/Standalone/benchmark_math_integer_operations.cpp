#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/Math/math.hpp"
#include <chrono>
#include <cstdio>

template <typename Operation>
static void benchmark_operation(const char *label, Operation operation)
{
    constexpr uint32_t COUNT = 1048576U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0U;
    while (index < COUNT)
    {
        checksum += static_cast<uint64_t>(operation(index));
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %" FT_PRIu64 "\n",
        label, elapsed / COUNT, checksum);
    return ;
}

template <typename Integer>
static void benchmark_absdiff(const char *label, uint64_t mask)
{
    constexpr uint32_t COUNT = 4096U;
    Integer first[COUNT];
    Integer second[COUNT];
    uint64_t state = 1U;
    uint32_t index = 0U;
    while (index < COUNT)
    {
        state = state * 6364136223846793005ULL + 1U;
        first[index] = static_cast<Integer>((state >> 1U) & mask);
        state = state * 6364136223846793005ULL + 1U;
        second[index] = static_cast<Integer>((state >> 1U) & mask);
        if ((index & 1U) != 0U)
            first[index] = -first[index];
        if ((index & 2U) != 0U)
            second[index] = -second[index];
        index += 1U;
    }
    benchmark_operation(label, [&first, &second](uint32_t sample)
    {
        const uint32_t array_index = sample % COUNT;
        return (math_absdiff(first[array_index], second[array_index]));
    });
    return ;
}

int main()
{
    benchmark_operation("factorial32", [](uint32_t sample)
    {
        return (math_factorial(static_cast<int32_t>(sample % 16U) - 1));
    });
    benchmark_operation("factorial64", [](uint32_t sample)
    {
        return (math_factorial(static_cast<int64_t>(sample % 24U) - 1));
    });
    benchmark_absdiff<int32_t>("absdiff32", INT32_MAX);
    benchmark_absdiff<int64_t>("absdiff64", INT64_MAX);
    return (0);
}
