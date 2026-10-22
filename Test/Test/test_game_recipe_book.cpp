#include "../test_internal.hpp"
#include "../../Modules/Game/game_recipe_book.hpp"
#include "../../Modules/Buffer/byte_buffer.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t recipe_book_test_catalogue(ft_vector<uint32_t> *recipe_ids,
    uint32_t first_id, uint32_t second_id)
{
    int32_t error_code;

    error_code = recipe_ids->initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = recipe_ids->push_back(first_id);
    if (error_code == FT_ERR_SUCCESS && second_id != 0U)
        error_code = recipe_ids->push_back(second_id);
    return (error_code);
}

FT_TEST(test_game_recipe_book_duplicate_event_is_idempotent)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book book;
    ft_bool newly_unlocked;
    ft_bool known;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids,
        100U, 200U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.unlock_recipe(100U, 9001U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_TRUE, newly_unlocked);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.unlock_recipe(100U, 9001U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_FALSE, newly_unlocked);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.is_recipe_known(100U, &known));
    FT_ASSERT_EQ(FT_TRUE, known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.is_recipe_known(200U, &known));
    FT_ASSERT_EQ(FT_FALSE, known);
    return (1);
}

FT_TEST(test_game_recipe_book_initial_unlocks_are_seeded)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book book;
    game_recipe_unlock_record initial_unlock;
    ft_bool known;
    ft_bool newly_unlocked;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids,
        100U, 200U));
    initial_unlock.source_event_id = 8001U;
    initial_unlock.recipe_id = 100U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.initialize(recipe_ids,
        &initial_unlock, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.is_recipe_known(100U, &known));
    FT_ASSERT_EQ(FT_TRUE, known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.is_recipe_known(200U, &known));
    FT_ASSERT_EQ(FT_FALSE, known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.unlock_recipe(100U, 8001U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_FALSE, newly_unlocked);
    return (1);
}

FT_TEST(test_game_recipe_book_invalid_initial_unlock_is_rejected)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book book;
    game_recipe_unlock_record initial_unlock;
    ft_bool known;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids,
        10U, 11U));
    initial_unlock.source_event_id = 100U;
    initial_unlock.recipe_id = 12U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, book.initialize(recipe_ids,
        &initial_unlock, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.is_recipe_known(10U, &known));
    FT_ASSERT_EQ(FT_FALSE, known);
    return (1);
}

FT_TEST(test_game_recipe_book_distinct_sources_do_not_duplicate_unlock)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book book;
    ft_bool newly_unlocked;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids,
        77U, 78U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.unlock_recipe(77U, 101U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_TRUE, newly_unlocked);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.unlock_recipe(77U, 102U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_FALSE, newly_unlocked);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, book.unlock_recipe(78U, 102U,
        &newly_unlocked));
    return (1);
}

FT_TEST(test_game_recipe_book_known_recipe_records_each_source_event)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book source;
    game_recipe_book restored;
    game_recipe_unlock_result result = {};
    ft_byte_buffer output;
    ft_byte_buffer input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids,
        77U, 78U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.unlock_recipe_with_result(77U,
        101U, &result));
    FT_ASSERT_EQ(FT_TRUE, result.event_newly_recorded);
    FT_ASSERT_EQ(FT_TRUE, result.recipe_newly_known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.unlock_recipe_with_result(77U,
        102U, &result));
    FT_ASSERT_EQ(FT_TRUE, result.event_newly_recorded);
    FT_ASSERT_EQ(FT_FALSE, result.recipe_newly_known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.export_book(output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.append(output.data(), output.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.import_book(input));
    FT_ASSERT_EQ(input.size(), input.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.unlock_recipe_with_result(77U,
        102U, &result));
    FT_ASSERT_EQ(FT_FALSE, result.event_newly_recorded);
    FT_ASSERT_EQ(FT_FALSE, result.recipe_newly_known);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, restored.unlock_recipe_with_result(78U,
        102U, &result));
    FT_ASSERT_EQ(FT_FALSE, result.event_newly_recorded);
    FT_ASSERT_EQ(FT_FALSE, result.recipe_newly_known);
    return (1);
}

FT_TEST(test_game_recipe_book_serialization_round_trip)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book source;
    game_recipe_book restored;
    ft_byte_buffer output;
    ft_byte_buffer input;
    ft_bool newly_unlocked;
    ft_bool known;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids,
        15U, 900000U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.unlock_recipe(900000U, 44U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_TRUE, newly_unlocked);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.export_book(output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.append(output.data(), output.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.import_book(input));
    FT_ASSERT_EQ(input.size(), input.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.is_recipe_known(900000U, &known));
    FT_ASSERT_EQ(FT_TRUE, known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.unlock_recipe(900000U, 44U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_FALSE, newly_unlocked);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.unlock_recipe(900000U, 45U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_FALSE, newly_unlocked);
    return (1);
}

FT_TEST(test_game_recipe_book_malformed_import_preserves_state)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book book;
    ft_byte_buffer valid_output;
    ft_byte_buffer truncated_input;
    ft_bool newly_unlocked;
    ft_bool known;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids,
        501U, 502U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.unlock_recipe(501U, 33U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, valid_output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.export_book(valid_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated_input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated_input.append(valid_output.data(),
        valid_output.size() - 1U));
    FT_ASSERT_NE(FT_ERR_SUCCESS, book.import_book(truncated_input));
    FT_ASSERT_EQ(0U, truncated_input.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.is_recipe_known(501U, &known));
    FT_ASSERT_EQ(FT_TRUE, known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.is_recipe_known(502U, &known));
    FT_ASSERT_EQ(FT_FALSE, known);
    return (1);
}

FT_TEST(test_game_recipe_book_export_failure_preserves_existing_output)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book book;
    ft_byte_buffer output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids,
        40U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize(16U, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.append_u8(0xA5U));
    FT_ASSERT_EQ(FT_ERR_FULL, book.export_book(output));
    FT_ASSERT_EQ(1U, output.size());
    FT_ASSERT_EQ(0xA5U, output.data()[0]);
    return (1);
}

FT_TEST(test_game_recipe_book_unknown_recipe_is_quarantined_and_preserved)
{
    ft_vector<uint32_t> source_ids;
    ft_vector<uint32_t> target_ids;
    game_recipe_book source;
    game_recipe_book target;
    game_recipe_book upgraded;
    ft_byte_buffer output;
    ft_byte_buffer input;
    ft_byte_buffer preserved_output;
    ft_byte_buffer preserved_input;
    ft_bool newly_unlocked;
    ft_bool known;
    uint32_t quarantined_count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&source_ids,
        7U, 900U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&target_ids,
        7U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(source_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize(target_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.unlock_recipe(900U, 800U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.unlock_recipe(7U, 700U,
        &newly_unlocked));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.export_book(output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.append(output.data(), output.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.import_book(input));
    FT_ASSERT_EQ(input.size(), input.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.is_recipe_known(7U, &known));
    FT_ASSERT_EQ(FT_FALSE, known);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, target.is_recipe_known(900U, &known));
    FT_ASSERT_EQ(FT_FALSE, known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        target.quarantined_unlock_count(&quarantined_count));
    FT_ASSERT_EQ(1U, quarantined_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, preserved_output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.export_book(preserved_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgraded.initialize(source_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, preserved_input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, preserved_input.append(
        preserved_output.data(), preserved_output.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgraded.import_book(preserved_input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgraded.is_recipe_known(900U, &known));
    FT_ASSERT_EQ(FT_TRUE, known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        upgraded.quarantined_unlock_count(&quarantined_count));
    FT_ASSERT_EQ(0U, quarantined_count);
    return (1);
}
