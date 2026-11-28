#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Math/roll.hpp"
#include <chrono>
#include <cstdio>

static int32_t benchmark_roll(const char *label, uint32_t terms,
    uint32_t iterations, ft_bool grouped)
{
    constexpr uint32_t MAXIMUM_TERMS = 512U;
    char expression[MAXIMUM_TERMS * 6U];
    uint32_t length = 0U;
    uint32_t term = 0U;
    while (term < terms)
    {
        if (term != 0U)
            expression[length++] = '+';
        if (grouped)
            expression[length++] = '(';
        expression[length++] = '1';
        expression[length++] = 'd';
        expression[length++] = '1';
        if (grouped)
            expression[length++] = ')';
        term += 1U;
    }
    expression[length] = '\0';

    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        int32_t *result = math_roll(expression);
        if (result == ft_nullptr)
            return (1);
        const int32_t value = *result;
        cma_free(result);
        if (value != static_cast<int32_t>(terms))
            return (1);
        checksum += static_cast<uint32_t>(value);
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %" FT_PRIu64 "\n",
        label, elapsed / iterations, checksum);
    return (0);
}

int main()
{
    if (benchmark_roll("roll 8 terms", 8U, 8192U, FT_FALSE) != 0
        || benchmark_roll("roll 512 terms", 512U, 32U, FT_FALSE) != 0
        || benchmark_roll("roll 128 grouped terms", 128U, 128U, FT_TRUE) != 0)
        return (1);
    return (0);
}
