#include "../../../Modules/Game/game_server.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_requests(const char *name, const char *text, int32_t expected)
{
    constexpr uint32_t ITERATIONS = 2000U;
    game_server server;
    ft_string message;
    if (server.initialize() != FT_ERR_SUCCESS || message.initialize(text) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    uint32_t matches = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (server.handle_message_locked(-1, message) == expected)
            ++matches;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (matches != ITERATIONS)
        return (1);
    std::printf("%s %.3f %" PRIu32 "\n", name, elapsed / ITERATIONS, matches);
    return (0);
}

static int32_t count_revision(json_group *group, void *context)
{
    if (group == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    const json_item *action = json_find_item(group, "action");
    if (action == ft_nullptr || ft_strcmp(action->value, "status") != 0)
        return (FT_ERR_INVALID_ARGUMENT);
    ++*static_cast<uint32_t *>(context);
    return (FT_ERR_SUCCESS);
}

static int32_t measure_revision_callback(ft_bool parsed)
{
    constexpr uint32_t ITERATIONS = 2000U;
    game_server server;
    ft_string message;
    if (server.initialize() != FT_ERR_SUCCESS || message.initialize(
        "{\"world_revision\":{\"protocol_version\":1,\"action\":\"status\","
        "\"request_id\":\"41\",\"selection_revision\":\"3\"}}") != FT_ERR_SUCCESS)
        return (1);
    uint32_t callbacks = 0U;
    if (parsed)
        server.set_parsed_message_callback([](int32_t, const char *, json_group &group,
            void *context) -> int32_t
        {
            return (count_revision(&group, context));
        }, &callbacks);
    else
        server.set_message_callback([](int32_t, const char *payload, void *context) -> int32_t
        {
            json_group *groups = json_read_from_string(payload);
            if (groups == ft_nullptr)
                return (FT_ERR_INVALID_ARGUMENT);
            const int32_t result = count_revision(json_find_group(groups, "world_revision"), context);
            json_free_groups(groups);
            return (result);
        }, &callbacks);
    ft_size_t allocations_before;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (server.handle_message_locked(-1, message) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    ft_size_t allocations_after;
    if (callbacks != ITERATIONS || cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("revision_%u %.3f %" PRIu32 " %" PRIu64 "\n", static_cast<uint32_t>(parsed),
        elapsed / ITERATIONS, callbacks, static_cast<uint64_t>(allocations_after - allocations_before));
    return (0);
}

int32_t main()
{
    if (measure_requests("join",
        "{\"event\":{\"id\":3,\"duration\":1},\"leave\":{\"id\":4},\"join\":{\"id\":7}}",
        FT_ERR_SUCCESS) != 0 || measure_requests("no_world",
        "{\"event\":{\"id\":3,\"duration\":1}}", FT_ERR_GAME_GENERAL_ERROR) != 0
        || measure_revision_callback(FT_FALSE) != 0)
        return (1);
    return (measure_revision_callback(FT_TRUE));
}
