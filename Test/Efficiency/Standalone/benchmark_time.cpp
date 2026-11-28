#include "../../../Modules/Time/time.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cstdio>

using benchmark_clock = std::chrono::steady_clock;
static volatile uint64_t observed;

template <typename Operation>
static void benchmark(const char *name, uint32_t iterations, Operation operation)
{
    uint64_t result = 0U;
    uint32_t index = 0U;
    const auto started = benchmark_clock::now();
    while (index < iterations)
    {
        result += operation(index);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        benchmark_clock::now() - started).count();
    observed = result;
    std::printf("%s ns/op=%.2f\n", name, elapsed / static_cast<double>(iterations));
    return ;
}

static uint64_t consume(ft_string *value)
{
    if (value == ft_nullptr)
        return (0U);
    const ft_size_t size = value->size();
    delete value;
    return (size);
}

int main()
{
    constexpr uint32_t CLOCK_ITERATIONS = 1000000U;
    constexpr uint32_t FORMAT_ITERATIONS = 150000U;
    constexpr t_time EPOCH = 1700000000;
    t_time_info date{};
    time_local(EPOCH, &date);
    benchmark("time_high_resolution_now", CLOCK_ITERATIONS, [](uint32_t)
    {
        t_high_resolution_time_point point;
        if (!time_high_resolution_now(&point))
            return (uint64_t{0U});
        return (static_cast<uint64_t>(point.nanoseconds));
    });
    benchmark("time_monotonic", CLOCK_ITERATIONS, [](uint32_t)
    {
        return (static_cast<uint64_t>(time_monotonic()));
    });
    benchmark("time_strftime", FORMAT_ITERATIONS, [&date](uint32_t)
    {
        char buffer[24];
        return (time_strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &date));
    });
    benchmark("time_format_rfc3339_with_offset", FORMAT_ITERATIONS, [](uint32_t index)
    {
        return (consume(time_format_rfc3339_with_offset(EPOCH + index, 330)));
    });
    benchmark("time_format_relative", FORMAT_ITERATIONS, [](uint32_t index)
    {
        return (consume(time_format_relative(time_duration_ms_create(90010000 + index))));
    });
    benchmark("time_parse_rfc3339_with_offset", FORMAT_ITERATIONS, [](uint32_t)
    {
        t_time timestamp = 0;
        if (!time_parse_rfc3339("2025-01-15T12:30:15+05:30", ft_nullptr,
                &timestamp))
            return (uint64_t{0U});
        return (static_cast<uint64_t>(timestamp));
    });
    benchmark("time_format_interval", FORMAT_ITERATIONS, [](uint32_t index)
    {
        return (consume(time_format_interval(EPOCH + index, EPOCH + index + 3600)));
    });
    if (observed != FORMAT_ITERATIONS * 41U)
        return (1);
    return (0);
}
