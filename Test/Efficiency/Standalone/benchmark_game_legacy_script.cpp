#include "../../../Modules/Game/game_scripting_bridge.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t sum_arguments(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    if (arguments.size() != 3U)
        return (FT_ERR_INVALID_ARGUMENT);
    auto *total = static_cast<int64_t *>(context.get_user_data());
    if (total == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *total += ft_atoi(arguments[0].c_str()) + ft_atoi(arguments[1].c_str())
        + ft_atoi(arguments[2].c_str());
    return (FT_ERR_SUCCESS);
}

int32_t main()
{
    constexpr uint32_t ITERATIONS = 512U;
    constexpr uint32_t COMMAND_GROUPS = 16U;
    game_script_bridge bridge;
    game_state state;
    ft_string script;
    ft_string callback_name;
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)> callback(sum_arguments);
    if (bridge.initialize() != FT_ERR_SUCCESS || state.initialize() != FT_ERR_SUCCESS
        || script.initialize() != FT_ERR_SUCCESS
        || callback_name.initialize("sum") != FT_ERR_SUCCESS
        || bridge.register_function(callback_name, callback) != FT_ERR_SUCCESS)
        return (1);
    bridge.set_max_operations(static_cast<int32_t>(COMMAND_GROUPS * 3U));
    uint32_t command = 0U;
    while (command < COMMAND_GROUPS)
    {
        if (script.append(" \t# comment\r\n   set value 10 \r\n"
                " call sum 2 3 4\r\nunset value\n") != FT_ERR_SUCCESS)
            return (1);
        ++command;
    }
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    int64_t total = 0;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (bridge.execute_with_user_data(script, &state, &total) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS
        || total != static_cast<int64_t>(ITERATIONS) * COMMAND_GROUPS * 9)
        return (1);
    std::printf("legacy_script %.3f %" PRIu64 " %" PRId64 "\n", elapsed / ITERATIONS,
        ft_platform_cast<uint64_t>(allocations_after - allocations_before), total);
    return (0);
}
