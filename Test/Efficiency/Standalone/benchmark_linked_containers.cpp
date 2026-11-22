#include "../../../Modules/Template/deque.hpp"
#include "../../../Modules/Template/queue.hpp"
#include "../../../Modules/Template/stack.hpp"
#include <chrono>
#include <cstdio>

using benchmark_clock = std::chrono::steady_clock;
static constexpr uint32_t ITERATIONS = 1000000U;

static int32_t report_timing(const char *name, benchmark_clock::duration elapsed,
    uint64_t sum)
{
    if (sum != static_cast<uint64_t>(ITERATIONS) * (ITERATIONS - 1U) / 2U)
        return (1);
    std::printf("%s ns/pair=%.2f\n", name,
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t benchmark_queue()
{
    ft_queue<uint32_t> queue;
    if (queue.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    uint64_t sum = 0U;
    const auto start = benchmark_clock::now();
    while (index < ITERATIONS)
    {
        queue.enqueue(index);
        sum += queue.dequeue();
        ++index;
    }
    return (report_timing("queue_enqueue_dequeue", benchmark_clock::now() - start, sum));
}

static int32_t benchmark_stack()
{
    ft_stack<uint32_t> stack;
    if (stack.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    uint64_t sum = 0U;
    const auto start = benchmark_clock::now();
    while (index < ITERATIONS)
    {
        stack.push(index);
        sum += stack.pop();
        ++index;
    }
    return (report_timing("stack_push_pop", benchmark_clock::now() - start, sum));
}

int32_t main()
{
    if (benchmark_queue() != 0 || benchmark_stack() != 0)
        return (1);
    ft_deque<uint32_t> deque;
    if (deque.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    uint64_t sum = 0U;
    const auto start = benchmark_clock::now();
    while (index < ITERATIONS)
    {
        deque.push_back(index);
        sum += deque.pop_front();
        ++index;
    }
    return (report_timing("deque_push_back_pop_front", benchmark_clock::now() - start, sum));
}
