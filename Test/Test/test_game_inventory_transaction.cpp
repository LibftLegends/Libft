#include "../test_internal.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <functional>
#include <new>
#include <thread>

static int32_t make_transfer_item(int32_t id, int32_t count,
    int32_t max_stack, int32_t modifier_id, int32_t modifier_value,
    ft_sharedptr<game_item> *output)
{
    if (output == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t result = output->initialize(new (std::nothrow) game_item());
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = output->get()->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    output->get()->set_item_id(id);
    output->get()->set_stack_size(count);
    output->get()->set_max_stack(max_stack);
    output->get()->set_width(1);
    output->get()->set_height(1);
    output->get()->set_modifier1_id(modifier_id);
    output->get()->set_modifier1_value(modifier_value);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_game_inventory_move_transfers_slots_without_allocating)
{
    game_inventory source;
    game_inventory destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(32U, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(4U, 0));
    int32_t index = 0;
    while (index < 16)
    {
        ft_sharedptr<game_item> item;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(40 + index, 1, 1, 0, 0, &item));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(item));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    const auto *slots = source._items._data;
    uint64_t destination_revision;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT(destination._items._data == slots);
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    uint64_t actual_revision;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_revision(&actual_revision));
    FT_ASSERT_EQ(destination_revision + 2U, actual_revision);
    FT_ASSERT_EQ(32U, destination.get_capacity());
    index = 0;
    while (index < 16)
    {
        FT_ASSERT_EQ(1, destination.count_item(40 + index));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_used());
    return (1);
}

FT_TEST(test_game_inventory_move_leaves_source_on_map_lock_failure)
{
    game_inventory source;
    game_inventory destination;
    ft_sharedptr<game_item> item;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(4U, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(42, 1, 1, 0, 0, &item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source._items.enable_thread_safety());
    const auto *slots = source._items._data;
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT(source._items._data == slots);
    FT_ASSERT_EQ(1, source.count_item(42));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(1, destination.count_item(42));
    return (1);
}

FT_TEST(test_game_inventory_copy_reports_slot_allocation_failure_and_can_retry)
{
    game_inventory source;
    game_inventory destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(32U, 0));
    int32_t index = 0;
    while (index < 16)
    {
        ft_sharedptr<game_item> item;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(40 + index, 1, 1, 0, 0, &item));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(item));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    const auto *slots = source._items._data;
    const ft_size_t empty_table_bytes = destination._items._initial_capacity * sizeof(*slots);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(empty_table_bytes));
    const int32_t copy_error = destination.initialize(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, copy_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, destination.get_error());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT(destination._items._data == ft_nullptr);
    FT_ASSERT(source._items._data == slots);
    FT_ASSERT_EQ(16U, source.get_used());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT(destination._items._data != slots);
    FT_ASSERT_EQ(source.get_capacity(), destination.get_capacity());
    FT_ASSERT_EQ(16U, destination.get_used());
    index = 0;
    while (index < 16)
    {
        FT_ASSERT_EQ(1, destination.count_item(40 + index));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(1, destination.count_item(55));
    return (1);
}

FT_TEST(test_game_inventory_remove_item_instance_requires_identity_and_revision)
{
    game_inventory inventory;
    ft_sharedptr<game_item> item;
    uint64_t revision;
    uint64_t after_revision;
    uint64_t instance_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(40, 1, 1, 0, 0, &item));
    item->set_instance_id(7001U);
    instance_id = item->get_instance_id();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        inventory.remove_item_instance(0, revision, instance_id + 1U));
    FT_ASSERT_EQ(1, inventory.count_item(40));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&after_revision));
    FT_ASSERT_EQ(revision, after_revision);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        inventory.remove_item_instance(0, revision - 1U, instance_id));
    FT_ASSERT_EQ(1, inventory.count_item(40));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        inventory.remove_item_instance(0, revision, instance_id));
    FT_ASSERT_EQ(0, inventory.count_item(40));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&after_revision));
    FT_ASSERT_EQ(revision + 1U, after_revision);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND,
        inventory.remove_item_instance(0, after_revision, instance_id));
    return (1);
}

FT_TEST(test_game_inventory_slot_erasure_preserves_totals_on_lock_failure)
{
    game_inventory inventory;
    ft_sharedptr<game_item> item;
    ft_sharedptr<game_item> retained;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(40, 1, 1, 0, 0, &item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(41, 1, 1, 0, 0, &retained));
    item->set_instance_id(7001U);
    retained->set_instance_id(7002U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(retained));
    uint64_t revision = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision));
    auto &items = inventory.get_items();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, items.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    inventory.remove_item(0);
    const int32_t map_error = inventory.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, map_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, items.disable_thread_safety());

    auto *entry = items.find(0);
    FT_ASSERT(entry != items.end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry->value->enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t item_error = inventory.remove_item_instance(0, revision, 7001U);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, item_error);
    uint64_t revision_after = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision_after));
    FT_ASSERT_EQ(revision, revision_after);
    FT_ASSERT_EQ(2, inventory.get_current_weight());
    FT_ASSERT_EQ(2U, items.size());
#if USE_INVENTORY_SLOTS
    FT_ASSERT_EQ(2U, inventory.get_used());
#endif

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t retry_error = inventory.remove_item_instance(0, revision, 7001U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, retry_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision_after));
    FT_ASSERT_EQ(revision + 1U, revision_after);
    FT_ASSERT_EQ(1, inventory.get_current_weight());
    FT_ASSERT_EQ(1U, items.size());
    FT_ASSERT_EQ(7002U, items.at(1)->get_instance_id());
#if USE_INVENTORY_SLOTS
    FT_ASSERT_EQ(1U, inventory.get_used());
#endif
    return (1);
}

FT_TEST(test_game_inventory_revisioned_add_returns_unique_instance_slot)
{
    game_inventory inventory;
    ft_sharedptr<game_item> item;
    ft_sharedptr<game_item> duplicate;
    uint64_t revision;
    uint64_t after_revision;
    int32_t inserted_slot = -1;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(41, 1, 1, 0, 0, &item));
    item->set_instance_id(8001U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(41, 1, 1, 0, 0, &duplicate));
    duplicate->set_instance_id(8001U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, inventory.add_item_revisioned(item,
        revision + 1U, &inserted_slot));
    FT_ASSERT_EQ(-1, inserted_slot);
    FT_ASSERT_EQ(0, inventory.count_item(41));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item_revisioned(item,
        revision, &inserted_slot));
    FT_ASSERT_EQ(0, inserted_slot);
    FT_ASSERT_EQ(1, inventory.count_item(41));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&after_revision));
    FT_ASSERT_EQ(revision + 1U, after_revision);
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS,
        inventory.add_item_revisioned(duplicate, after_revision,
            &inserted_slot));
    FT_ASSERT_EQ(1, inventory.count_item(41));
    return (1);
}

FT_TEST(test_game_inventory_quantity_remove_is_atomic_and_idempotent)
{
    game_inventory inventory;
    game_inventory destination;
    ft_sharedptr<game_item> item;
    ft_sharedptr<game_item> later_item;
    game_inventory_quantity_result result = {};
    game_inventory_quantity_result retry = {};
    uint64_t revision;
    uint64_t current_revision;
    uint64_t destination_revision;
    game_inventory_transfer_result transfer_result = {};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(45, 5, 3, 0, 0, &item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));
    FT_ASSERT_EQ(5, inventory.count_item(45));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.remove_item_quantity(45, 4,
        revision, 12U, 1U, &result));
    FT_ASSERT_EQ(4, result.removed_quantity);
    FT_ASSERT_EQ(revision + 1U, result.inventory_revision);
    FT_ASSERT_EQ(1, inventory.count_item(45));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(45, 2, 3, 0, 0,
        &later_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(later_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.remove_item_quantity(45, 4,
        revision, 12U, 1U, &retry));
    FT_ASSERT_EQ(4, retry.removed_quantity);
    FT_ASSERT_EQ(3, inventory.count_item(45));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, inventory.remove_item_quantity(45,
        3, revision, 12U, 1U, &retry));
    FT_ASSERT_EQ(3, inventory.count_item(45));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&current_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, inventory.transfer_to(destination,
        1, 1, current_revision, destination_revision, 12U, 1U,
        &transfer_result));
    FT_ASSERT_EQ(3, inventory.count_item(45));
    FT_ASSERT_EQ(0, destination.count_item(45));
    return (1);
}

FT_TEST(test_game_inventory_quantity_remove_debits_lowest_sparse_slots_first)
{
    game_inventory inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize());
    game_inventory_snapshot snapshot = {};
    snapshot.next_slot = 20000;
    snapshot.used_slots = 6U;
    snapshot.current_weight = 14;
    snapshot.revision = 42U;
    constexpr int32_t SLOTS[] = {9000, 50, 100, 3, 40};
    constexpr int32_t STACKS[] = {4, 2, 5, 2, 1};
    ft_size_t index = 0U;
    while (index < sizeof(SLOTS) / sizeof(SLOTS[0]))
    {
        game_inventory_item_snapshot item = {};
        item.slot = SLOTS[index];
        item.item_id = 45;
        item.max_stack = STACKS[index];
        item.stack_size = STACKS[index];
        item.width = 1;
        item.height = 1;
        if (item.slot == 3)
            item.width = 2;
        if (item.slot == 40)
            item.instance_id = UINT64_MAX;
        snapshot.items.push_back(item);
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.restore_snapshot(snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.enable_thread_safety());
    game_inventory_quantity_result result = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.remove_item_quantity(45, 6,
        snapshot.revision, 12U, 9U, &result));
    FT_ASSERT_EQ(6, result.removed_quantity);
    FT_ASSERT_EQ(43U, result.inventory_revision);
    game_inventory_snapshot restored = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_snapshot(restored));
    FT_ASSERT_EQ(8, restored.current_weight);
    FT_ASSERT_EQ(3U, restored.used_slots);
    FT_ASSERT_EQ(3U, restored.items.size());
    FT_ASSERT_EQ(9000, restored.items[0].slot);
    FT_ASSERT_EQ(4, restored.items[0].stack_size);
    FT_ASSERT_EQ(100, restored.items[1].slot);
    FT_ASSERT_EQ(3, restored.items[1].stack_size);
    FT_ASSERT_EQ(40, restored.items[2].slot);
    FT_ASSERT_EQ(UINT64_MAX, restored.items[2].instance_id);
    return (1);
}

FT_TEST(test_game_inventory_quantity_remove_plan_failure_is_atomic_and_cached)
{
    game_inventory inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize());
    game_inventory_snapshot snapshot = {};
    snapshot.next_slot = 16;
    snapshot.used_slots = 16U;
    snapshot.current_weight = 16;
    snapshot.revision = 42U;
    int32_t slot = 0;
    while (slot < snapshot.next_slot)
    {
        game_inventory_item_snapshot item = {};
        item.slot = slot;
        item.item_id = 46;
        item.max_stack = 1;
        item.stack_size = 1;
        item.width = 1;
        item.height = 1;
        snapshot.items.push_back(item);
        ++slot;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.restore_snapshot(snapshot));
    game_inventory_quantity_result result = {};
    // Allow the cache entry to allocate while rejecting growth of the removal plan.
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(256U));
    const int32_t error = inventory.remove_item_quantity(46, 12, 42U, 12U, 1U, &result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error);
    FT_ASSERT_EQ(0, result.removed_quantity);
    FT_ASSERT_EQ(42U, result.inventory_revision);
    FT_ASSERT_EQ(16, inventory.count_item(46));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY,
        inventory.remove_item_quantity(46, 12, 42U, 12U, 1U, &result));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        inventory.remove_item_quantity(46, 12, 42U, 12U, 2U, &result));
    FT_ASSERT_EQ(4, inventory.count_item(46));
    return (1);
}

FT_TEST(test_game_inventory_quantity_remove_failure_does_not_partially_debit)
{
    game_inventory inventory;
    ft_sharedptr<game_item> item;
    ft_sharedptr<game_item> later_item;
    game_inventory_quantity_result result = {};
    uint64_t revision;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(46, 2, 3, 0, 0, &later_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(later_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, inventory.remove_item_quantity(46, 3,
        revision, 12U, 2U, &result));
    FT_ASSERT_EQ(0, result.removed_quantity);
    FT_ASSERT_EQ(revision, result.inventory_revision);
    FT_ASSERT_EQ(2, inventory.count_item(46));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(46, 2, 3, 0, 0, &item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, inventory.remove_item_quantity(46, 3,
        revision, 12U, 2U, &result));
    FT_ASSERT_EQ(4, inventory.count_item(46));
    return (1);
}

FT_TEST(test_game_inventory_quantity_remove_rejects_inconsistent_accounting)
{
    game_inventory inventory;
    ft_sharedptr<game_item> item;
    game_inventory_quantity_result result = {};
    uint64_t revision;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(47, 2, 3, 0, 0, &item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision));
    inventory.set_current_weight(1);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, inventory.remove_item_quantity(47,
        2, revision, 12U, 3U, &result));
    FT_ASSERT_EQ(0, result.removed_quantity);
    FT_ASSERT_EQ(2, inventory.count_item(47));
    FT_ASSERT_EQ(1, inventory.get_current_weight());
    return (1);
}

FT_TEST(test_game_inventory_transfer_splits_and_merges_preserving_metadata)
{
    game_inventory source;
    game_inventory destination;
    ft_sharedptr<game_item> source_item;
    ft_sharedptr<game_item> destination_item;
    uint64_t source_revision;
    uint64_t destination_revision;
    Pair<int32_t, ft_sharedptr<game_item> > *merged;
    Pair<int32_t, ft_sharedptr<game_item> > *split;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(41, 7, 4, 8, 99, &source_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(41, 2, 4, 8, 99, &destination_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(source_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.add_item(destination_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.transfer_to(destination, 0, 4,
        source_revision, destination_revision));
    FT_ASSERT_EQ(3, source.count_item(41));
    FT_ASSERT_EQ(6, destination.count_item(41));
    merged = destination.get_items().find(0);
    split = destination.get_items().find(1);
    FT_ASSERT_NEQ(destination.get_items().end(), merged);
    FT_ASSERT_NEQ(destination.get_items().end(), split);
    FT_ASSERT_EQ(4, merged->value->get_stack_size());
    FT_ASSERT_EQ(2, split->value->get_stack_size());
    FT_ASSERT_EQ(8, split->value->get_modifier1_id());
    FT_ASSERT_EQ(99, split->value->get_modifier1_value());
    return (1);
}

FT_TEST(test_game_inventory_transfer_keeps_previous_item_handles_detached)
{
    game_inventory source;
    game_inventory destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(8U, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(8U, 0));
    ft_sharedptr<game_item> moving;
    ft_sharedptr<game_item> unrelated;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(7, 2, 4, 0, 0, &moving));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(8, 1, 4, 0, 0, &unrelated));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(moving));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(unrelated));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.add_item(unrelated));
    uint64_t source_revision;
    uint64_t destination_revision;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_revision(&destination_revision));
    ft_sharedptr<game_item> *old_source;
    ft_sharedptr<game_item> *old_destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.copy_item_at_revision(1, source_revision, &old_source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.copy_item_at_revision(0, destination_revision, &old_destination));
    const int32_t transferred = source.transfer_to(destination, 0, 1,
        source_revision, destination_revision);
    (*old_source)->set_stack_size(3);
    (*old_destination)->set_stack_size(4);
    delete old_source;
    delete old_destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transferred);
    FT_ASSERT_EQ(1, source.count_item(8));
    FT_ASSERT_EQ(1, destination.count_item(8));
    FT_ASSERT_EQ(1, source.count_item(7));
    FT_ASSERT_EQ(1, destination.count_item(7));
    return (1);
}

FT_TEST(test_game_item_stack_arithmetic_handles_integer_limits)
{
    game_item item;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.initialize());
    item.set_max_stack(INT32_MAX);
    item.set_stack_size(INT32_MAX - 1);
    item.add_to_stack(INT32_MAX);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.get_error());
    FT_ASSERT_EQ(INT32_MAX, item.get_stack_size());
    item.set_stack_size(INT32_MIN);
    item.add_to_stack(-1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, item.get_error());
    FT_ASSERT_EQ(INT32_MIN, item.get_stack_size());
    item.sub_from_stack(INT32_MAX);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.get_error());
    FT_ASSERT_EQ(0, item.get_stack_size());
    item.sub_from_stack(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, item.get_error());
    FT_ASSERT_EQ(0, item.get_stack_size());
    return (1);
}

FT_TEST(test_game_inventory_stack_merge_checks_weight_before_mutating)
{
    game_inventory inventory;
    ft_sharedptr<game_item> item;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4U, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(18, 1, 4, 0, 0, &item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));
    inventory.set_current_weight(INT32_MAX);
    uint64_t revision = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, inventory.restore_serialized_item(item));
    FT_ASSERT_EQ(INT32_MAX, inventory.get_current_weight());
    FT_ASSERT_EQ(1, inventory.count_item(18));
    uint64_t revision_after = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision_after));
    FT_ASSERT_EQ(revision, revision_after);
    return (1);
}

FT_TEST(test_game_item_snapshot_preserves_output_on_modifier_lock_failure)
{
    game_inventory inventory;
    ft_sharedptr<game_item> item;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4U, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(18, 1, 4, 0, 0, &item));
    item->set_modifier4_id(9);
    item->set_modifier4_value(-71);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));
    auto &items = inventory.get_items();
    auto *stored = items.find(0);
    FT_ASSERT(stored != items.end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored->value->_modifier4.enable_thread_safety());

    game_inventory_item_snapshot item_snapshot = {};
    item_snapshot.slot = 91;
    item_snapshot.item_id = 82;
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = stored->value->capture_inventory_fields(0, item_snapshot);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(91, item_snapshot.slot);
    FT_ASSERT_EQ(82, item_snapshot.item_id);
    game_inventory_snapshot snapshot = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_snapshot(snapshot));
    FT_ASSERT_EQ(1U, snapshot.items.size());
    FT_ASSERT_EQ(18, snapshot.items[0].item_id);
    FT_ASSERT_EQ(9, snapshot.items[0].modifier4_id);
    FT_ASSERT_EQ(-71, snapshot.items[0].modifier4_value);
    return (1);
}

FT_TEST(test_game_inventory_stack_comparison_propagates_lock_failure)
{
    game_inventory inventory;
    ft_sharedptr<game_item> stored;
    ft_sharedptr<game_item> incoming;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4U, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(18, 1, 4, 3, 71, &stored));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(18, 1, 4, 3, 71, &incoming));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(stored));
    auto *entry = inventory.get_items().find(0);
    FT_ASSERT(entry != inventory.get_items().end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry->value->_modifier1.enable_thread_safety());
    uint64_t revision = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision));
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = inventory.restore_serialized_item(incoming);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    uint64_t revision_after = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&revision_after));
    FT_ASSERT_EQ(revision, revision_after);
    FT_ASSERT_EQ(1, inventory.get_current_weight());
    FT_ASSERT_EQ(1, inventory.count_item(18));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.restore_serialized_item(incoming));
    FT_ASSERT_EQ(2, inventory.count_item(18));
    FT_ASSERT_EQ(1U, inventory.get_items().size());
    return (1);
}

FT_TEST(test_game_inventory_full_transfer_reuses_only_detached_item)
{
    game_inventory source;
    game_inventory destination;
    ft_sharedptr<game_item> item;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(4U, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(4U, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(18, 1, 1, 3, 71, &item));
    item->set_instance_id(9044U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(item));
    uint64_t source_revision = 0U;
    uint64_t destination_revision = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_revision(&destination_revision));
    ft_sharedptr<game_item> *old_item = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.copy_item_at_revision(0, source_revision, &old_item));
    const int32_t result = source.transfer_to(destination, 0, 1,
        source_revision, destination_revision);
    (*old_item)->set_modifier1_value(999);
    delete old_item;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    game_inventory_snapshot snapshot = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_snapshot(snapshot));
    FT_ASSERT_EQ(1U, snapshot.items.size());
    FT_ASSERT_EQ(9044U, snapshot.items[0].instance_id);
    FT_ASSERT_EQ(71, snapshot.items[0].modifier1_value);
    FT_ASSERT_EQ(0, source.count_item(18));
    FT_ASSERT_EQ(1, destination.count_item(18));
    FT_ASSERT_EQ(0, source.get_current_weight());
    FT_ASSERT_EQ(1, destination.get_current_weight());
    return (1);
}

FT_TEST(test_game_inventory_transfer_keeps_distinct_item_metadata_separate)
{
    game_inventory source;
    game_inventory destination;
    ft_sharedptr<game_item> source_item;
    ft_sharedptr<game_item> destination_item;
    uint64_t source_revision;
    uint64_t destination_revision;
    Pair<int32_t, ft_sharedptr<game_item> > *original;
    Pair<int32_t, ft_sharedptr<game_item> > *transferred;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(45, 2, 8, 3, 10, &source_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(45, 2, 8, 3, 11, &destination_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(source_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.add_item(destination_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.transfer_to(destination, 0, 2,
        source_revision, destination_revision));
    original = destination.get_items().find(0);
    transferred = destination.get_items().find(1);
    FT_ASSERT_NEQ(destination.get_items().end(), original);
    FT_ASSERT_NEQ(destination.get_items().end(), transferred);
    FT_ASSERT_EQ(11, original->value->get_modifier1_value());
    FT_ASSERT_EQ(10, transferred->value->get_modifier1_value());
    FT_ASSERT_EQ(2, original->value->get_stack_size());
    FT_ASSERT_EQ(2, transferred->value->get_stack_size());
    return (1);
}

FT_TEST(test_game_inventory_transfer_full_destination_rolls_back_both)
{
    game_inventory source;
    game_inventory destination;
    ft_sharedptr<game_item> source_item;
    ft_sharedptr<game_item> blocking_item;
    uint64_t source_revision;
    uint64_t destination_revision;
    uint64_t source_before;
    uint64_t destination_before;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(3, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(51, 3, 8, 0, 0, &source_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(52, 1, 8, 0, 0, &blocking_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(source_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.add_item(blocking_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    source_before = source_revision;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_revision(&destination_revision));
    destination_before = destination_revision;
    FT_ASSERT_EQ(FT_ERR_FULL, source.transfer_to(destination, 0, 2,
        source_revision, destination_revision));
    FT_ASSERT_EQ(3, source.count_item(51));
    FT_ASSERT_EQ(0, destination.count_item(51));
    FT_ASSERT_EQ(1, destination.count_item(52));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(source_before, source_revision);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(destination_before, destination_revision);
    return (1);
}

FT_TEST(test_game_inventory_transfer_weight_limit_rolls_back_both)
{
    game_inventory source;
    game_inventory destination;
    ft_sharedptr<game_item> item;
    uint64_t source_revision;
    uint64_t destination_revision;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(4, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(55, 4, 8, 0, 0, &item));
    item->set_width(2);
    item->set_height(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(FT_ERR_FULL, source.transfer_to(destination, 0, 2,
        source_revision, destination_revision));
    FT_ASSERT_EQ(4, source.count_item(55));
    FT_ASSERT_EQ(0, destination.count_item(55));
    FT_ASSERT_EQ(0, destination.get_current_weight());
    return (1);
}

FT_TEST(test_game_inventory_transfer_rejects_stale_revisions)
{
    game_inventory source;
    game_inventory destination;
    ft_sharedptr<game_item> item;
    ft_sharedptr<game_item> second_item;
    uint64_t source_revision;
    uint64_t destination_revision;
    uint64_t stale_destination_revision;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(61, 5, 8, 0, 0, &item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_revision(&destination_revision));
    source.remove_item(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, source.transfer_to(destination, 0, 1,
        source_revision, destination_revision));
    FT_ASSERT_EQ(0, source.count_item(61));
    FT_ASSERT_EQ(0, destination.count_item(61));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(62, 2, 8, 0, 0, &second_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(second_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_revision(&stale_destination_revision));
    destination.resize(5);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, source.transfer_to(destination, 1, 1,
        source_revision, stale_destination_revision));
    FT_ASSERT_EQ(2, source.count_item(62));
    FT_ASSERT_EQ(0, destination.count_item(62));
    return (1);
}

FT_TEST(test_game_inventory_transfer_argument_and_missing_slot_semantics)
{
    game_inventory source;
    game_inventory destination;
    uint64_t source_revision;
    uint64_t destination_revision;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        source.transfer_to(destination, 0, 0, source_revision,
            destination_revision));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        source.transfer_to(destination, 0, -1, source_revision,
            destination_revision));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND,
        source.transfer_to(destination, 0, 1, source_revision,
            destination_revision));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        source.transfer_to(source, 0, 1, source_revision, source_revision));
    return (1);
}

static void game_inventory_transfer_worker(game_inventory &source,
    game_inventory &destination, int32_t source_slot, int32_t iterations,
    std::atomic<int32_t> &failures)
{
    int32_t iteration = 0;

    while (iteration < iterations)
    {
        uint64_t source_revision;
        uint64_t destination_revision;
        int32_t result = source.get_revision(&source_revision);

        if (result == FT_ERR_SUCCESS)
            result = destination.get_revision(&destination_revision);
        if (result == FT_ERR_SUCCESS)
            result = source.transfer_to(destination, source_slot, 1,
                source_revision, destination_revision);
        if (result == FT_ERR_SUCCESS)
            iteration += 1;
        else if (result != FT_ERR_INVALID_STATE)
        {
            failures.fetch_add(1, std::memory_order_relaxed);
            return ;
        }
    }
    return ;
}

FT_TEST(test_game_inventory_transfer_thread_safe_reverse_direction)
{
    game_inventory first;
    game_inventory second;
    ft_sharedptr<game_item> first_stack;
    ft_sharedptr<game_item> second_stack;
    std::atomic<int32_t> failures(0);
    std::thread forward;
    std::thread reverse;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(8, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(8, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(71, 1000, 2000, 0, 0, &first_stack));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_transfer_item(72, 1000, 2000, 0, 0, &second_stack));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.add_item(first_stack));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.add_item(second_stack));
    forward = std::thread(game_inventory_transfer_worker,
        std::ref(first), std::ref(second), 0, 200, std::ref(failures));
    reverse = std::thread(game_inventory_transfer_worker,
        std::ref(second), std::ref(first), 0, 200, std::ref(failures));
    forward.join();
    reverse.join();
    FT_ASSERT_EQ(0, failures.load(std::memory_order_relaxed));
    FT_ASSERT_EQ(1000, first.count_item(71) + second.count_item(71));
    FT_ASSERT_EQ(1000, first.count_item(72) + second.count_item(72));
    return (1);
}

FT_TEST(test_game_inventory_transfer_transaction_id_replays_exact_result)
{
    game_inventory source;
    game_inventory destination;
    ft_sharedptr<game_item> item;
    uint64_t source_revision;
    uint64_t destination_revision;
    uint64_t source_after;
    uint64_t destination_after;
    game_inventory_transfer_result first_result;
    game_inventory_transfer_result replay_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(81, 5, 8, 0, 0, &item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.transfer_to(destination, 0, 2,
        source_revision, destination_revision, 9001U, &first_result));
    FT_ASSERT_EQ(9001, first_result.transaction_id);
    FT_ASSERT_EQ(2, first_result.transferred_quantity);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_result.result_code);
    FT_ASSERT_EQ(3, source.count_item(81));
    FT_ASSERT_EQ(2, destination.count_item(81));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_after));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_revision(&destination_after));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.transfer_to(destination, 0, 2,
        source_revision, destination_revision, 9001U, &replay_result));
    FT_ASSERT_EQ(first_result.transaction_id, replay_result.transaction_id);
    FT_ASSERT_EQ(first_result.source_revision, replay_result.source_revision);
    FT_ASSERT_EQ(first_result.destination_revision, replay_result.destination_revision);
    FT_ASSERT_EQ(first_result.transferred_quantity, replay_result.transferred_quantity);
    FT_ASSERT_EQ(first_result.result_code, replay_result.result_code);
    FT_ASSERT_EQ(3, source.count_item(81));
    FT_ASSERT_EQ(2, destination.count_item(81));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(source_after, source_revision);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(destination_after, destination_revision);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, source.transfer_to(destination, 0, 1,
        source_after, destination_after, 9001U, &replay_result));
    FT_ASSERT_EQ(3, source.count_item(81));
    FT_ASSERT_EQ(2, destination.count_item(81));
    return (1);
}

FT_TEST(test_game_inventory_transfer_transaction_id_replays_failure)
{
    game_inventory source;
    game_inventory destination;
    uint64_t source_revision;
    uint64_t destination_revision;
    game_inventory_transfer_result first_result;
    game_inventory_transfer_result replay_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_revision(&destination_revision));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, source.transfer_to(destination, 0, 1,
        source_revision, destination_revision, 9002U, &first_result));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, first_result.result_code);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, source.transfer_to(destination, 0, 1,
        source_revision, destination_revision, 9002U, &replay_result));
    FT_ASSERT_EQ(first_result.result_code, replay_result.result_code);
    FT_ASSERT_EQ(first_result.source_revision, replay_result.source_revision);
    FT_ASSERT_EQ(first_result.destination_revision, replay_result.destination_revision);
    return (1);
}

FT_TEST(test_game_inventory_transfer_transaction_cache_fails_closed_at_capacity)
{
    game_inventory source;
    game_inventory destination;
    uint64_t source_revision;
    uint64_t destination_revision;
    game_inventory_transfer_result transfer_result;
    uint64_t transaction_id = 1U;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_revision(&destination_revision));
    while (transaction_id <= 4096U)
    {
        FT_ASSERT_EQ(FT_ERR_NOT_FOUND, source.transfer_to(destination, 0, 1,
            source_revision, destination_revision, transaction_id,
            &transfer_result));
        FT_ASSERT_EQ(FT_ERR_NOT_FOUND, transfer_result.result_code);
        transaction_id += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_FULL, source.transfer_to(destination, 0, 1,
        source_revision, destination_revision, transaction_id,
        &transfer_result));
    FT_ASSERT_EQ(FT_ERR_FULL, transfer_result.result_code);
    FT_ASSERT_EQ(0, source.count_item(81));
    FT_ASSERT_EQ(0, destination.count_item(81));
    return (1);
}

FT_TEST(test_game_inventory_transfer_transaction_ids_are_scope_local)
{
    game_inventory source;
    game_inventory first_destination;
    game_inventory second_destination;
    ft_sharedptr<game_item> item;
    uint64_t source_revision;
    uint64_t first_revision;
    uint64_t second_revision;
    game_inventory_transfer_result first_result;
    game_inventory_transfer_result second_result;
    game_inventory_transfer_result replay_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_destination.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_destination.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_transfer_item(82, 4, 8, 0, 0, &item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_destination.get_revision(&first_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.transfer_to(first_destination, 0, 1,
        source_revision, first_revision, 55U, 7001U, &first_result));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&source_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_destination.get_revision(&second_revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.transfer_to(second_destination, 0, 1,
        source_revision, second_revision, 66U, 7001U, &second_result));
    FT_ASSERT_EQ(55, first_result.transaction_scope_id);
    FT_ASSERT_EQ(66, second_result.transaction_scope_id);
    FT_ASSERT_EQ(7001, first_result.transaction_id);
    FT_ASSERT_EQ(7001, second_result.transaction_id);
    FT_ASSERT_EQ(1, first_destination.count_item(82));
    FT_ASSERT_EQ(1, second_destination.count_item(82));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.transfer_to(first_destination, 0, 1,
        first_result.source_revision - 1U, first_revision, 55U, 7001U,
        &replay_result));
    FT_ASSERT_EQ(first_result.source_revision, replay_result.source_revision);
    FT_ASSERT_EQ(2, source.count_item(82));
    return (1);
}
