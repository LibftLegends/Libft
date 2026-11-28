#include "../../../Modules/Game/game_hooks.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t populate_registry(game_hooks &hooks, uint32_t count)
{
    uint32_t index = 0U;
    char hook_identifier[] = "hook-aa";
    while (index < count)
    {
        hook_identifier[5] = static_cast<char>('a' + index / 26U);
        hook_identifier[6] = static_cast<char>('a' + index % 26U);
        ft_game_hook_metadata metadata;
        if (metadata.hook_identifier.initialize(hook_identifier) != FT_ERR_SUCCESS
            || metadata.listener_name.initialize("listener") != FT_ERR_SUCCESS
            || metadata.description.initialize("Example listener for the registry benchmark") != FT_ERR_SUCCESS
            || metadata.argument_contract.initialize("No arguments") != FT_ERR_SUCCESS)
            return (1);
        hooks.register_listener(metadata, 0, [](ft_game_hook_context &) {});
        if (hooks.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    return (0);
}

static int32_t measure_registry_copy(uint32_t count)
{
    constexpr uint32_t ITERATIONS = 200U;
    game_hooks source;
    if (source.initialize() != FT_ERR_SUCCESS || populate_registry(source, count) != 0)
        return (1);
    uint32_t index = 0U;
    const auto copy_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        game_hooks copy;
        if (copy.initialize(source) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double copy_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - copy_start).count();
    index = 0U;
    uint64_t checksum = 0U;
    const auto metadata_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        ft_vector<ft_game_hook_metadata> *metadata = source.get_catalog_metadata();
        if (metadata == ft_nullptr)
            return (1);
        checksum += metadata->size();
        delete metadata;
        ++index;
    }
    const double metadata_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - metadata_start).count();
    if (checksum != static_cast<uint64_t>(count) * ITERATIONS)
        return (1);
    std::printf("registry_%u %.3f %.3f %" PRIu64 "\n", count,
        copy_elapsed / ITERATIONS, metadata_elapsed / ITERATIONS, checksum);
    return (0);
}

static int32_t measure_priority_registration()
{
    constexpr uint32_t ITERATIONS = 200U;
    game_hooks hooks;
    if (hooks.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    char listener_name[] = "listener-a";
    while (index < 16U)
    {
        listener_name[9] = static_cast<char>('a' + index);
        ft_game_hook_metadata metadata;
        if (metadata.hook_identifier.initialize("priority") != FT_ERR_SUCCESS
            || metadata.listener_name.initialize(listener_name) != FT_ERR_SUCCESS
            || metadata.description.initialize("Existing benchmark listener") != FT_ERR_SUCCESS
            || metadata.argument_contract.initialize("No arguments") != FT_ERR_SUCCESS)
            return (1);
        hooks.register_listener(metadata, 0, [](ft_game_hook_context &) {});
        if (hooks.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    ft_game_hook_metadata high_priority;
    if (high_priority.hook_identifier.initialize("priority") != FT_ERR_SUCCESS
        || high_priority.listener_name.initialize("high") != FT_ERR_SUCCESS
        || high_priority.description.initialize("Higher priority listener") != FT_ERR_SUCCESS
        || high_priority.argument_contract.initialize("No arguments") != FT_ERR_SUCCESS)
        return (1);
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        hooks.register_listener(high_priority, 1, [](ft_game_hook_context &) {});
        if (hooks.get_error() != FT_ERR_SUCCESS)
            return (1);
        hooks.unregister_listener(high_priority.hook_identifier, high_priority.listener_name);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    ft_vector<ft_game_hook_metadata> *metadata = hooks.get_catalog_metadata();
    if (metadata == ft_nullptr)
        return (1);
    const ft_size_t checksum = metadata->size();
    delete metadata;
    if (checksum != 16U)
        return (1);
    std::printf("priority_registration %.3f %" PRIu64 "\n", elapsed / ITERATIONS,
        static_cast<uint64_t>(checksum));
    return (0);
}

static int32_t measure_registry_move(uint32_t count)
{
    constexpr uint32_t ITERATIONS = 200U;
    game_hooks first;
    game_hooks second;
    if (first.initialize() != FT_ERR_SUCCESS || populate_registry(first, count) != 0)
        return (1);
    game_hooks *source = &first;
    game_hooks *destination = &second;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (destination->move(*source) != FT_ERR_SUCCESS)
            return (1);
        game_hooks *previous_source = source;
        source = destination;
        destination = previous_source;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    ft_vector<ft_game_hook_metadata> *metadata = source->get_catalog_metadata();
    if (metadata == ft_nullptr)
        return (1);
    const ft_size_t checksum = metadata->size();
    delete metadata;
    if (checksum != count)
        return (1);
    std::printf("move_%u %.3f %" PRIu64 "\n", count, elapsed / ITERATIONS,
        static_cast<uint64_t>(checksum));
    return (0);
}

static int32_t measure_filtered_catalog(const game_hooks &hooks, const char *key,
    ft_size_t expected_matches)
{
    constexpr uint32_t ITERATIONS = 4000U;
    ft_string identifier;
    if (identifier.initialize(key) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        ft_vector<ft_game_hook_metadata> *metadata = hooks.get_catalog_metadata_for(identifier);
        if (metadata == ft_nullptr)
            return (1);
        checksum += metadata->size();
        delete metadata;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != expected_matches * ITERATIONS)
        return (1);
    std::printf("filtered_%s %.3f %" PRIu64 "\n", key, elapsed / ITERATIONS, checksum);
    return (0);
}

int main()
{
    if (measure_registry_copy(8U) != 0 || measure_registry_copy(32U) != 0
        || measure_registry_move(8U) != 0 || measure_registry_move(32U) != 0
        || measure_priority_registration() != 0)
        return (1);
    game_hooks hooks;
    if (hooks.initialize() != FT_ERR_SUCCESS || populate_registry(hooks, 64U) != 0
        || measure_filtered_catalog(hooks, "missing", 0U) != 0)
        return (1);
    return (measure_filtered_catalog(hooks, "hook-aa", 1U));
}
