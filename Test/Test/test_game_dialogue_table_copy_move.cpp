#include "../test_internal.hpp"
#include "../../Modules/Game/game_dialogue_table.hpp"
#include "../../Modules/Game/game_dialogue_line.hpp"
#include "../../Modules/Game/game_dialogue_script.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int32_t make_line(ft_sharedptr<game_dialogue_line> &stored, int id,
        const char *speaker, const char *text)
{
    ft_vector<int> next;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next.get_error());
    next.push_back(id + 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next.get_error());
    stored = ft_sharedptr<game_dialogue_line>(new (std::nothrow) game_dialogue_line());
    FT_ASSERT(stored.get() != ft_nullptr);
    ft_string speaker_string;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, speaker_string.initialize(speaker));
    ft_string text_string;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, text_string.initialize(text));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored->initialize(id, speaker_string,
        text_string, next));
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_dialogue_table_register_line_retrieves_data)
{
    game_dialogue_table table;
    game_dialogue_line loaded;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    ft_vector<int> next;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next.get_error());
    next.push_back(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next.get_error());
    game_dialogue_line original;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.get_error());
    ft_string npc;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, npc.initialize("npc"));
    ft_string hello;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hello.initialize("hello"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize(1, npc,
        hello, next));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_line(original));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_line(1, loaded));
    FT_ASSERT_EQ(1, loaded.get_line_id());
    FT_ASSERT_STR_EQ("hello", loaded.get_text().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded.get_error());
    return (1);
}

FT_TEST(test_dialogue_table_move_transfers_owned_line_cache)
{
    game_dialogue_table source;
    game_dialogue_table destination;
    game_dialogue_script script;
    ft_sharedptr<game_dialogue_line> line;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(line, 4, "npc", "hello"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_line(*line.get()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    script.set_script_id(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_script(script));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_scripts().enable_thread_safety());
    const auto *line_slot = source.get_lines().find(4);
    const auto *script_slot = source.get_scripts().find(7);
    const auto *cache_slot = source._line_cache.find(4);

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t failed_move = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, failed_move);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT(line_slot == source.get_lines().find(4));
    FT_ASSERT(cache_slot == source._line_cache.find(4));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT(line_slot == destination.get_lines().find(4));
    FT_ASSERT(script_slot == destination.get_scripts().find(7));
    FT_ASSERT(cache_slot == destination._line_cache.find(4));
    game_dialogue_line fetched;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.fetch_line(4, fetched));
    FT_ASSERT_STR_EQ("hello", fetched.get_text().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_lines().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(0U, destination.get_scripts().size());
    return (1);
}

FT_TEST(test_dialogue_table_bulk_copy_preserves_live_scripts_on_failure)
{
    game_dialogue_table source;
    game_dialogue_table destination;
    game_dialogue_table clone;
    game_dialogue_script script;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    script.set_script_id(9);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_script(script));
    script.set_script_id(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.register_script(script));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_scripts().enable_thread_safety());

    destination.set_scripts(destination.get_scripts());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(1U, destination.get_scripts().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    destination.set_scripts(source.get_scripts());
    const int32_t setter_error = destination.get_error();
    const int32_t copy_error = clone.initialize(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, setter_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, copy_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, clone._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.fetch_script(7, script));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, destination.fetch_script(9, script));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.fetch_script(9, script));

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t fetch_error = destination.fetch_script(7, script);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, fetch_error);
    FT_ASSERT_EQ(9, script.get_script_id());
    return (1);
}

FT_TEST(test_dialogue_table_copy_keeps_cached_lines_independent)
{
    game_dialogue_table source;
    game_dialogue_table clone;
    ft_sharedptr<game_dialogue_line> line;
    game_dialogue_line fetched;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(line, 4, "npc", "hello"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_line(*line.get()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.initialize(source));
    const auto *stored = source._line_cache.find(4);
    const auto *copied = clone._line_cache.find(4);
    FT_ASSERT(stored != source._line_cache.end());
    FT_ASSERT(copied != clone._line_cache.end());
    FT_ASSERT(stored->value != copied->value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.fetch_line(4, fetched));
    FT_ASSERT_STR_EQ("hello", fetched.get_text().c_str());
    return (1);
}

FT_TEST(test_dialogue_table_failed_script_registration_preserves_entry)
{
    game_dialogue_table table;
    game_dialogue_script script;
    game_dialogue_script fetched;
    ft_string title;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, title.initialize("old title"));
    script.set_script_id(7);
    script.set_title(title);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_script(script));
    ft_string long_title;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, long_title.initialize(4096U, 'x'));
    script.set_title(long_title);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(64U));
    const int32_t result = table.register_script(script);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_script(7, fetched));
    FT_ASSERT_STR_EQ("old title", fetched.get_title().c_str());
    return (1);
}

FT_TEST(test_dialogue_table_failed_line_updates_preserve_both_views)
{
    game_dialogue_table table;
    ft_sharedptr<game_dialogue_line> first;
    ft_sharedptr<game_dialogue_line> replacement;
    ft_map<int32_t, ft_sharedptr<game_dialogue_line>> replacement_lines;
    game_dialogue_line fetched;
    ft_string large_text;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement_lines.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(first, 4, "npc", "hello"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(replacement, 4, "npc", "reply"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_line(*first.get()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, large_text.initialize(4096U, 'x'));
    replacement->set_text(large_text);
    replacement_lines.insert(4, replacement);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement_lines.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(256U));
    const int32_t register_error = table.register_line(*replacement.get());
    table.set_lines(replacement_lines);
    const int32_t setter_error = table.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, register_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, setter_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_line(4, fetched));
    FT_ASSERT_STR_EQ("hello", fetched.get_text().c_str());
    FT_ASSERT_EQ(1U, table.get_lines().size());
    table.set_lines(table.get_lines());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_line(*replacement.get()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_line(4, fetched));
    FT_ASSERT_EQ(4096U, fetched.get_text().size());
    return (1);
}

FT_TEST(test_dialogue_table_failed_public_map_growth_rolls_back_line_cache)
{
    game_dialogue_table table;
    ft_sharedptr<game_dialogue_line> line;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(line, 40, "npc", "hello"));
    auto &lines = table.get_lines();
    const int32_t capacity = static_cast<int32_t>(lines.capacity());
    FT_ASSERT(capacity < 40);
    int32_t id = 1;
    while (id <= capacity)
    {
        lines.insert(id++, line);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.get_error());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(64U));
    const int32_t result = table.register_line(*line.get());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(0U, table._line_cache.size());
    FT_ASSERT_EQ(static_cast<ft_size_t>(capacity), lines.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_line(*line.get()));
    FT_ASSERT_EQ(1U, table._line_cache.size());
    return (1);
}

FT_TEST(test_dialogue_table_fetch_preserves_snapshots_and_propagates_fallback_lock_error)
{
    game_dialogue_table table;
    game_dialogue_line fetched;
    ft_sharedptr<game_dialogue_line> line;
    ft_sharedptr<game_dialogue_line> fallback;
    ft_string edited_text;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(line, 4, "npc", "cached"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(fallback, 7, "npc", "fallback"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, edited_text.initialize("edited"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_line(*line.get()));
    auto &lines = table.get_lines();
    auto *registered = lines.find(4);
    FT_ASSERT(registered != lines.end());
    registered->value->set_text(edited_text);
    lines.insert(7, fallback);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_line(4, fetched));
    FT_ASSERT_STR_EQ("cached", fetched.get_text().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_line(7, *fallback.get()));
    FT_ASSERT_STR_EQ("fallback", fallback->get_text().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.enable_thread_safety());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t fetch_error = table.fetch_line(7, fetched);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, fetch_error);
    FT_ASSERT_EQ(4, fetched.get_line_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_line(7, fetched));
    FT_ASSERT_STR_EQ("fallback", fetched.get_text().c_str());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, table.fetch_line(9, fetched));
    FT_ASSERT_EQ(7, fetched.get_line_id());
    return (1);
}

FT_TEST(test_dialogue_table_scripts_store_lines_via_sharedptrs)
{
    game_dialogue_table table;
    ft_vector<ft_sharedptr<game_dialogue_line>> lines;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.get_error());
    ft_sharedptr<game_dialogue_line> line;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(line, 2, "player", "reply"));
    lines.push_back(line);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.get_error());

    game_dialogue_script script;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    script.set_script_id(5);
    ft_string intro;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, intro.initialize("intro"));
    ft_string start;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, start.initialize("start"));
    script.set_title(intro);
    script.set_summary(start);
    script.set_start_line_id(2);
    script.set_lines(lines);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_script(script));
    game_dialogue_script fetched;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_script(5, fetched));
    const ft_vector<ft_sharedptr<game_dialogue_line>> &fetched_lines = fetched.get_lines();
    FT_ASSERT_EQ(1u, fetched_lines.size());
    FT_ASSERT_EQ(2, fetched_lines[0]->get_line_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched_lines[0]->get_error());
    return (1);
}
