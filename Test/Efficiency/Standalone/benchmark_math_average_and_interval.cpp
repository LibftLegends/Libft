#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/Math/math.hpp"
#include <chrono>
#include <cstdio>

template <typename Value, typename Operation>
static void benchmark_operation(const char *label, Operation operation)
{
    constexpr uint32_t ITERATIONS = 1048576U;
    Value checksum{};
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        checksum += static_cast<Value>(operation(iteration));
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %.17g\n",
        label, elapsed / ITERATIONS, static_cast<double>(checksum));
    return ;
}

static ft_interval make_positive_interval(uint32_t value)
{
    const double lower = 1.0 + static_cast<double>(value % 113U) / 128.0;
    return (ft_interval{lower, lower + 0.125, FT_ERR_SUCCESS});
}

int main()
{
    benchmark_operation<int64_t>("average32", [](uint32_t index)
    {
        const int32_t first = static_cast<int32_t>(index % 257U) - 100;
        const int32_t second = static_cast<int32_t>(index % 101U) - 43;
        return (math_average(first, second));
    });
    benchmark_operation<int64_t>("average64", [](uint32_t index)
    {
        const int64_t first = static_cast<int64_t>(index % 257U) - 100;
        const int64_t second = static_cast<int64_t>(index % 101U) - 43;
        return (math_average(first, second));
    });
    benchmark_operation<double>("multiply", [](uint32_t index)
    {
        const ft_interval first = make_positive_interval(index);
        const ft_interval second = make_positive_interval(index * 7U);
        const ft_interval result = ft_interval_multiply(first, second);
        return (result.lower + result.upper);
    });
    benchmark_operation<double>("divide", [](uint32_t index)
    {
        const ft_interval first = make_positive_interval(index);
        const ft_interval second = make_positive_interval(index * 7U);
        const ft_interval result = ft_interval_divide(first, second);
        return (result.lower + result.upper);
    });
    return (0);
}
