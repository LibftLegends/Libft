#include "../../../Modules/Game/game_crafting.hpp"
#include "../../../Modules/Game/game_inventory.hpp"
#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t register_recipe(game_crafting &crafting)
{
    game_crafting_ingredient input;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> output(new game_item());
    if (!output || output->initialize() != FT_ERR_SUCCESS
        || input.initialize(17, 8, -1) != FT_ERR_SUCCESS
        || ingredients.initialize() != FT_ERR_SUCCESS
        || ingredients.push_back(input) != FT_ERR_SUCCESS)
        return (1);
    output->set_item_id(42);
    output->set_max_stack(64);
    output->set_stack_size(1);
    return (crafting.register_recipe(9, ft_move(ingredients), output, 1, FT_TRUE));
}

static int32_t measure_query(const game_crafting &crafting, const game_inventory &inventory,
    int32_t stride, uint32_t iterations, const game_inventory_snapshot *snapshot)
{
    ft_vector<game_crafting_missing_ingredient> missing;
    game_crafting_query_result query = {};
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        int32_t result;
        if (snapshot == ft_nullptr)
            result = crafting.query_recipe(9, inventory, &query, &missing);
        else
            result = crafting.query_recipe_at_station_snapshot(9, *snapshot, 0, &query, &missing);
        if (result != FT_ERR_SUCCESS)
            return (1);
        checksum += query.craftable;
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != iterations || !missing.empty() || inventory.count_item(17) != 8)
        return (1);
    std::printf("query_%" PRId32 " %.3f %" PRIu64 "\n", stride, elapsed / iterations, checksum);
    return (0);
}

static int32_t measure_prepare(int32_t stride, uint32_t iterations, ft_bool commit = FT_FALSE,
    ft_bool query = FT_FALSE, ft_bool snapshot_query = FT_FALSE)
{
    constexpr int32_t ITEM_COUNT = 64;
    game_inventory inventory;
    game_crafting crafting;
    game_crafting_prepared prepared;
    if (inventory.initialize() != FT_ERR_SUCCESS || crafting.initialize() != FT_ERR_SUCCESS
        || register_recipe(crafting) != FT_ERR_SUCCESS)
        return (1);
    game_inventory_snapshot snapshot = {};
    snapshot.next_slot = ITEM_COUNT * stride;
    snapshot.current_weight = ITEM_COUNT;
    snapshot.used_slots = ITEM_COUNT;
    int32_t index = 0;
    while (index < ITEM_COUNT)
    {
        game_inventory_item_snapshot item = {};
        item.slot = index * stride;
        item.item_id = 1;
        if (index >= ITEM_COUNT - 8)
            item.item_id = 17;
        item.max_stack = 1;
        item.stack_size = 1;
        item.width = 1;
        item.height = 1;
        snapshot.items.push_back(item);
        ++index;
    }
    if (snapshot_query == FT_TRUE && stride > 1)
        std::reverse(snapshot.items.begin(), snapshot.items.end());
    if (inventory.restore_snapshot(snapshot) != FT_ERR_SUCCESS)
        return (1);
    if (snapshot_query == FT_TRUE)
        return (measure_query(crafting, inventory, stride, iterations, &snapshot));
    if (query == FT_TRUE)
        return (measure_query(crafting, inventory, stride, iterations, ft_nullptr));
    uint32_t iteration = 0U;
    std::chrono::steady_clock::duration commit_elapsed{};
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (crafting.prepare_craft(inventory, 9, 1, &prepared) != FT_ERR_SUCCESS)
            return (1);
        if (commit == FT_TRUE)
        {
            const auto commit_start = std::chrono::steady_clock::now();
            const int32_t result = crafting.commit_craft(inventory, &prepared);
            commit_elapsed += std::chrono::steady_clock::now() - commit_start;
            if (result != FT_ERR_SUCCESS)
                return (1);
            if (iteration + 1U != iterations && inventory.restore_snapshot(snapshot) != FT_ERR_SUCCESS)
                return (1);
        }
        ++iteration;
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (commit == FT_TRUE)
        elapsed = commit_elapsed;
    else if (inventory.count_item(17) != 8
        || crafting.commit_craft(inventory, &prepared) != FT_ERR_SUCCESS)
        return (1);
    if (inventory.get_snapshot(snapshot) != FT_ERR_SUCCESS || inventory.count_item(17) != 0
        || inventory.count_item(42) != 1 || snapshot.current_weight != ITEM_COUNT - 7)
        return (1);
    uint64_t checksum = 0U;
    auto item = snapshot.items.begin();
    while (item != snapshot.items.end())
    {
        checksum = checksum * 31U + static_cast<uint64_t>(item->slot)
            + static_cast<uint64_t>(item->item_id);
        ++item;
    }
    const char *operation = "prepare";
    if (commit == FT_TRUE)
        operation = "commit";
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("%s_%" PRId32 " %.3f %" PRIu64 "\n", operation, stride,
        microseconds / iterations, checksum);
    return (0);
}

static int32_t measure_legacy(int32_t item_count, uint32_t iterations)
{
    game_inventory inventory;
    game_crafting crafting;
    game_crafting_ingredient input;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> output(new game_item());
    if (inventory.initialize() != FT_ERR_SUCCESS || crafting.initialize() != FT_ERR_SUCCESS
        || !output || output->initialize() != FT_ERR_SUCCESS
        || ingredients.initialize() != FT_ERR_SUCCESS
        || input.initialize(17, 1, -1) != FT_ERR_SUCCESS
        || ingredients.push_back(input) != FT_ERR_SUCCESS
        || crafting.register_recipe(9, ft_move(ingredients), FT_TRUE) != FT_ERR_SUCCESS)
        return (1);
    output->set_item_id(42);
    output->set_max_stack(1);
    output->set_stack_size(1);
    game_inventory_snapshot snapshot = {};
    snapshot.next_slot = item_count;
    snapshot.current_weight = item_count;
    snapshot.used_slots = item_count;
    int32_t index = 0;
    while (index < item_count)
    {
        game_inventory_item_snapshot item = {};
        item.slot = index;
        item.item_id = 100 + index;
        if (index == item_count - 1)
            item.item_id = 17;
        item.max_stack = 1;
        item.stack_size = 1;
        item.width = 1;
        item.height = 1;
        snapshot.items.push_back(item);
        ++index;
    }
    std::chrono::steady_clock::duration elapsed{};
    uint32_t iteration = 0U;
    while (iteration < iterations)
    {
        if (inventory.restore_snapshot(snapshot) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = crafting.craft_item(inventory, 9, output);
        elapsed += std::chrono::steady_clock::now() - start;
        if (result != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    game_inventory_snapshot crafted = {};
    if (inventory.count_item(17) != 0 || inventory.count_item(42) != 1
        || inventory.get_snapshot(crafted) != FT_ERR_SUCCESS
        || crafted.items.size() != static_cast<ft_size_t>(item_count))
        return (1);
    uint64_t checksum = 0U;
    auto item = crafted.items.begin();
    while (item != crafted.items.end())
    {
        checksum = checksum * 31U + static_cast<uint64_t>(item->slot)
            + static_cast<uint64_t>(item->item_id);
        ++item;
    }
    std::printf("legacy_%" PRId32 " %.3f %" PRIu64 " %" PRId32 "\n", item_count,
        std::chrono::duration<double, std::micro>(elapsed).count() / iterations,
        checksum, crafted.current_weight);
    return (0);
}

static int32_t measure_output_query()
{
    game_crafting crafting;
    if (crafting.initialize() != FT_ERR_SUCCESS || register_recipe(crafting) != FT_ERR_SUCCESS)
        return (1);
    constexpr uint32_t ITERATIONS = 200000U;
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        game_crafting_output_snapshot output;
        if (crafting.query_recipe_output_snapshot(9, 3, &output) != FT_ERR_SUCCESS)
            return (1);
        checksum += static_cast<uint64_t>(output.primary.item_id) + output.primary.quantity;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != 45U * ITERATIONS)
        return (1);
    std::printf("output_query %.3f %" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--output-query") == 0)
        return (measure_output_query());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--query-snapshot") == 0)
    {
        if (measure_prepare(1, 4096U, FT_FALSE, FT_TRUE, FT_TRUE) != 0)
            return (1);
        return (measure_prepare(512, 4096U, FT_FALSE, FT_TRUE, FT_TRUE));
    }
    if (argument_count == 2 && ft_strcmp(arguments[1], "--query") == 0)
    {
        if (measure_prepare(1, 4096U, FT_FALSE, FT_TRUE) != 0)
            return (1);
        return (measure_prepare(16384, 256U, FT_FALSE, FT_TRUE));
    }
    if (argument_count == 2 && ft_strcmp(arguments[1], "--commit") == 0)
    {
        if (measure_prepare(1, 48U, FT_TRUE) != 0)
            return (1);
        return (measure_prepare(512, 48U, FT_TRUE));
    }
    if (argument_count == 2 && ft_strcmp(arguments[1], "--legacy") == 0)
    {
        if (measure_legacy(8, 48U) != 0)
            return (1);
        return (measure_legacy(64, 16U));
    }
    if (measure_prepare(1, 48U) != 0)
        return (1);
    return (measure_prepare(512, 12U));
}
