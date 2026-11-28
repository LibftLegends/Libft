#include "../../../Modules/Template/circular_buffer.hpp"
#include "../../../Modules/Template/priority_queue.hpp"
#include "../../../Modules/Template/vector.hpp"
#include <chrono>
#include <cstdio>

using benchmark_clock = std::chrono::steady_clock;
static volatile ft_size_t circular_capacity = 1009U;

static int32_t benchmark_circular_buffer()
{
    constexpr uint32_t ITERATIONS = 4000000U;
    ft_circular_buffer<uint32_t> buffer(circular_capacity);
    if (buffer.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint64_t total = 0U;
    uint32_t index = 0U;
    const auto start = benchmark_clock::now();
    while (index < ITERATIONS)
    {
        buffer.push(index);
        total += buffer.pop();
        ++index;
    }
    const auto elapsed = benchmark_clock::now() - start;
    if (total != static_cast<uint64_t>(ITERATIONS) * (ITERATIONS - 1U) / 2U)
        return (1);
    std::printf("circular_push_pop ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t benchmark_vector_reserve()
{
    constexpr uint32_t ITERATIONS = 200U;
    constexpr ft_size_t LENGTH = 262144U;
    benchmark_clock::duration elapsed{};
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        ft_vector<uint32_t> values;
        if (values.initialize() != FT_ERR_SUCCESS)
            return (1);
        values.resize(LENGTH, iteration + 1U);
        if (values.get_error() != FT_ERR_SUCCESS)
            return (1);
        const auto start = benchmark_clock::now();
        values.reserve(LENGTH * 2U);
        elapsed += benchmark_clock::now() - start;
        if (values.get_error() != FT_ERR_SUCCESS || values.size() != LENGTH
            || values[LENGTH - 1U] != iteration + 1U)
            return (1);
        ++iteration;
    }
    std::printf("vector_reserve_262144 ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}

struct s_priority_queue_entry
{
    uint32_t priority;
    uint32_t payload[15];

    ft_bool operator<(const s_priority_queue_entry &other) const;
};

ft_bool s_priority_queue_entry::operator<(const s_priority_queue_entry &other) const
{
    return (this->priority < other.priority);
}

static int32_t benchmark_priority_queue()
{
    constexpr uint32_t LENGTH = 8192U;
    constexpr uint32_t ITERATIONS = 100U;
    ft_priority_queue<s_priority_queue_entry> values(LENGTH);
    if (values.initialize() != FT_ERR_SUCCESS)
        return (1);
    const auto start = benchmark_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        uint32_t index = 0U;
        while (index < LENGTH)
        {
            s_priority_queue_entry value{};
            value.priority = index;
            uint32_t word = 0U;
            while (word < sizeof(value.payload) / sizeof(value.payload[0]))
            {
                value.payload[word] = index + word;
                ++word;
            }
            values.push(ft_move(value));
            ++index;
        }
        while (index != 0U)
        {
            --index;
            const s_priority_queue_entry value = values.pop();
            if (value.priority != index || value.payload[iteration % 15U] != index + iteration % 15U)
                return (1);
        }
        ++iteration;
    }
    std::printf("priority_queue_push_pop ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(benchmark_clock::now() - start).count()
        / (LENGTH * ITERATIONS));
    return (0);
}

int main()
{
    if (benchmark_circular_buffer() != 0 || benchmark_vector_reserve() != 0)
        return (1);
    return (benchmark_priority_queue());
}
