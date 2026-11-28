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

FT_TEST(test_game_recipe_book_indexes_unsorted_recipes_and_unlocks)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book book;
    game_recipe_book restored;
    ft_byte_buffer wire;
    game_recipe_unlock_result result = {};
    ft_bool known = FT_FALSE;
    const game_recipe_unlock_record duplicate_seeds[] = {{90U, 1U}, {90U, UINT32_MAX}};
    const game_recipe_unlock_record seeds[] = {{UINT64_MAX, 1U}, {9U, UINT32_MAX}};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids,
        UINT32_MAX, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_ids.push_back(32U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_ids.push_back(UINT32_MAX));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, book.initialize(recipe_ids));
    recipe_ids.pop_back();
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, book.initialize(recipe_ids, duplicate_seeds, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.initialize(recipe_ids, seeds, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.unlock_recipe_with_result(32U, 5U, &result));
    FT_ASSERT_EQ(FT_TRUE, result.event_newly_recorded);
    FT_ASSERT_EQ(FT_TRUE, result.recipe_newly_known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.unlock_recipe_with_result(1U, 40U, &result));
    FT_ASSERT_EQ(FT_TRUE, result.event_newly_recorded);
    FT_ASSERT_EQ(FT_FALSE, result.recipe_newly_known);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, book.unlock_recipe_with_result(32U, UINT64_MAX, &result));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.unlock_recipe_with_result(1U, 12U, &result));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wire.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, book.export_book(wire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.import_book(wire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.is_recipe_known(UINT32_MAX, &known));
    FT_ASSERT_EQ(FT_TRUE, known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.unlock_recipe_with_result(32U, 5U, &result));
    FT_ASSERT_EQ(FT_FALSE, result.event_newly_recorded);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, restored.unlock_recipe_with_result(1U, 9U, &result));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.unlock_recipe_with_result(32U, 12U, &result));
    FT_ASSERT_EQ(FT_TRUE, result.event_newly_recorded);
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

FT_TEST(test_game_recipe_book_cached_revision_matches_serialized_event_order)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book source;
    game_recipe_book restored;
    ft_byte_buffer payload;
    ft_bool newly_unlocked = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids, 77U, 78U));
    const game_recipe_unlock_record initial[] = {{77U, 78U}};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(recipe_ids, initial, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.unlock_recipe(77U, 33U, &newly_unlocked));
    uint64_t revision = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_revision(&revision));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.export_book(payload));
    uint64_t expected = 14695981039346656037ULL;
    ft_size_t index = 12U;
    while (index < payload.size() - 4U)
    {
        expected = (expected ^ payload.data()[index]) * 1099511628211ULL;
        ++index;
    }
    FT_ASSERT_EQ(expected, revision);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.import_book(payload));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.get_revision(&revision));
    FT_ASSERT_EQ(expected, revision);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.unlock_recipe(77U, 33U, &newly_unlocked));
    FT_ASSERT_EQ(FT_FALSE, newly_unlocked);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.get_revision(&revision));
    FT_ASSERT_EQ(expected, revision);
    return (1);
}

FT_TEST(test_game_recipe_book_duplicate_import_preserves_cached_state)
{
    ft_vector<uint32_t> recipe_ids;
    game_recipe_book invalid_book;
    game_recipe_book live;
    ft_byte_buffer payload;
    ft_bool newly_unlocked = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&recipe_ids, 77U, 78U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_book.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_book.unlock_recipe(77U, 33U, &newly_unlocked));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_book.unlock_recipe(78U, 5U, &newly_unlocked));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_book._unlock_events.push_back({33U, 78U}));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_book.export_book(payload));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, live.initialize(recipe_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, live.unlock_recipe(78U, 40U, &newly_unlocked));
    uint64_t expected = 0U;
    uint64_t revision = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, live.get_revision(&expected));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, live.import_book(payload));
    FT_ASSERT_EQ(0U, payload.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, live.get_revision(&revision));
    FT_ASSERT_EQ(expected, revision);
    ft_bool known = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, live.is_recipe_known(78U, &known));
    FT_ASSERT_EQ(FT_TRUE, known);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, live.is_recipe_known(77U, &known));
    FT_ASSERT_EQ(FT_FALSE, known);
    return (1);
}

FT_TEST(test_game_recipe_book_wire_record_appends_and_imports_at_nonzero_offset)
{
    ft_vector<uint32_t> ids;
    game_recipe_book source;
    game_recipe_book restored;
    ft_byte_buffer payload;
    const uint32_t recipe_id = 0x01020304U;
    const uint64_t event_id = 0x0102030405060708ULL;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_book_test_catalogue(&ids, recipe_id, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize(ids));
    ft_bool known = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.unlock_recipe(recipe_id, event_id, &known));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.append_u8(0xA5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.append_u8(0xACU));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.set_read_position(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.export_book(payload));
    FT_ASSERT_EQ(1U, payload.read_position());
    const uint8_t expected[] = {0xA5U, 0xACU, 0x42U, 0x52U, 0x42U, 0x47U,
        1U, 0U, 0U, 0U, 1U, 0U, 0U, 0U, 8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U,
        4U, 3U, 2U, 1U};
    FT_ASSERT_EQ(sizeof(expected) + 4U, payload.size());
    ft_size_t index = 0U;
    while (index < sizeof(expected))
    {
        FT_ASSERT_EQ(expected[index], payload.data()[index]);
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.set_read_position(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.import_book(payload));
    FT_ASSERT_EQ(payload.size(), payload.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.is_recipe_known(recipe_id, &known));
    FT_ASSERT_EQ(FT_TRUE, known);
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
