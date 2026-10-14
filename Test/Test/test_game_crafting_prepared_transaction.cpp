#include "../test_internal.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t make_prepared_craft_item(int32_t item_id, int32_t count,
    ft_sharedptr<game_item> *item)
{
    int32_t result;

    if (item == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = item->initialize(new game_item());
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = item->get()->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    item->get()->set_item_id(item_id);
    item->get()->set_max_stack(64);
    item->get()->set_stack_size(count);
    item->get()->set_width(1);
    item->get()->set_height(1);
    return (FT_ERR_SUCCESS);
}

static int32_t append_prepared_ingredient(
    ft_vector<game_crafting_ingredient> &ingredients, int32_t item_id,
    int32_t count, int32_t rarity)
{
    game_crafting_ingredient ingredient;
    int32_t result = ingredient.initialize(item_id, count, rarity);

    if (result != FT_ERR_SUCCESS)
        return (result);
    return (ingredients.push_back(ingredient));
}

static int32_t register_prepared_recipe(game_crafting &crafting,
    int32_t recipe_id, int32_t input_id, int32_t input_count,
    int32_t output_id, int32_t output_count, ft_bool unlocked)
{
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> output;
    int32_t result = ingredients.initialize();

    if (result == FT_ERR_SUCCESS)
        result = append_prepared_ingredient(ingredients, input_id,
            input_count, -1);
    if (result == FT_ERR_SUCCESS)
        result = make_prepared_craft_item(output_id, 1, &output);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (crafting.register_recipe(recipe_id,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        output, output_count, unlocked));
}

FT_TEST(test_prepared_craft_missing_recipe_does_not_change_inventory)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_sharedptr<game_item> input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 3, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND,
        crafting.prepare_craft(inventory, 900, 1, &prepared));
    FT_ASSERT_EQ(3, inventory.count_item(7));
    FT_ASSERT_EQ(FT_FALSE, prepared.is_ready());
    return (1);
}

FT_TEST(test_prepared_craft_locked_recipe_does_not_change_inventory)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_sharedptr<game_item> input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 51, 7,
        1, 8, 1, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 2, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        crafting.prepare_craft(inventory, 51, 1, &prepared));
    FT_ASSERT_EQ(2, inventory.count_item(7));
    return (1);
}

FT_TEST(test_prepared_craft_output_full_rolls_back_all_inputs)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_sharedptr<game_item> input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 52, 7,
        1, 8, 65, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 1, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_FULL,
        crafting.prepare_craft(inventory, 52, 1, &prepared));
    FT_ASSERT_EQ(1, inventory.count_item(7));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    return (1);
}

FT_TEST(test_prepared_processing_consumes_inputs_without_reserving_output_space)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared normal_prepared;
    game_crafting_prepared processing_prepared;
    game_crafting_output_snapshot output = {};
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> pending_output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 57, 7,
        1, 8, 1, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 2, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_FULL,
        crafting.prepare_craft(inventory, 57, 1, &normal_prepared));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.prepare_processing_at_station(
        inventory, 57, 0, 1, &processing_prepared, &output));
    FT_ASSERT_EQ(8, output.primary.item_id);
    FT_ASSERT_EQ(1, output.primary.quantity);
    FT_ASSERT_EQ(64, output.primary.max_stack);
    FT_ASSERT_EQ(0, output.byproduct_count);
    FT_ASSERT_EQ(2, inventory.count_item(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        crafting.commit_processing_inputs(inventory, &processing_prepared));
    FT_ASSERT_EQ(1, inventory.count_item(7));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        make_prepared_craft_item(8, 1, &pending_output));
    FT_ASSERT_EQ(FT_ERR_FULL, inventory.add_item(pending_output));
    FT_ASSERT_EQ(1, inventory.count_item(7));
    inventory.remove_item(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(pending_output));
    FT_ASSERT_EQ(1, inventory.count_item(8));
    return (1);
}

FT_TEST(test_prepared_craft_duplicate_ingredients_share_inventory_units)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, append_prepared_ingredient(ingredients,
        7, 2, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, append_prepared_ingredient(ingredients,
        7, 2, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(8, 1, &output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(53,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        output, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 3, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR,
        crafting.prepare_craft(inventory, 53, 1, &prepared));
    FT_ASSERT_EQ(3, inventory.count_item(7));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    return (1);
}

FT_TEST(test_prepared_craft_batch_overflow_does_not_change_inventory)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_sharedptr<game_item> input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 54, 7,
        2, 8, 1, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 4, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        crafting.prepare_craft(inventory, 54, FT_INT32_MAX, &prepared));
    FT_ASSERT_EQ(4, inventory.count_item(7));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    return (1);
}

FT_TEST(test_prepared_craft_uses_recipe_owned_output_definition)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> recipe_output;
    ft_sharedptr<game_item> caller_output;
    uint64_t inventory_revision_before = 0U;
    uint64_t inventory_revision_after = 0U;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, append_prepared_ingredient(ingredients,
        7, 1, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(8, 1,
        &recipe_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(55,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        recipe_output, 2));
    recipe_output->set_item_id(80);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 1, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(99, 1,
        &caller_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        inventory.get_revision(&inventory_revision_before));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        crafting.craft_item(inventory, 55, caller_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        crafting.prepare_craft(inventory, 55, 1, &prepared));
    FT_ASSERT_EQ(1, inventory.count_item(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.commit_craft(inventory, &prepared));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        inventory.get_revision(&inventory_revision_after));
    FT_ASSERT_EQ(inventory_revision_before + 1U, inventory_revision_after);
    FT_ASSERT_EQ(0, inventory.count_item(7));
    FT_ASSERT_EQ(2, inventory.count_item(8));
    FT_ASSERT_EQ(0, inventory.count_item(80));
    FT_ASSERT_EQ(0, inventory.count_item(99));
    return (1);
}

FT_TEST(test_prepared_craft_rejects_changed_inventory_snapshot)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> extra;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 56, 7,
        1, 8, 1, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 2, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        crafting.prepare_craft(inventory, 56, 1, &prepared));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(9, 1, &extra));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(extra));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, crafting.commit_craft(inventory,
        &prepared));
    FT_ASSERT_EQ(2, inventory.count_item(7));
    FT_ASSERT_EQ(1, inventory.count_item(9));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        crafting.prepare_craft(inventory, 56, 1, &prepared));
    inventory._revision += 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, crafting.commit_craft(inventory,
        &prepared));
    FT_ASSERT_EQ(2, inventory.count_item(7));
    FT_ASSERT_EQ(1, inventory.count_item(9));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    return (1);
}

static game_crafting_byproduct make_prepared_byproduct(
    const ft_sharedptr<game_item> &item, int32_t quantity)
{
    game_crafting_byproduct byproduct = {};

    byproduct.item_id = item->get_item_id();
    byproduct.quantity = quantity;
    byproduct.max_stack = item->get_max_stack();
    byproduct.rarity = item->get_rarity();
    byproduct.width = item->get_width();
    byproduct.height = item->get_height();
    byproduct.modifier1_id = item->get_modifier1_id();
    byproduct.modifier1_value = item->get_modifier1_value();
    byproduct.modifier2_id = item->get_modifier2_id();
    byproduct.modifier2_value = item->get_modifier2_value();
    byproduct.modifier3_id = item->get_modifier3_id();
    byproduct.modifier3_value = item->get_modifier3_value();
    byproduct.modifier4_id = item->get_modifier4_id();
    byproduct.modifier4_value = item->get_modifier4_value();
    byproduct.instance_id = item->get_instance_id();
    return (byproduct);
}

FT_TEST(test_prepared_craft_rejects_physical_id_on_output_template)
{
    game_crafting crafting;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(20, 1, &output));
    output->set_instance_id(UINT64_C(5001));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        crafting.register_recipe(901,
            static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
            output, 1, FT_TRUE));
    return (1);
}

FT_TEST(test_prepared_craft_commits_byproducts_with_primary_output)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> output;
    ft_sharedptr<game_item> byproduct;
    std::vector<game_crafting_byproduct> byproducts;
    game_crafting_byproduct configured_byproduct;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(31, 1, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(32, 1, &output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(33, 1, &byproduct));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        append_prepared_ingredient(ingredients, 31, 1, -1));
    configured_byproduct = make_prepared_byproduct(byproduct, 2);
    byproducts.push_back(configured_byproduct);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(902,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        output, 1, byproducts, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        crafting.prepare_craft(inventory, 902, 1, &prepared));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.commit_craft(inventory, &prepared));
    FT_ASSERT_EQ(0, inventory.count_item(31));
    FT_ASSERT_EQ(1, inventory.count_item(32));
    FT_ASSERT_EQ(2, inventory.count_item(33));
    return (1);
}

FT_TEST(test_prepared_craft_byproduct_capacity_failure_rolls_back)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> output;
    ft_sharedptr<game_item> byproduct;
    std::vector<game_crafting_byproduct> byproducts;
    game_crafting_byproduct configured_byproduct;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(41, 1, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(42, 1, &output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(43, 1, &byproduct));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        append_prepared_ingredient(ingredients, 41, 1, -1));
    configured_byproduct = make_prepared_byproduct(byproduct, 1);
    byproducts.push_back(configured_byproduct);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(903,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        output, 1, byproducts, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_FULL,
        crafting.prepare_craft(inventory, 903, 1, &prepared));
    FT_ASSERT_EQ(1, inventory.count_item(41));
    FT_ASSERT_EQ(0, inventory.count_item(42));
    FT_ASSERT_EQ(0, inventory.count_item(43));
    return (1);
}

FT_TEST(test_prepared_craft_registration_bounds_byproduct_count)
{
    game_crafting crafting;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> output;
    std::vector<game_crafting_byproduct> byproducts(
        GAME_CRAFTING_MAX_BYPRODUCTS + 1U);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(52, 1, &output));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crafting.register_recipe(904,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        output, 1, byproducts, FT_TRUE));
    return (1);
}

FT_TEST(test_prepared_craft_requires_registered_station)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    game_crafting_query_result query = {};
    ft_vector<game_crafting_missing_ingredient> missing;
    ft_vector<game_crafting_ingredient> legacy_ingredients;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> legacy_input;
    ft_sharedptr<game_item> legacy_output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 91, 7,
        1, 8, 1, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, crafting.set_recipe_station(900, 42));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.set_recipe_station(91, 42));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, crafting.set_recipe_station(91, 43));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.unlock_recipe(91));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 92, 7,
        1, 8, 1, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, crafting.set_recipe_station(92, 42));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 1, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.query_recipe_at_station(91,
        inventory, 41, &query, &missing));
    FT_ASSERT_EQ(FT_TRUE, query.found);
    FT_ASSERT_EQ(FT_FALSE, query.craftable);
    FT_ASSERT_EQ(FT_FALSE, query.station_available);
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crafting.prepare_craft(
        inventory, 91, 1, &prepared));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crafting.prepare_craft_at_station(
        inventory, 91, 41, 1, &prepared));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.query_recipe_at_station(91,
        inventory, 42, &query, &missing));
    FT_ASSERT_EQ(FT_TRUE, query.craftable);
    FT_ASSERT_EQ(FT_TRUE, query.station_available);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.prepare_craft_at_station(
        inventory, 91, 42, 1, &prepared));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.commit_craft(inventory, &prepared));
    FT_ASSERT_EQ(0, inventory.count_item(7));
    FT_ASSERT_EQ(1, inventory.count_item(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, legacy_ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, append_prepared_ingredient(
        legacy_ingredients, 7, 1, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(93,
        static_cast<ft_vector<game_crafting_ingredient> &&>(legacy_ingredients),
        FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.set_recipe_station(93, 42));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.unlock_recipe(93));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 1, &legacy_input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(9, 1, &legacy_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(legacy_input));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crafting.craft_item(
        inventory, 93, legacy_output));
    FT_ASSERT_EQ(1, inventory.count_item(7));
    FT_ASSERT_EQ(0, inventory.count_item(9));
    return (1);
}

FT_TEST(test_prepared_craft_alternative_ingredients_share_inventory_units)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    game_crafting_query_result query = {};
    ft_vector<game_crafting_missing_ingredient> missing;
    ft_vector<game_crafting_ingredient> ingredients;
    game_crafting_ingredient alternative;
    game_crafting_ingredient exact;
    ft_sharedptr<game_item> output;
    ft_sharedptr<game_item> input;
    const int32_t alternative_ids[2] = {7, 9};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, alternative.initialize_alternatives(
        alternative_ids, 2U, 2, -1));
    FT_ASSERT_EQ(2U, alternative.get_alternative_count());
    FT_ASSERT_EQ(7, alternative.get_alternative_item_id(0U));
    FT_ASSERT_EQ(9, alternative.get_alternative_item_id(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.push_back(alternative));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, exact.initialize(9, 2, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.push_back(exact));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(10, 1, &output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(94,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        output, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(9, 3, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.query_recipe(94, inventory,
        &query, &missing));
    FT_ASSERT_EQ(FT_FALSE, query.craftable);
    FT_ASSERT_EQ(1U, query.missing_group_count);
    FT_ASSERT_EQ(1, query.missing_units);
    FT_ASSERT_EQ(9, missing[0].item_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(9, 1, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.prepare_craft(inventory, 94,
        1, &prepared));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.commit_craft(inventory, &prepared));
    FT_ASSERT_EQ(0, inventory.count_item(9));
    FT_ASSERT_EQ(1, inventory.count_item(10));
    return (1);
}
