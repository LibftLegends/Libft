#include "../../../Modules/Game/game_world.hpp"
#include "../../../Modules/Game/game_character.hpp"
#include "../../../Modules/Game/game_inventory.hpp"
#include "../../../Modules/Game/game_item.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <new>

static int32_t benchmark_load(game_world &world, const ft_string &encoded,
    ft_size_t item_count, int32_t stack_size)
{
    constexpr uint32_t ITERATIONS = 16U;
    game_character character;
    game_inventory inventory;
    if (character.initialize() != FT_ERR_SUCCESS || inventory.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (world.load_from_buffer(encoded.c_str(), character, inventory) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    game_inventory_snapshot snapshot;
    if (inventory.get_snapshot(snapshot) != FT_ERR_SUCCESS || snapshot.items.size() != item_count)
        return (1);
    uint64_t checksum = 0U;
    ft_size_t index = 0U;
    while (index < snapshot.items.size())
    {
        const game_inventory_item_snapshot &item = snapshot.items[index];
        if (item.stack_size != stack_size || inventory.count_item(item.item_id) != stack_size)
            return (1);
        checksum = checksum * 31U + static_cast<uint32_t>(item.item_id);
        ++index;
    }
    std::printf("world buffer %.3f us/load items=%" PRIu64 " checksum=%" PRIu64 "\n",
        elapsed / ITERATIONS, static_cast<uint64_t>(index), checksum);
    return (0);
}

int main(int argument_count, char **arguments)
{
    constexpr int32_t ITEM_COUNT = 128;
    constexpr uint32_t ITERATIONS = 16U;
    int32_t stack_size = 1;
    const ft_bool load_stacks = argument_count == 2
        && ft_strcmp(arguments[1], "--load-stacks") == 0;
    if (load_stacks == FT_TRUE)
        stack_size = 2;
    game_world world;
    game_character character;
    game_inventory inventory;
    ft_string output;
    if (world.initialize() != FT_ERR_SUCCESS || character.initialize() != FT_ERR_SUCCESS
        || inventory.initialize(ITEM_COUNT, 0) != FT_ERR_SUCCESS
        || output.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t item_id = 1;
    while (item_id <= ITEM_COUNT)
    {
        ft_sharedptr<game_item> item(new (std::nothrow) game_item());
        if (!item || item->initialize() != FT_ERR_SUCCESS)
            return (1);
        item->set_item_id(item_id);
        item->set_max_stack(stack_size);
        item->set_stack_size(stack_size);
        if (inventory.add_item(item) != FT_ERR_SUCCESS)
            return (1);
        ++item_id;
    }
    uint32_t iteration = 0U;
    if (load_stacks == FT_TRUE
        || (argument_count == 2 && ft_strcmp(arguments[1], "--load") == 0))
    {
        if (world.save_to_buffer(output, character, inventory) != FT_ERR_SUCCESS)
            return (1);
        return (benchmark_load(world, output, ITEM_COUNT, stack_size));
    }
    const ft_bool fresh_buffer = argument_count == 2
        && ft_strcmp(arguments[1], "--fresh-buffer") == 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (fresh_buffer == FT_TRUE && output.destroy() != FT_ERR_SUCCESS)
            return (1);
        if (world.save_to_buffer(output, character, inventory) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    uint64_t checksum = 0U;
    ft_size_t index = 0U;
    const ft_size_t length = output.size();
    const char *data = output.c_str();
    while (index < length)
    {
        checksum = checksum * 31U + static_cast<uint8_t>(data[index]);
        ++index;
    }
    std::printf("world buffer %.3f us/save bytes=%" PRIu64 " checksum=%" PRIu64 "\n",
        elapsed / ITERATIONS, static_cast<uint64_t>(index), checksum);
    return (0);
}
