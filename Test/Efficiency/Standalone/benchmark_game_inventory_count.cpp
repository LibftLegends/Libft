#include "../../../Modules/Game/game_inventory.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_removal(game_inventory &inventory,
    const game_inventory_snapshot &snapshot, uint32_t iterations)
{
    constexpr int32_t REMOVED = 32;
    std::chrono::steady_clock::duration elapsed{};
    uint32_t iteration = 0U;
    while (iteration < iterations)
    {
        if (inventory.restore_snapshot(snapshot) != FT_ERR_SUCCESS)
            return (1);
        game_inventory_quantity_result result = {};
        const auto start = std::chrono::steady_clock::now();
        const int32_t error = inventory.remove_item_quantity(17, REMOVED,
            snapshot.revision, 1U, iteration + 1U, &result);
        elapsed += std::chrono::steady_clock::now() - start;
        if (error != FT_ERR_SUCCESS || result.removed_quantity != REMOVED
            || result.inventory_revision != snapshot.revision + 1U)
            return (1);
        ++iteration;
    }
    game_inventory_snapshot restored = {};
    if (inventory.get_snapshot(restored) != FT_ERR_SUCCESS
        || restored.items.size() != snapshot.items.size() - REMOVED
        || restored.current_weight != snapshot.current_weight - REMOVED)
        return (1);
    uint64_t checksum = 0U;
    auto item = restored.items.begin();
    while (item != restored.items.end())
    {
        checksum = checksum * 31U + static_cast<uint32_t>(item->slot);
        ++item;
    }
    const double time = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("remove %" PRId32 " %.3f us/request checksum=%" PRIu64 "\n",
        snapshot.next_slot, time / iterations, checksum);
    return (0);
}

static int32_t measure_count(int32_t stride, uint32_t iterations, ft_bool removal,
    ft_bool lock_map = FT_FALSE)
{
    constexpr int32_t ITEM_COUNT = 128;
    game_inventory inventory;
    if (inventory.initialize() != FT_ERR_SUCCESS)
        return (1);
    game_inventory_snapshot snapshot = {};
    snapshot.next_slot = ITEM_COUNT * stride;
    snapshot.used_slots = ITEM_COUNT;
    snapshot.current_weight = ITEM_COUNT;
    int32_t index = 0;
    while (index < ITEM_COUNT)
    {
        game_inventory_item_snapshot item = {};
        item.slot = index * stride;
        item.item_id = 17 + (index % 2);
        item.rarity = 4 + (index % 2);
        item.max_stack = 1;
        item.stack_size = 1;
        item.width = 1;
        item.height = 1;
        snapshot.items.push_back(item);
        ++index;
    }
    if (removal == FT_TRUE)
        return (measure_removal(inventory, snapshot, iterations));
    if (inventory.restore_snapshot(snapshot) != FT_ERR_SUCCESS)
        return (1);
    if (lock_map == FT_TRUE && inventory.get_items().enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        checksum += inventory.count_item(17);
        checksum += inventory.count_rarity(5);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != static_cast<uint64_t>(ITEM_COUNT) * iterations)
        return (1);
    const char *label = "count";
    if (lock_map == FT_TRUE)
        label = "count_locked";
    std::printf("%s %" PRId32 " %.3f us/query checksum=%" PRIu64 "\n",
        label, snapshot.next_slot, elapsed / (2U * iterations), checksum);
    return (0);
}

static int32_t measure_slot_erasure(ft_bool lock_map, ft_bool erase_tail)
{
    constexpr int32_t ITEMS = 128;
    constexpr uint32_t ITERATIONS = 128U;
    game_inventory inventory;
    if (inventory.initialize() != FT_ERR_SUCCESS)
        return (1);
    game_inventory_snapshot snapshot = {};
    snapshot.next_slot = ITEMS;
    snapshot.current_weight = ITEMS;
    snapshot.used_slots = ITEMS;
    int32_t slot = 0;
    while (slot < ITEMS)
    {
        game_inventory_item_snapshot item = {};
        item.slot = slot;
        item.item_id = 1;
        item.max_stack = 1;
        item.stack_size = 1;
        item.width = 1;
        item.height = 1;
        snapshot.items.push_back(item);
        ++slot;
    }
    std::chrono::steady_clock::duration elapsed{};
    uint64_t revision = 0U;
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        if (inventory.restore_snapshot(snapshot) != FT_ERR_SUCCESS)
            return (1);
        if (lock_map == FT_TRUE && inventory.get_items().enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        int32_t index = 0;
        while (index < ITEMS)
        {
            slot = index;
            if (erase_tail == FT_TRUE)
                slot = ITEMS - 1 - index;
            inventory.remove_item(slot);
            if (inventory.get_error() != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        elapsed += std::chrono::steady_clock::now() - start;
        if (inventory.get_revision(&revision) != FT_ERR_SUCCESS
            || revision != ITEMS || inventory.get_current_weight() != 0
            || !inventory.get_items().empty())
            return (1);
        ++iteration;
    }
    const double nanoseconds = std::chrono::duration<double, std::nano>(elapsed).count();
    std::printf("slot_%u_%u %.3f %" PRIu64 "\n", static_cast<uint32_t>(lock_map),
        static_cast<uint32_t>(erase_tail), nanoseconds / (ITEMS * ITERATIONS), revision);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--count-map") == 0)
    {
        if (measure_count(512, 8192U, FT_FALSE) != 0)
            return (1);
        return (measure_count(512, 8192U, FT_FALSE, FT_TRUE));
    }
    if (argument_count == 2 && ft_strcmp(arguments[1], "--remove-slot") == 0)
    {
        if (measure_slot_erasure(FT_FALSE, FT_FALSE) != 0
            || measure_slot_erasure(FT_TRUE, FT_FALSE) != 0)
            return (1);
        return (measure_slot_erasure(FT_TRUE, FT_TRUE));
    }
    const ft_bool removal = argument_count == 2
        && ft_strcmp(arguments[1], "--remove") == 0;
    if (measure_count(1, 512U, removal) != 0 || measure_count(512, 16U, removal) != 0)
        return (1);
    return (0);
}
