#include "../test_internal.hpp"
#include "../../Modules/Game/game_dialogue_line.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_dialogue_line_failed_edits_keep_previous_values)
{
    game_dialogue_line line;
    ft_string original;
    ft_string changed;
    ft_string large_text;
    ft_vector<int32_t> old_ids;
    ft_vector<int32_t> changed_ids;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize("old"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, changed.initialize("new"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, large_text.initialize(4096U, 'x'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, old_ids.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, changed_ids.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, old_ids.push_back(7));
    int32_t id = 1;
    while (id <= 32)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, changed_ids.push_back(id++));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.initialize(5, original, original, old_ids));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(64U));
    line.set_text(large_text);
    const int32_t text_error = line.get_error();
    line.set_next_line_ids(changed_ids);
    const int32_t ids_error = line.get_error();
    const int32_t replace_error = line.initialize(6, changed, changed, changed_ids);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, text_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, ids_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, replace_error);
    FT_ASSERT_EQ(5, line.get_line_id());
    FT_ASSERT_STR_EQ("old", line.get_speaker().c_str());
    FT_ASSERT_STR_EQ("old", line.get_text().c_str());
    FT_ASSERT_EQ(1U, line.get_next_line_ids().size());
    FT_ASSERT_EQ(7, line.get_next_line_ids()[0]);
    line.set_next_line_ids(line.get_next_line_ids());
    FT_ASSERT_EQ(1U, line.get_next_line_ids().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.initialize(6, changed, changed, changed_ids));
    FT_ASSERT_EQ(6, line.get_line_id());
    FT_ASSERT_EQ(32U, line.get_next_line_ids().size());
    return (1);
}

FT_TEST(test_dialogue_line_move_transfers_text_and_link_storage)
{
    game_dialogue_line source;
    game_dialogue_line destination;
    ft_string speaker;
    ft_string text;
    ft_vector<int32_t> next_ids;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, speaker.initialize("npc"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, text.initialize(4096U, 'x'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next_ids.initialize());
    int32_t id = 1;
    while (id <= 32)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, next_ids.push_back(id++));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(4, speaker, text, next_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_next_line_ids().enable_thread_safety());
    const char *speaker_storage = source.get_speaker().c_str();
    const char *text_storage = source.get_text().c_str();
    const auto *links_storage = source.get_next_line_ids().begin();

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t failed_move = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, failed_move);
    FT_ASSERT_EQ(FT_FALSE, destination.is_initialised());
    FT_ASSERT(text_storage == source.get_text().c_str());
    FT_ASSERT(links_storage == source.get_next_line_ids().begin());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_FALSE, source.is_initialised());
    FT_ASSERT_EQ(4, destination.get_line_id());
    FT_ASSERT(speaker_storage == destination.get_speaker().c_str());
    FT_ASSERT(text_storage == destination.get_text().c_str());
    FT_ASSERT(links_storage == destination.get_next_line_ids().begin());
    FT_ASSERT_EQ(FT_TRUE, destination.get_next_line_ids().is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_next_line_ids().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(0U, destination.get_text().size());
    return (1);
}

FT_TEST(test_dialogue_line_next_ids_reset)
{
    ft_vector<int> initial_next_lines;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initial_next_lines.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initial_next_lines.push_back(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initial_next_lines.push_back(4));
    game_dialogue_line line;

    ft_string speaker;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, speaker.initialize("speaker"));
    ft_string text;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, text.initialize("text"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.initialize(1, speaker,
        text, initial_next_lines));

    line.set_line_id(1);
    line.set_speaker(speaker);
    line.set_text(text);
    line.set_next_line_ids(initial_next_lines);

    ft_vector<int> replacement_next_lines;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement_next_lines.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement_next_lines.push_back(9));
    line.set_next_line_ids(replacement_next_lines);

    FT_ASSERT_EQ(1u, line.get_next_line_ids().size());
    FT_ASSERT_EQ(9, line.get_next_line_ids()[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.get_error());
    return (1);
}

FT_TEST(test_dialogue_line_getters_expose_vector_views)
{
    ft_vector<int> ids;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ids.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ids.push_back(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ids.push_back(11));
    game_dialogue_line line;

    ft_string npc;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, npc.initialize("npc"));
    ft_string hint;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hint.initialize("hint"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.initialize(3, npc,
        hint, ids));

    line.set_line_id(3);
    line.set_speaker(npc);
    line.set_text(hint);
    line.set_next_line_ids(ids);

    ft_vector<int> &editable = line.get_next_line_ids();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, editable.push_back(15));

    const ft_vector<int> &readonly = line.get_next_line_ids();
    FT_ASSERT_EQ(3u, readonly.size());
    FT_ASSERT_EQ(15, readonly[2]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.get_error());
    return (1);
}

FT_TEST(test_dialogue_line_setters_apply_latest_values)
{
    game_dialogue_line line;
    ft_vector<int> empty_next_lines;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty_next_lines.initialize());

    ft_string empty_speaker;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty_speaker.initialize(""));
    ft_string empty_text;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty_text.initialize(""));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.initialize(0, empty_speaker, empty_text,
        empty_next_lines));

    line.set_line_id(5);
    ft_string old_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, old_value.initialize("old"));
    line.set_speaker(old_value);
    ft_string start;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, start.initialize("start"));
    line.set_text(start);

    ft_string middle;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, middle.initialize("middle"));
    line.set_speaker(middle);
    ft_string next_new;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next_new.initialize("new"));
    line.set_speaker(next_new);
    ft_string first;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize("first"));
    line.set_text(first);
    ft_string last;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, last.initialize("last"));
    line.set_text(last);
    line.set_line_id(4);
    line.set_line_id(9);

    FT_ASSERT_STR_EQ("new", line.get_speaker().c_str());
    FT_ASSERT_STR_EQ("last", line.get_text().c_str());
    FT_ASSERT_EQ(9, line.get_line_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, line.get_error());
    return (1);
}
