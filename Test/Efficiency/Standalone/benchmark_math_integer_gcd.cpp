#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/Math/math.hpp"
#include <chrono>
#include <cstdio>

template <typename Integer>
static void benchmark_operation(const char *label,
    Integer (*operation)(Integer, Integer), uint64_t input_mask)
{
    constexpr uint32_t SAMPLE_COUNT = 4096U;
    constexpr uint32_t REPETITIONS = 128U;
    Integer first_numbers[SAMPLE_COUNT];
    Integer second_numbers[SAMPLE_COUNT];
    uint64_t random_state = 1U;
    uint32_t sample = 0U;
    while (sample < SAMPLE_COUNT)
    {
        random_state = random_state * 6364136223846793005ULL + 1U;
        first_numbers[sample] = static_cast<Integer>((random_state >> 1U) & input_mask);
        random_state = random_state * 6364136223846793005ULL + 1U;
        second_numbers[sample] = -static_cast<Integer>((random_state >> 1U) & input_mask);
        sample += 1U;
    }
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    uint32_t repetition = 0U;
    while (repetition < REPETITIONS)
    {
        sample = 0U;
        while (sample < SAMPLE_COUNT)
        {
            checksum += static_cast<uint64_t>(
                operation(first_numbers[sample], second_numbers[sample]));
            sample += 1U;
        }
        repetition += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %" FT_PRIu64 "\n",
        label, elapsed / (SAMPLE_COUNT * REPETITIONS), checksum);
    return ;
}

int32_t main()
{
    benchmark_operation<int32_t>("gcd32 small", math_gcd, 63U);
    benchmark_operation<int32_t>("gcd32 wide", math_gcd, INT32_MAX);
    benchmark_operation<int64_t>("gcd64 small", math_gcd, 63U);
    benchmark_operation<int64_t>("gcd64 wide", math_gcd, INT64_MAX);
    benchmark_operation<int32_t>("lcm32 small", math_lcm, 63U);
    benchmark_operation<int32_t>("lcm32 wide", math_lcm, INT32_MAX);
    benchmark_operation<int64_t>("lcm64 small", math_lcm, 63U);
    benchmark_operation<int64_t>("lcm64 wide", math_lcm, INT64_MAX);
    return (0);
}
