#include "../../../Modules/Game/game_inventory.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_capture(game_inventory &inventory,
    const game_inventory_snapshot &source, uint32_t iterations, ft_bool thread_safe,
    ft_bool lock_modifier)
{
    if (inventory.restore_snapshot(source) != FT_ERR_SUCCESS)
        return (1);
    if (thread_safe == FT_TRUE)
    {
        if (inventory.enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
        auto item = source.items.begin();
        while (item != source.items.end())
        {
            ft_sharedptr<game_item> *handle;
            if (inventory.copy_item_at_revision(item->slot, source.revision, &handle) != FT_ERR_SUCCESS)
                return (1);
            int32_t error = (*handle)->enable_thread_safety();
#ifdef LIBFT_TEST_BUILD
            if (error == FT_ERR_SUCCESS && lock_modifier == FT_TRUE)
                error = (*handle)->_modifier4.enable_thread_safety();
#endif
            delete handle;
            if (error != FT_ERR_SUCCESS)
                return (1);
            ++item;
        }
    }
    game_inventory_snapshot captured = {};
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (inventory.get_snapshot(captured) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (captured.revision != source.revision || captured.items.size() != source.items.size())
        return (1);
    uint64_t checksum = 0U;
    ft_size_t index = 0U;
    while (index < captured.items.size())
    {
        const auto &item = captured.items[index];
        if (item.slot != source.items[index].slot || item.instance_id != source.items[index].instance_id
            || item.item_id != 42 || item.stack_size != 1
            || item.modifier4_id != source.items[index].modifier4_id
            || item.modifier4_value != source.items[index].modifier4_value)
            return (1);
        checksum = checksum * 31U + item.instance_id;
        ++index;
    }
    const char *label = "unlocked";
    if (thread_safe == FT_TRUE)
        label = "locked";
    if (lock_modifier == FT_TRUE)
        label = "locked_modifier";
    std::printf("capture %s %.3f us/snapshot checksum=%" PRIu64 "\n",
        label, elapsed / iterations, checksum);
    return (0);
}

static int32_t measure_restore(int32_t item_count, uint32_t iterations, ft_bool capture,
    ft_bool lock_modifier = FT_FALSE)
{
    game_inventory inventory;
    if (inventory.initialize() != FT_ERR_SUCCESS)
        return (1);
    game_inventory_snapshot snapshot = {};
    snapshot.capacity = static_cast<ft_size_t>(item_count);
    snapshot.used_slots = snapshot.capacity;
    snapshot.current_weight = item_count;
    snapshot.next_slot = item_count;
    snapshot.revision = 33U;
    int32_t index = 0;
    while (index < item_count)
    {
        game_inventory_item_snapshot item = {};
        item.slot = index;
        item.item_id = 42;
        item.instance_id = UINT64_MAX - static_cast<uint64_t>(index);
        item.max_stack = 1;
        item.stack_size = 1;
        item.width = 1;
        item.height = 1;
        item.modifier4_id = 7;
        item.modifier4_value = -index;
        snapshot.items.push_back(item);
        ++index;
    }
    if (capture == FT_TRUE)
    {
        if (measure_capture(inventory, snapshot, iterations, FT_FALSE, FT_FALSE) != 0)
            return (1);
        return (measure_capture(inventory, snapshot, iterations, FT_TRUE, lock_modifier));
    }
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (inventory.restore_snapshot(snapshot) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    game_inventory_snapshot restored = {};
    if (inventory.get_snapshot(restored) != FT_ERR_SUCCESS
        || restored.revision != snapshot.revision
        || restored.items.size() != snapshot.items.size())
        return (1);
    uint64_t checksum = 0U;
    index = 0;
    while (index < item_count)
    {
        const game_inventory_item_snapshot &item = restored.items[index];
        if (item.slot != index || item.instance_id != snapshot.items[index].instance_id
            || item.item_id != 42 || item.stack_size != 1)
            return (1);
        checksum = checksum * 31U + item.instance_id;
        ++index;
    }
    std::printf("snapshot %" PRId32 " %.3f us/restore checksum=%" PRIu64 "\n",
        item_count, elapsed / iterations, checksum);
    return (0);
}

int main(int argument_count, char **arguments)
{
#ifdef LIBFT_TEST_BUILD
    if (argument_count == 2 && ft_strcmp(arguments[1], "--capture-modifier") == 0)
        return (measure_restore(256, 1024U, FT_TRUE, FT_TRUE));
#endif
    if (argument_count == 2 && ft_strcmp(arguments[1], "--capture") == 0)
        return (measure_restore(256, 1024U, FT_TRUE));
    if (measure_restore(16, 1024U, FT_FALSE) != 0
        || measure_restore(4096, 16U, FT_FALSE) != 0)
        return (1);
    return (0);
}
