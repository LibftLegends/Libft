#include "../../../Modules/Game/game_data_catalog.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_items(ft_bool locked)
{
    constexpr int32_t LAST_ID = 512;
    constexpr uint32_t ITERATIONS = 50000U;
    game_data_catalog catalog;
    if (catalog.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t item_id = 1;
    while (item_id <= LAST_ID)
    {
        game_item_definition definition;
        if (definition.initialize(item_id, 2, 64, 1, 2, 3, 0) != FT_ERR_SUCCESS
            || catalog.register_item_definition(definition) != FT_ERR_SUCCESS)
            return (1);
        ++item_id;
    }
    if (locked == FT_TRUE && catalog.get_item_definitions().enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    game_item_definition definition;
    game_item_definition fetched;
    if (definition.initialize(LAST_ID, 3, 96, 2, 3, 4, 1) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (catalog.register_item_definition(definition) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double replace_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    iteration = 0U;
    start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (catalog.fetch_item_definition(LAST_ID, fetched) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double fetch_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (fetched.get_item_id() != LAST_ID || fetched.get_max_stack() != 96)
        return (1);
    const int32_t checksum = fetched.get_item_id() + fetched.get_rarity() + fetched.get_max_stack()
        + fetched.get_width() + fetched.get_height() + fetched.get_weight() + fetched.get_slot_requirement();
    std::printf("replace_%u %.3f %" PRId32 "\n", static_cast<uint32_t>(locked),
        replace_elapsed / ITERATIONS, checksum);
    std::printf("fetch_%u %.3f %" PRId32 "\n", static_cast<uint32_t>(locked),
        fetch_elapsed / ITERATIONS, checksum);
    return (0);
}

static int32_t measure_moves()
{
    constexpr uint32_t ITERATIONS = 256U;
    constexpr int32_t ITEM_COUNT = 256;
    constexpr int32_t BLUEPRINT_COUNT = 64;
    game_data_catalog fixture;
    game_data_catalog destination;
    if (fixture.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t identifier = 1;
    while (identifier <= ITEM_COUNT)
    {
        game_item_definition item;
        if (item.initialize(identifier, 2, 64, 1, 2, 3, 0) != FT_ERR_SUCCESS
            || fixture.register_item_definition(item) != FT_ERR_SUCCESS)
            return (1);
        ++identifier;
    }
    game_recipe_blueprint recipe;
    game_loadout_blueprint loadout;
    if (recipe.initialize() != FT_ERR_SUCCESS || loadout.initialize() != FT_ERR_SUCCESS)
        return (1);
    identifier = 1;
    while (identifier <= BLUEPRINT_COUNT)
    {
        recipe.set_recipe_id(identifier);
        loadout.set_loadout_id(identifier);
        if (fixture.register_recipe(recipe) != FT_ERR_SUCCESS
            || fixture.register_loadout(loadout) != FT_ERR_SUCCESS)
            return (1);
        ++identifier;
    }
    using clock_type = std::chrono::steady_clock;
    clock_type::duration elapsed{};
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        game_data_catalog source;
        if (source.initialize(fixture) != FT_ERR_SUCCESS)
            return (1);
        const auto start = clock_type::now();
        const int32_t result = destination.move(source);
        elapsed += clock_type::now() - start;
        if (result != FT_ERR_SUCCESS)
            return (1);
        checksum += destination.get_item_definitions().size()
            + destination.get_recipes().size() + destination.get_loadouts().size();
        ++iteration;
    }
    if (checksum != ITERATIONS * (ITEM_COUNT + 2U * BLUEPRINT_COUNT))
        return (1);
    const double nanoseconds = std::chrono::duration<double, std::nano>(elapsed).count();
    std::printf("move %.3f %" PRIu64 "\n", nanoseconds / ITERATIONS, checksum);
    return (0);
}

static int32_t measure_blueprints()
{
    constexpr uint32_t ITERATIONS = 1024U;
    constexpr ft_size_t ENTRY_COUNT = 128U;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_vector<game_loadout_entry> entries;
    game_crafting_ingredient ingredient;
    game_loadout_entry entry;
    if (ingredients.initialize() != FT_ERR_SUCCESS || entries.initialize() != FT_ERR_SUCCESS
        || ingredient.initialize(4, 2, 0) != FT_ERR_SUCCESS
        || entry.initialize(1, 4, 2) != FT_ERR_SUCCESS)
        return (1);
    while (ingredients.size() < ENTRY_COUNT)
    {
        if (ingredients.push_back(ingredient) != FT_ERR_SUCCESS
            || entries.push_back(entry) != FT_ERR_SUCCESS)
            return (1);
    }
    game_recipe_blueprint recipe;
    game_loadout_blueprint loadout;
    if (recipe.initialize(7, 5, ingredients) != FT_ERR_SUCCESS
        || loadout.initialize(8, entries) != FT_ERR_SUCCESS)
        return (1);
    using clock_type = std::chrono::steady_clock;
    clock_type::duration copy_elapsed{};
    clock_type::duration move_elapsed{};
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        game_recipe_blueprint copied_recipe;
        game_loadout_blueprint copied_loadout;
        game_recipe_blueprint moved_recipe;
        game_loadout_blueprint moved_loadout;
        auto start = clock_type::now();
        if (copied_recipe.initialize(recipe) != FT_ERR_SUCCESS
            || copied_loadout.initialize(loadout) != FT_ERR_SUCCESS)
            return (1);
        copy_elapsed += clock_type::now() - start;
        start = clock_type::now();
        if (moved_recipe.move(copied_recipe) != FT_ERR_SUCCESS
            || moved_loadout.move(copied_loadout) != FT_ERR_SUCCESS)
            return (1);
        move_elapsed += clock_type::now() - start;
        checksum += moved_recipe.get_ingredients().size() + moved_loadout.get_entries().size();
        ++iteration;
    }
    if (checksum != ITERATIONS * 2U * ENTRY_COUNT)
        return (1);
    const double copy_nanoseconds = std::chrono::duration<double, std::nano>(copy_elapsed).count();
    const double move_nanoseconds = std::chrono::duration<double, std::nano>(move_elapsed).count();
    std::printf("blueprint_copy %.3f %" PRIu64 "\n", copy_nanoseconds / ITERATIONS, checksum);
    std::printf("blueprint_move %.3f %" PRIu64 "\n", move_nanoseconds / ITERATIONS, checksum);
    return (0);
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--move") == 0)
        return (measure_moves());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--blueprints") == 0)
        return (measure_blueprints());
    if (measure_items(FT_FALSE) != 0)
        return (1);
    return (measure_items(FT_TRUE));
}
