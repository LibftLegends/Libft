#include "../test_internal.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/map.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include <atomic>
#include <thread>
#ifndef LIBFT_TEST_BUILD
#endif

int test_inventory_slots(void)
{
    game_inventory inventory;
    ft_sharedptr<game_item> bulky(new game_item());
    if (inventory.initialize(4, 0) != FT_ERR_SUCCESS)
        return (0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bulky->initialize());
    bulky->set_item_id(1);
    bulky->set_max_stack(1);
    bulky->set_stack_size(1);
    bulky->set_width(2);
    bulky->set_height(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(bulky));
    if (inventory.get_used() != 4)
        return (0);
    ft_sharedptr<game_item> small(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, small->initialize());
    small->set_item_id(2);
    small->set_max_stack(1);
    small->set_stack_size(1);
    FT_ASSERT_EQ(FT_ERR_FULL, inventory.add_item(small));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bulky->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, small->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    return (1);
}

int test_inventory_count(void)
{
    game_inventory inv;
    ft_sharedptr<game_item> potion(new game_item());
    if (inv.initialize(5, 0) != FT_ERR_SUCCESS)
        return (0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, potion->initialize());
    potion->set_item_id(1);
    potion->set_max_stack(10);
    potion->set_stack_size(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inv.add_item(potion));

    ft_sharedptr<game_item> more(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, more->initialize());
    more->set_item_id(1);
    more->set_max_stack(10);
    more->set_stack_size(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inv.add_item(more));

    FT_ASSERT_EQ(true, inv.has_item(1));
    FT_ASSERT_EQ(11, inv.count_item(1));
    FT_ASSERT_EQ(0, inv.count_item(2));
    FT_ASSERT_EQ(false, inv.has_item(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, potion->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, more->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inv.get_error());
    return (1);
}

int test_inventory_full(void)
{
    game_inventory inv;
    ft_sharedptr<game_item> item(new game_item());
    if (inv.initialize(1, 0) != FT_ERR_SUCCESS)
        return (0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    item->set_item_id(1);
    item->set_max_stack(5);
    item->set_stack_size(5);
    FT_ASSERT_EQ(false, inv.is_full());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inv.add_item(item));
    FT_ASSERT_EQ(true, inv.is_full());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inv.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->get_error());
    return (1);
}

FT_TEST(test_game_inventory_remove_clears_usage)
{
    game_inventory inventory;
    ft_sharedptr<game_item> shield(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shield->initialize());

    shield->set_item_id(7);
    shield->set_max_stack(3);
    shield->set_stack_size(2);
    shield->set_width(2);
    shield->set_height(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(shield));
    FT_ASSERT_EQ(static_cast<size_t>(4), inventory.get_used());
    FT_ASSERT_EQ(2, inventory.get_current_weight());

    inventory.remove_item(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    FT_ASSERT_EQ(static_cast<size_t>(0), inventory.get_used());
    FT_ASSERT_EQ(0, inventory.get_current_weight());
    FT_ASSERT_EQ(0, inventory.count_item(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shield->get_error());
    return (1);
}

FT_TEST(test_game_inventory_count_queries_include_sparse_out_of_order_slots)
{
    game_inventory inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize());
    game_inventory_snapshot snapshot = {};
    snapshot.next_slot = 32768;
    snapshot.used_slots = 2U;
    snapshot.current_weight = 5;
    game_inventory_item_snapshot item = {};
    item.slot = 30000;
    item.item_id = 17;
    item.rarity = 3;
    item.max_stack = 10;
    item.stack_size = 2;
    item.width = 1;
    item.height = 1;
    snapshot.items.push_back(item);
    item.slot = 7;
    item.rarity = 4;
    item.stack_size = 3;
    snapshot.items.push_back(item);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.restore_snapshot(snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.enable_thread_safety());
    FT_ASSERT_EQ(5, inventory.count_item(17));
    FT_ASSERT_EQ(2, inventory.count_rarity(3));
    FT_ASSERT_EQ(3, inventory.count_rarity(4));
    FT_ASSERT_EQ(FT_TRUE, inventory.has_item(17));
    FT_ASSERT_EQ(FT_TRUE, inventory.has_rarity(4));
    FT_ASSERT_EQ(FT_FALSE, inventory.has_item(99));
    FT_ASSERT_EQ(FT_FALSE, inventory.has_rarity(99));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    return (1);
}

FT_TEST(test_game_inventory_count_never_returns_partial_or_overflowed_total)
{
    game_inventory inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize());
    game_inventory_snapshot snapshot = {};
    snapshot.next_slot = 9;
    snapshot.used_slots = 2U;
    snapshot.current_weight = 2;
    game_inventory_item_snapshot item = {};
    item.slot = 8;
    item.item_id = 17;
    item.rarity = 4;
    item.max_stack = INT32_MAX;
    item.stack_size = 1;
    item.width = 1;
    item.height = 1;
    snapshot.items.push_back(item);
    item.slot = 2;
    snapshot.items.push_back(item);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.restore_snapshot(snapshot));
    auto &items = inventory.get_items();
    auto *first = items.find(8);
    auto *second = items.find(2);
    FT_ASSERT(first != items.end() && second != items.end());
    first->value->set_stack_size(INT32_MAX);
    FT_ASSERT_EQ(0, inventory.count_item(17));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, inventory.get_error());
    FT_ASSERT_EQ(FT_FALSE, inventory.has_rarity(4));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, inventory.get_error());
    first->value->set_stack_size(1);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, items.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t map_count = inventory.count_item(17);
    const int32_t map_error = inventory.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(0, map_count);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, map_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, items.disable_thread_safety());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->value->enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t item_count = inventory.count_rarity(4);
    const int32_t item_error = inventory.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(0, item_count);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, item_error);
    FT_ASSERT_EQ(2, inventory.count_rarity(4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    return (1);
}

FT_TEST(test_game_inventory_snapshot_reads_coherent_item_fields)
{
    game_inventory inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize());
    ft_sharedptr<game_item> source(new game_item());
    FT_ASSERT(source.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->initialize());
    source->set_item_id(17);
    source->set_max_stack(4);
    source->set_stack_size(1);
    source->set_modifier1_id(1);
    source->set_modifier1_value(10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(source));
    auto *stored = inventory.get_items().find(0);
    FT_ASSERT(stored != inventory.get_items().end());
    game_item &item = *stored->value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.enable_thread_safety());

    std::atomic<ft_bool> stop(FT_FALSE);
    std::atomic<ft_bool> wrote(FT_FALSE);
    std::atomic<int32_t> writer_error(FT_ERR_SUCCESS);
    std::thread writer([&]()
    {
        int32_t quantity = 2;
        while (stop.load() == FT_FALSE)
        {
            const int32_t error = item._mutex->lock();
            if (error != FT_ERR_SUCCESS)
            {
                writer_error.store(error);
                break ;
            }
            item.set_max_stack(quantity * 4);
            item.set_stack_size(quantity);
            item.set_modifier1_id(quantity);
            item.set_modifier1_value(quantity * 10);
            const int32_t unlock_error = item._mutex->unlock();
            if (unlock_error != FT_ERR_SUCCESS)
            {
                writer_error.store(unlock_error);
                break ;
            }
            wrote.store(FT_TRUE);
            quantity = 3 - quantity;
        }
        return ;
    });
    while (wrote.load() == FT_FALSE && writer_error.load() == FT_ERR_SUCCESS)
        std::this_thread::yield();
    ft_bool consistent = FT_TRUE;
    uint32_t attempt = 0U;
    while (attempt < 1024U && consistent == FT_TRUE)
    {
        game_inventory_snapshot snapshot = {};
        if (inventory.get_snapshot(snapshot) != FT_ERR_SUCCESS || snapshot.items.size() != 1U)
            consistent = FT_FALSE;
        else
        {
            const game_inventory_item_snapshot &captured = snapshot.items.front();
            consistent = captured.max_stack == captured.stack_size * 4
                && captured.modifier1_id == captured.stack_size
                && captured.modifier1_value == captured.stack_size * 10;
        }
        ++attempt;
    }
    stop.store(FT_TRUE);
    writer.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer_error.load());
    FT_ASSERT_EQ(FT_TRUE, consistent);
    return (1);
}

FT_TEST(test_game_inventory_restore_snapshot_validates_slots_and_instance_ids)
{
    game_inventory inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize());
    game_inventory_snapshot snapshot = {};
    snapshot.capacity = 48U;
    snapshot.next_slot = 48;
    snapshot.revision = 42U;
    int32_t index = 0;
    while (index < 24)
    {
        game_inventory_item_snapshot item = {};
        item.slot = 46 - 2 * index;
        item.item_id = 17;
        item.instance_id = UINT64_MAX - static_cast<uint64_t>(index);
        if (index % 3 == 0)
            item.instance_id = 0U;
        item.max_stack = 1;
        item.stack_size = 1;
        item.width = 1;
        item.height = 1;
        snapshot.items.push_back(item);
        ++snapshot.used_slots;
        ++snapshot.current_weight;
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.restore_snapshot(snapshot));
    const game_inventory_item_snapshot last_item = snapshot.items.back();
    snapshot.items.back().slot = snapshot.items.front().slot;
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, inventory.restore_snapshot(snapshot));
    snapshot.items.back() = last_item;
    snapshot.items.back().instance_id = snapshot.items[1].instance_id;
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, inventory.restore_snapshot(snapshot));
    snapshot.items.back() = last_item;

    snapshot.capacity = 0U;
    snapshot.used_slots = 0U;
    auto item = snapshot.items.begin();
    while (item != snapshot.items.end())
    {
        item->width = INT32_MAX;
        item->height = INT32_MAX;
        snapshot.used_slots += static_cast<ft_size_t>(INT32_MAX) * INT32_MAX;
        ++item;
    }
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, inventory.restore_snapshot(snapshot));
    game_inventory_snapshot restored = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_snapshot(restored));
    FT_ASSERT_EQ(42U, restored.revision);
    FT_ASSERT_EQ(24U, restored.items.size());
    FT_ASSERT_EQ(46, restored.items.front().slot);
    FT_ASSERT_EQ(last_item.slot, restored.items.back().slot);
    FT_ASSERT_EQ(last_item.instance_id, restored.items.back().instance_id);
    FT_ASSERT_EQ(1, restored.items.back().width);
    return (1);
}

FT_TEST(test_game_inventory_snapshot_is_value_only_and_revisioned)
{
    game_inventory inventory;
    ft_sharedptr<game_item> item(new game_item());
    std::vector<game_inventory_item_snapshot> snapshot;
    std::vector<game_inventory_item_snapshot> empty_snapshot;
    game_inventory_snapshot complete_snapshot;
    uint64_t snapshot_revision = 0U;
    uint64_t empty_revision = 0U;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    item->set_item_id(41);
    item->set_max_stack(16);
    item->set_stack_size(7);
    item->set_rarity(3);
    item->set_width(1);
    item->set_height(1);
    item->set_modifier2_id(8);
    item->set_modifier2_value(19);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        inventory.get_snapshot(snapshot, &snapshot_revision));
    FT_ASSERT_EQ(static_cast<size_t>(1), snapshot.size());
    FT_ASSERT_EQ(0, snapshot[0].slot);
    FT_ASSERT_EQ(41, snapshot[0].item_id);
    FT_ASSERT_EQ(7, snapshot[0].stack_size);
    FT_ASSERT_EQ(16, snapshot[0].max_stack);
    FT_ASSERT_EQ(3, snapshot[0].rarity);
    FT_ASSERT_EQ(1, snapshot[0].width);
    FT_ASSERT_EQ(1, snapshot[0].height);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_snapshot(complete_snapshot));
    FT_ASSERT_EQ(static_cast<ft_size_t>(4), complete_snapshot.capacity);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), complete_snapshot.used_slots);
    FT_ASSERT_EQ(0, complete_snapshot.weight_limit);
    FT_ASSERT_EQ(7, complete_snapshot.current_weight);
    FT_ASSERT_EQ(snapshot_revision, complete_snapshot.revision);
    FT_ASSERT_EQ(8, complete_snapshot.items[0].modifier2_id);
    FT_ASSERT_EQ(19, complete_snapshot.items[0].modifier2_value);

    inventory.remove_item(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        inventory.get_snapshot(empty_snapshot, &empty_revision));
    FT_ASSERT_EQ(static_cast<size_t>(0), empty_snapshot.size());
    FT_ASSERT_EQ(true, empty_revision > snapshot_revision);
    FT_ASSERT_EQ(static_cast<size_t>(1), snapshot.size());
    FT_ASSERT_EQ(7, snapshot[0].stack_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->get_error());
    return (1);
}

FT_TEST(test_game_item_instance_identity_survives_copy_and_transfer)
{
    game_inventory source;
    game_inventory destination;
    ft_sharedptr<game_item> unique_item(new game_item());
    ft_sharedptr<game_item> duplicate_item(new game_item());
    game_item copied_item;
    std::vector<game_inventory_item_snapshot> snapshot;
    uint64_t source_revision = 0U;
    uint64_t destination_revision = 0U;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, unique_item->initialize());
    unique_item->set_item_id(90);
    unique_item->set_instance_id(0x1122334455667788ULL);
    unique_item->set_max_stack(1);
    unique_item->set_stack_size(1);
    unique_item->set_width(1);
    unique_item->set_height(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_item.initialize(*unique_item));
    FT_ASSERT_EQ(0x1122334455667788ULL, copied_item.get_instance_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(unique_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.transfer_to(destination, 0, 1,
        source_revision, destination_revision));
    FT_ASSERT_EQ(0, source.count_item(90));
    FT_ASSERT_EQ(1, destination.count_item(90));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_snapshot(snapshot, &destination_revision));
    FT_ASSERT_EQ(static_cast<size_t>(1), snapshot.size());
    FT_ASSERT_EQ(0x1122334455667788ULL, snapshot[0].instance_id);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate_item->initialize());
    duplicate_item->set_item_id(90);
    duplicate_item->set_instance_id(0x1122334455667788ULL);
    duplicate_item->set_max_stack(1);
    duplicate_item->set_stack_size(1);
    duplicate_item->set_width(1);
    duplicate_item->set_height(1);
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS,
        destination.add_item(duplicate_item));
    FT_ASSERT_EQ(1, destination.count_item(90));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_item.get_error());
    return (1);
}

FT_TEST(test_game_inventory_merges_into_existing_stack)
{
    game_inventory inventory;
    ft_sharedptr<game_item> arrows(new game_item());
    ft_sharedptr<game_item> refill(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(3, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, arrows->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, refill->initialize());

    arrows->set_item_id(12);
    arrows->set_max_stack(5);
    arrows->set_stack_size(4);
    arrows->set_width(1);
    arrows->set_height(1);
    refill->set_item_id(12);
    refill->set_max_stack(5);
    refill->set_stack_size(4);
    refill->set_width(1);
    refill->set_height(1);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(arrows));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(refill));
    FT_ASSERT_EQ(8, inventory.count_item(12));
    Pair<int, ft_sharedptr<game_item> > *first_slot = inventory.get_items().find(0);
    Pair<int, ft_sharedptr<game_item> > *second_slot = inventory.get_items().find(1);
    FT_ASSERT_NEQ(first_slot, inventory.get_items().end());
    FT_ASSERT_NEQ(second_slot, inventory.get_items().end());
    FT_ASSERT_EQ(5, first_slot->value->get_stack_size());
    FT_ASSERT_EQ(3, second_slot->value->get_stack_size());
    FT_ASSERT_EQ(8, inventory.get_current_weight());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, arrows->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, refill->get_error());
    return (1);
}

FT_TEST(test_game_inventory_rarity_tracking)
{
    game_inventory inventory;
    ft_sharedptr<game_item> gem(new game_item());
    ft_sharedptr<game_item> ore(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(3, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gem->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ore->initialize());

    gem->set_item_id(10);
    gem->set_rarity(2);
    gem->set_max_stack(10);
    gem->set_stack_size(3);
    ore->set_item_id(11);
    ore->set_rarity(2);
    ore->set_max_stack(10);
    ore->set_stack_size(4);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(gem));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(ore));
    FT_ASSERT_EQ(7, inventory.count_rarity(2));
    FT_ASSERT_EQ(true, inventory.has_rarity(2));
    FT_ASSERT_EQ(false, inventory.has_rarity(5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gem->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ore->get_error());
    return (1);
}

FT_TEST(test_game_inventory_resize_updates_capacity)
{
    game_inventory inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));

    FT_ASSERT_EQ(static_cast<size_t>(1), inventory.get_capacity());
    inventory.resize(3);
    FT_ASSERT_EQ(static_cast<size_t>(3), inventory.get_capacity());
    return (1);
}

FT_TEST(test_game_inventory_add_item_respects_weight_limit)
{
    game_inventory inventory;
    ft_sharedptr<game_item> heavy(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(5, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, heavy->initialize());

    heavy->set_item_id(4);
    heavy->set_max_stack(10);
    heavy->set_stack_size(4);
    heavy->set_width(1);
    heavy->set_height(1);
    FT_ASSERT_EQ(FT_ERR_FULL, inventory.add_item(heavy));
    FT_ASSERT_EQ(FT_ERR_FULL, inventory.get_error());
    FT_ASSERT_EQ(0, inventory.get_current_weight());
    return (1);
}

FT_TEST(test_game_inventory_rejects_null_item)
{
    game_inventory inventory;
    ft_sharedptr<game_item> none;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(2, 0));

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, inventory.add_item(none));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, inventory.get_error());
    FT_ASSERT_EQ(static_cast<size_t>(0), inventory.get_used());
    FT_ASSERT_EQ(0, inventory.get_current_weight());
    return (1);
}

FT_TEST(test_game_inventory_splits_large_stack)
{
    game_inventory inventory;
    ft_sharedptr<game_item> arrows(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(5, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, arrows->initialize());

    arrows->set_item_id(9);
    arrows->set_max_stack(5);
    arrows->set_stack_size(7);
    arrows->set_width(1);
    arrows->set_height(1);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(arrows));
    FT_ASSERT_EQ(7, inventory.count_item(9));
    FT_ASSERT_EQ(static_cast<size_t>(2), inventory.get_used());
    FT_ASSERT_EQ(7, inventory.get_current_weight());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, arrows->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    return (1);
}

FT_TEST(test_game_inventory_full_addition_preserves_items)
{
    game_inventory inventory;
    ft_sharedptr<game_item> potion(new game_item());
    ft_sharedptr<game_item> elixir(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, potion->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, elixir->initialize());

    potion->set_item_id(3);
    potion->set_max_stack(2);
    potion->set_stack_size(1);
    potion->set_width(1);
    potion->set_height(1);
    elixir->set_item_id(4);
    elixir->set_max_stack(2);
    elixir->set_stack_size(1);
    elixir->set_width(1);
    elixir->set_height(1);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(potion));
    FT_ASSERT_EQ(FT_ERR_FULL, inventory.add_item(elixir));
    FT_ASSERT_EQ(FT_ERR_FULL, inventory.get_error());
    FT_ASSERT_EQ(static_cast<size_t>(1), inventory.get_used());
    FT_ASSERT_EQ(1, inventory.count_item(3));
    FT_ASSERT_EQ(0, inventory.count_item(4));
    FT_ASSERT_EQ(1, inventory.get_current_weight());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, potion->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, elixir->get_error());
    return (1);
}

FT_TEST(test_game_inventory_remove_missing_slot_noops)
{
    game_inventory inventory;
    ft_sharedptr<game_item> potion(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, potion->initialize());

    potion->set_item_id(6);
    potion->set_max_stack(3);
    potion->set_stack_size(2);
    potion->set_width(1);
    potion->set_height(1);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(potion));
    FT_ASSERT_EQ(2, inventory.get_current_weight());
    inventory.remove_item(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    Pair<int, ft_sharedptr<game_item> > *slot = inventory.get_items().find(0);
    FT_ASSERT_NEQ(slot, inventory.get_items().end());
    FT_ASSERT_EQ(2, slot->value->get_stack_size());
    FT_ASSERT_EQ(2, inventory.get_current_weight());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, potion->get_error());
    return (1);
}

FT_TEST(test_game_inventory_clear_items_advances_revision)
{
    game_inventory inventory;
    ft_sharedptr<game_item> item(new game_item());
    game_inventory_snapshot snapshot = {};
    uint64_t revision_before;
    uint64_t revision_after;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(3, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    item->set_item_id(12);
    item->set_max_stack(4);
    item->set_stack_size(3);
    item->set_width(1);
    item->set_height(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision_before));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.clear_items());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision_after));
    FT_ASSERT(revision_after > revision_before);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_snapshot(snapshot));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), snapshot.capacity);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), snapshot.used_slots);
    FT_ASSERT_EQ(0, snapshot.current_weight);
    FT_ASSERT_EQ(static_cast<size_t>(0), snapshot.items.size());
    FT_ASSERT_EQ(0, inventory.count_item(12));
    return (1);
}
