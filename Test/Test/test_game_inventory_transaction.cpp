#include "../test_internal.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Errno/errno.hpp"
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
