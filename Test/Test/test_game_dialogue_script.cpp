#include "../test_internal.hpp"
#include "../../Modules/Game/game_dialogue_script.hpp"
#include "../../Modules/Game/game_dialogue_line.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int32_t make_line(ft_sharedptr<game_dialogue_line> &stored, int id,
        const char *speaker, const char *text)
{
    ft_vector<int> next;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next.push_back(id + 1));
    stored = ft_sharedptr<game_dialogue_line>(new (std::nothrow) game_dialogue_line());
    FT_ASSERT(stored.get() != ft_nullptr);
    ft_string speaker_string;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, speaker_string.initialize(speaker));
    ft_string text_string;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, text_string.initialize(text));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored->initialize(id, speaker_string,
        text_string, next));
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_dialogue_script_initial_state)
{
    game_dialogue_script script;
    ft_string empty;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty.initialize());
    FT_ASSERT_EQ(0, script.get_script_id());
    FT_ASSERT_EQ(empty.c_str(), script.get_title().c_str());
    FT_ASSERT_EQ(empty.c_str(), script.get_summary().c_str());
    FT_ASSERT_EQ(0, script.get_start_line_id());
    FT_ASSERT_EQ(0u, script.get_lines().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.get_error());
    return (1);
}

FT_TEST(test_dialogue_script_metadata_preserves_values_on_failure)
{
    game_dialogue_script script;
    ft_string original;
    ft_string large;
    ft_vector<game_dialogue_line> lines;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize("opening"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, large.initialize(4096U, 'x'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.initialize());
    script.set_title(original);
    script.set_summary(original);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize(5, script.get_title(),
        script.get_summary(), 9, lines));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.enable_thread_safety());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(64U));
    script.set_title(large);
    const int32_t title_error = script.get_error();
    script.set_summary(large);
    const int32_t summary_error = script.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, title_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, summary_error);
    FT_ASSERT_STR_EQ("opening", script.get_title().c_str());
    FT_ASSERT_STR_EQ("opening", script.get_summary().c_str());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    script.set_start_line_id(10);
    const int32_t setter_error = script.get_error();
    const int32_t failed_id = script.get_script_id();
    const int32_t getter_error = script.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, setter_error);
    FT_ASSERT_EQ(0, failed_id);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, getter_error);
    FT_ASSERT_EQ(5, script.get_script_id());
    FT_ASSERT_EQ(9, script.get_start_line_id());
    return (1);
}

FT_TEST(test_dialogue_script_deep_copy_rolls_back_failed_line_replacement)
{
    game_dialogue_script source;
    game_dialogue_script destination;
    game_dialogue_script copied;
    ft_sharedptr<game_dialogue_line> first;
    ft_sharedptr<game_dialogue_line> second;
    ft_sharedptr<game_dialogue_line> empty;
    ft_vector<ft_sharedptr<game_dialogue_line>> lines;
    ft_string long_text;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(first, 3, "npc", "hello"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(second, 7, "npc", "reply"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, long_text.initialize(4096U, 'x'));
    second->set_text(long_text);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.push_back(first));
    destination.set_lines(lines);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.push_back(empty));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.push_back(second));
    source.set_lines(lines);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(128U));
    destination.set_lines(lines);
    const int32_t setter_error = destination.get_error();
    const int32_t copy_error = copied.initialize(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, setter_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, copy_error);
    FT_ASSERT_EQ(1U, destination.get_lines().size());
    FT_ASSERT_STR_EQ("hello", destination.get_lines()[0]->get_text().c_str());
    destination.set_lines(destination.get_lines());
    FT_ASSERT_EQ(1U, destination.get_lines().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize(source));
    const auto &copied_lines = copied.get_lines();
    const auto &source_lines = source.get_lines();
    FT_ASSERT_EQ(3U, copied_lines.size());
    FT_ASSERT(copied_lines[0].get() != source_lines[0].get());
    FT_ASSERT_EQ(ft_nullptr, copied_lines[1].get());
    FT_ASSERT_EQ(4096U, copied_lines[2]->get_text().size());
    const auto *first_clone = copied_lines[0].get();
    game_dialogue_script moved;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t move_error = moved.move(copied);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, move_error);
    FT_ASSERT(first_clone == moved.get_lines()[0].get());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize());
    FT_ASSERT_EQ(0U, copied.get_lines().size());
    return (1);
}

FT_TEST(test_dialogue_script_setters_modify_metadata_and_lines)
{
    game_dialogue_script script;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    script.set_script_id(4);
    ft_string intro;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, intro.initialize("intro"));
    ft_string desc;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, desc.initialize("desc"));
    script.set_title(intro);
    script.set_summary(desc);
    script.set_start_line_id(2);
    ft_vector<ft_sharedptr<game_dialogue_line>> lines;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.initialize());
    ft_sharedptr<game_dialogue_line> line;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_line(line, 2, "npc", "reply"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.push_back(line));
    script.set_lines(lines);

    const ft_vector<ft_sharedptr<game_dialogue_line>> &stored = script.get_lines();
    FT_ASSERT_EQ(1u, stored.size());
    FT_ASSERT_EQ(2, stored[0]->get_line_id());
    FT_ASSERT_STR_EQ("reply", stored[0]->get_text().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored[0]->get_error());
    return (1);
}
