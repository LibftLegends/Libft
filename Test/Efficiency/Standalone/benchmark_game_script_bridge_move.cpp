#include "../../../Modules/Game/game_scripting_bridge.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_bridge_move()
{
    constexpr uint32_t REGISTRATIONS = 48U;
    constexpr uint32_t ITERATIONS = 250U;
    game_script_bridge first;
    game_script_bridge second;
    ft_string callback_name;
    uint32_t calls = 0U;
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)> callback(
        [&calls](game_script_context &, const ft_vector<ft_string> &) noexcept
        {
            ++calls;
            return (FT_ERR_SUCCESS);
        });
    if (first.initialize() != FT_ERR_SUCCESS || callback_name.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < REGISTRATIONS)
    {
        char name[32];
        const int32_t length = std::snprintf(name, sizeof(name), "callback_%02" PRIu32, index);
        if (length < 0 || callback_name.assign(name, static_cast<ft_size_t>(length)) != FT_ERR_SUCCESS
            || first.register_function(callback_name, callback) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    game_script_bridge *source = &first;
    game_script_bridge *destination = &second;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (destination->move(*source) != FT_ERR_SUCCESS)
            return (1);
        game_script_bridge *previous = source;
        source = destination;
        destination = previous;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS
        || source->get_callback_count() != REGISTRATIONS)
        return (1);
    ft_string script;
    if (script.initialize("callback_47()") != FT_ERR_SUCCESS
        || source->execute_custom_with_user_data(script, ft_nullptr, ft_nullptr) != FT_ERR_SUCCESS
        || calls != 1U)
        return (1);
    std::printf("bridge_move %.6f %" PRIu64 "\n", elapsed / ITERATIONS,
        ft_platform_cast<uint64_t>(allocations_after - allocations_before));
    return (0);
}

static int32_t measure_context_move()
{
    constexpr uint32_t VARIABLES = 48U;
    constexpr uint32_t ITERATIONS = 1000U;
    game_script_context first;
    game_script_context second;
    ft_string key;
    ft_string value;
    if (first.initialize() != FT_ERR_SUCCESS || key.initialize() != FT_ERR_SUCCESS
        || value.initialize("stored value") != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < VARIABLES)
    {
        char name[32];
        const int32_t length = std::snprintf(name, sizeof(name), "variable_%02" PRIu32, index);
        if (length < 0 || key.assign(name, static_cast<ft_size_t>(length)) != FT_ERR_SUCCESS)
            return (1);
        first.set_variable(key, value);
        if (first.get_variable(key) == ft_nullptr)
            return (1);
        ++index;
    }
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    game_script_context *source = &first;
    game_script_context *destination = &second;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (destination->move(*source) != FT_ERR_SUCCESS)
            return (1);
        game_script_context *previous = source;
        source = destination;
        destination = previous;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    const ft_string *stored = source->get_variable(key);
    if (stored == ft_nullptr || *stored != value)
        return (1);
    std::printf("context_move %.6f %" PRIu64 "\n", elapsed / ITERATIONS,
        ft_platform_cast<uint64_t>(allocations_after - allocations_before));
    return (0);
}

int main()
{
    if (measure_bridge_move() != 0 || measure_context_move() != 0)
        return (1);
    return (0);
}
