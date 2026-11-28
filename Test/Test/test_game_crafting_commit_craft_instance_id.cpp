#include "../test_internal.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t make_instance_id_test_item(int32_t item_id,
	ft_sharedptr<game_item> *item_out)
{
	int32_t result;

	if (item_out == nullptr)
		return (FT_ERR_INVALID_ARGUMENT);
	result = item_out->initialize(new game_item());
	if (result == FT_ERR_SUCCESS)
		result = item_out->get()->initialize();
	if (result == FT_ERR_SUCCESS)
	{
		item_out->get()->set_item_id(item_id);
		item_out->get()->set_stack_size(1);
		item_out->get()->set_max_stack(1);
		item_out->get()->set_width(1);
		item_out->get()->set_height(1);
	}
	return (result);
}

FT_TEST(test_game_crafting_commit_craft_assigns_host_instance_id)
{
	game_crafting crafting;
	game_inventory inventory;
	game_crafting_prepared prepared;
	ft_vector<game_crafting_ingredient> ingredients;
	ft_sharedptr<game_item> input;
	ft_sharedptr<game_item> output;
	ft_sharedptr<game_item> second_input;
	game_crafting_ingredient ingredient;
	game_inventory_snapshot snapshot = {};
	ft_bool found_output = FT_FALSE;

	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
	FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4U, 0));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
	FT_ASSERT_EQ(FT_ERR_SUCCESS, make_instance_id_test_item(31, &input));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, make_instance_id_test_item(32, &output));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, make_instance_id_test_item(31, &second_input));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(31, 1, -1));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.push_back(ingredient));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(9901,
		static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
		output, 1, FT_TRUE));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.prepare_craft(inventory, 9901, 1,
		&prepared));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.commit_craft(inventory, &prepared,
		UINT64_C(5001)));
	FT_ASSERT_EQ(1, inventory.count_item(32));
	FT_ASSERT_EQ(0, inventory.count_item(31));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_snapshot(snapshot));
	while (snapshot.items.size() > 0U)
	{
		const game_inventory_item_snapshot &entry = snapshot.items.back();

		if (entry.item_id == 32 && entry.instance_id == UINT64_C(5001))
			found_output = FT_TRUE;
		snapshot.items.pop_back();
	}
	FT_ASSERT_EQ(FT_TRUE, found_output);
	FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(second_input));
	FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.prepare_craft(inventory, 9901, 1,
		&prepared));
	FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, crafting.commit_craft(inventory,
		&prepared, UINT64_C(5001)));
	FT_ASSERT_EQ(1, inventory.count_item(31));
	FT_ASSERT_EQ(1, inventory.count_item(32));
	return (1);
}
