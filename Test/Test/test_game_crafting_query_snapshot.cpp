#include "../test_internal.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t snapshot_query_make_item(int32_t id, int32_t count,
	ft_sharedptr<game_item> *item)
{
	int32_t result = item->initialize(new game_item());
	if (result != FT_ERR_SUCCESS)
		return (result);
	result = item->get()->initialize();
	if (result != FT_ERR_SUCCESS)
		return (result);
	item->get()->set_item_id(id);
	item->get()->set_stack_size(count);
	item->get()->set_max_stack(64);
	item->get()->set_width(1);
	item->get()->set_height(1);
	return (FT_ERR_SUCCESS);
}

FT_TEST(test_game_crafting_snapshot_query_uses_captured_inventory_revision)
{
	game_crafting crafting;
	game_inventory inventory;
	ft_vector<game_crafting_ingredient> ingredients;
	ft_vector<game_crafting_missing_ingredient> missing;
	game_crafting_query_result query = {};
	game_inventory_snapshot snapshot = {};
	ft_sharedptr<game_item> input;
	ft_sharedptr<game_item> later_input;

	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
	FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(8, 0));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
	game_crafting_ingredient ingredient;
	FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(31, 2, -1));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.push_back(ingredient));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(601,
		static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients), FT_TRUE));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot_query_make_item(31, 1, &input));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_snapshot(snapshot));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, missing.initialize());

	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.query_recipe_at_station_snapshot(
		601, snapshot, 0, &query, &missing));
	FT_ASSERT_EQ(FT_FALSE, query.craftable);
	FT_ASSERT_EQ(1, query.missing_units);
	FT_ASSERT_EQ(1U, missing.size());
	FT_ASSERT_EQ(1, missing[0].available_units);

	FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot_query_make_item(31, 1, &later_input));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(later_input));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.query_recipe_at_station_snapshot(
		601, snapshot, 0, &query, &missing));
	FT_ASSERT_EQ(FT_FALSE, query.craftable);
	FT_ASSERT_EQ(1, query.missing_units);
	FT_ASSERT_EQ(2, inventory.count_item(31));

	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.query_recipe_at_station(
		601, inventory, 0, &query, &missing));
	FT_ASSERT_EQ(FT_TRUE, query.craftable);
	FT_ASSERT_EQ(0U, missing.size());
	return (1);
}

FT_TEST(test_game_crafting_snapshot_query_rejects_inconsistent_snapshot)
{
	game_crafting crafting;
	game_inventory_snapshot snapshot = {};
	ft_vector<game_crafting_missing_ingredient> missing;
	game_crafting_query_result query = {};

	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
	FT_ASSERT_EQ(FT_ERR_SUCCESS, missing.initialize());
	snapshot.capacity = 8U;
	snapshot.weight_limit = 0;
	snapshot.current_weight = 1;
	snapshot.next_slot = 1;
	game_inventory_item_snapshot item = {};
	item.slot = 0;
	item.item_id = 31;
	item.stack_size = 1;
	item.max_stack = 64;
	item.width = 1;
	item.height = 1;
	snapshot.items.push_back(item);

	FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
		crafting.query_recipe_at_station_snapshot(601, snapshot, 0,
			&query, &missing));
	FT_ASSERT_EQ(FT_FALSE, query.found);
	FT_ASSERT_EQ(0U, missing.size());
	return (1);
}

