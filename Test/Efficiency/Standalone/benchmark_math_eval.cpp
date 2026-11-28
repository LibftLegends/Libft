#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Math/roll.hpp"
#include <chrono>
#include <cstdio>

static int32_t benchmark_eval(const char *label, uint32_t terms, uint32_t iterations)
{
    constexpr uint32_t MAXIMUM_TERMS = 1024U;
    char expression[MAXIMUM_TERMS * 2U];
    uint32_t term = 0U;
    while (term < terms)
    {
        expression[term * 2U] = '1';
        expression[term * 2U + 1U] = '+';
        term += 1U;
    }
    expression[terms * 2U - 1U] = '\0';

    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < iterations)
    {
        int32_t *result = math_eval(expression);
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
    if (benchmark_eval("eval 8 terms", 8U, 8192U) != 0
        || benchmark_eval("eval 32 terms", 32U, 1024U) != 0
        || benchmark_eval("eval 1024 terms", 1024U, 16U) != 0)
        return (1);
    return (0);
}
