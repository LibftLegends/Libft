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
