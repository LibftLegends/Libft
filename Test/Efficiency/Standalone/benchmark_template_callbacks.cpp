#include "../../../Modules/Template/event_emitter.hpp"
#include "../../../Modules/Template/function.hpp"
#include <chrono>
#include <cstdio>
#include <string>

using benchmark_clock = std::chrono::steady_clock;

static void print_average(const char *name, benchmark_clock::duration elapsed,
    uint32_t iterations)
{
    std::printf("%s ns/op=%.2f\n", name,
        std::chrono::duration<double, std::nano>(elapsed).count() / iterations);
    return ;
}

static int32_t benchmark_dispatch()
{
    constexpr uint32_t ITERATIONS = 200000U;
    const std::string payload(2048U, 'a');
    ft_function<ft_size_t(std::string)> callback([](const std::string &argument)
    {
        return (static_cast<ft_size_t>(argument.size()));
    });
    ft_size_t total = 0U;
    uint32_t iteration = 0U;
    const auto start = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        total += callback(payload);
        ++iteration;
    }
    const auto elapsed = benchmark_clock::now() - start;
    if (total != payload.size() * ITERATIONS)
        return (1);
    print_average("function_nontrivial_value_dispatch", elapsed, ITERATIONS);
    return (0);
}

static void count_listener(uint32_t &count)
{
    ++count;
    return ;
}

static int32_t benchmark_listener_removal()
{
    constexpr uint32_t LISTENERS = 4096U;
    constexpr uint32_t ITERATIONS = 20000U;
    ft_event_emitter<uint32_t, uint32_t &> emitter(LISTENERS);
    if (emitter.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < LISTENERS)
    {
        emitter.on(17U, count_listener);
        ++index;
    }
    uint32_t iteration = 0U;
    const auto start = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        emitter.remove_listener(17U, count_listener);
        emitter.on(17U, count_listener);
        ++iteration;
    }
    const auto elapsed = benchmark_clock::now() - start;
    uint32_t delivered = 0U;
    emitter.emit(17U, delivered);
    if (delivered != LISTENERS)
        return (1);
    print_average("event_remove_head_and_append", elapsed, ITERATIONS);
    return (0);
}

int32_t main()
{
    if (benchmark_dispatch() != 0)
        return (1);
    return (benchmark_listener_removal());
}
