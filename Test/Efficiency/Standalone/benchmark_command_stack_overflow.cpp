#include "../../../Modules/Command/command.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t increment_value(void *context)
{
    ++*static_cast<uint32_t *>(context);
    return (FT_ERR_SUCCESS);
}

static int32_t decrement_value(void *context)
{
    --*static_cast<uint32_t *>(context);
    return (FT_ERR_SUCCESS);
}

static int32_t benchmark_capacity(ft_size_t capacity)
{
    constexpr uint32_t EXECUTE_COUNT = 100000;
    command_action entries[4096];
    command_stack stack;
    uint32_t count = 0;
    const command_action action = {
        "increment", increment_value, decrement_value, &count
    };
    if (command_stack_initialize(&stack, entries, capacity, nullptr, 0) != FT_ERR_SUCCESS)
        return (1);
    while (count < capacity)
    {
        if (command_stack_execute(&stack, &action) != FT_ERR_SUCCESS)
            return (1);
    }

    const auto start = std::chrono::steady_clock::now();
    while (count < EXECUTE_COUNT + capacity)
    {
        if (command_stack_execute(&stack, &action) != FT_ERR_SUCCESS)
            return (1);
    }
    const std::chrono::duration<double, std::micro> elapsed =
        std::chrono::steady_clock::now() - start;
    std::printf("command-%" PRIu64 " checksum %" PRIu32 " %.3f us/operation\n",
        capacity, count, elapsed.count() / EXECUTE_COUNT);
    return (command_stack_undo_count(&stack) != capacity);
}

int main()
{
    if (benchmark_capacity(4) != 0)
        return (1);
    return (benchmark_capacity(4096));
}
