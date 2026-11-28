#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/ReadLine/readline.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstdio>

static int64_t discard_readline_redraw(int32_t descriptor, const void *, ft_size_t length)
{
    if (descriptor != STDOUT_FILENO)
        return (-1);
    return (static_cast<int64_t>(length));
}

static int32_t run_readline_from_text(const char *input, char **line)
{
    FILE *input_file = std::tmpfile();
    if (input_file == ft_nullptr)
        return (FT_ERR_IO);
    const ft_size_t length = ft_strlen_size_t(input);
    if (std::fwrite(input, 1, length, input_file) != length || std::fseek(input_file, 0, SEEK_SET) != 0)
    {
        (void)std::fclose(input_file);
        return (FT_ERR_IO);
    }
    const int32_t backup = dup(STDIN_FILENO);
    if (backup < 0)
    {
        (void)std::fclose(input_file);
        return (FT_ERR_IO);
    }
    int32_t result = FT_ERR_IO;
    if (dup2(fileno(input_file), STDIN_FILENO) != -1)
    {
        su_set_write_syscall_hook(discard_readline_redraw);
        *line = rl_readline("> ");
        su_reset_write_syscall_hook();
        if (dup2(backup, STDIN_FILENO) != -1)
            result = FT_ERR_SUCCESS;
    }
    (void)close(backup);
    (void)std::fclose(input_file);
    return (result);
}

static int32_t readline_session_candidate(const char *, int32_t, const char *, void *)
{
    return (rl_completion_add_candidate("hello"));
}

FT_TEST(test_readline_session_returns_edited_line_and_releases_cancelled_completion)
{
    rl_clear_history();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_enable_auto_save(FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_set_completion_callback(readline_session_candidate, ft_nullptr));
    char *accepted = ft_nullptr;
    const int32_t accepted_status = run_readline_from_text("a\bhel\t\n", &accepted);
    const ft_bool returned_line = (accepted != ft_nullptr && ft_strcmp(accepted, "hello") == 0);
    const ft_bool saved_line = (history_count == 1 && ft_strcmp(history[0], "hello") == 0);
    cma_free(accepted);
    char *cancelled = ft_nullptr;
    const int32_t cancelled_status = run_readline_from_text("hel\t", &cancelled);
    const ft_bool no_result = (cancelled == ft_nullptr);
    cma_free(cancelled);
    int32_t dynamic_count = -1;
    const int32_t completion_status = rl_completion_get_dynamic_count(&dynamic_count);
    const int32_t entries_after_cancel = history_count;
    const int32_t reset_status = rl_set_completion_callback(ft_nullptr, ft_nullptr);
    rl_clear_history();

    FT_ASSERT_EQ(FT_ERR_SUCCESS, accepted_status);
    FT_ASSERT_EQ(FT_TRUE, returned_line);
    FT_ASSERT_EQ(FT_TRUE, saved_line);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cancelled_status);
    FT_ASSERT_EQ(FT_TRUE, no_result);
    FT_ASSERT_EQ(1, entries_after_cancel);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, completion_status);
    FT_ASSERT_EQ(0, dynamic_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reset_status);
    return (1);
}

static int32_t readline_session_failing_candidate(const char *, int32_t, const char *, void *)
{
    (void)rl_completion_add_candidate("hello");
    return (FT_ERR_INTERNAL);
}

FT_TEST(test_readline_session_navigates_long_history_entries)
{
    rl_clear_history();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_enable_auto_save(FT_FALSE));
    char long_entry[INITIAL_BUFFER_SIZE + 32];
    ft_memset(long_entry, 'x', sizeof(long_entry) - 1);
    long_entry[sizeof(long_entry) - 1] = '\0';
    rl_update_history("older command");
    rl_update_history(long_entry);

    char *recalled_line = ft_nullptr;
    const int32_t recalled_status = run_readline_from_text("\033[A\033[A\033[B\n", &recalled_line);
    const ft_bool recalled_latest = (recalled_line != ft_nullptr && ft_strcmp(recalled_line, long_entry) == 0);
    cma_free(recalled_line);
    char *blank_line = ft_nullptr;
    const int32_t blank_status = run_readline_from_text("\033[A\033[B\n", &blank_line);
    const ft_bool returned_empty = (blank_line != ft_nullptr && blank_line[0] == '\0');
    cma_free(blank_line);
    rl_clear_history();

    FT_ASSERT_EQ(FT_ERR_SUCCESS, recalled_status);
    FT_ASSERT_EQ(FT_TRUE, recalled_latest);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, blank_status);
    FT_ASSERT_EQ(FT_TRUE, returned_empty);
    return (1);
}

FT_TEST(test_readline_session_releases_candidates_on_failed_completion_callback)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_set_completion_callback(readline_session_failing_candidate, ft_nullptr));
    char *line = ft_nullptr;
    const int32_t session_status = run_readline_from_text("hel\t", &line);
    const ft_bool no_result = (line == ft_nullptr);
    cma_free(line);
    int32_t dynamic_count = -1;
    const int32_t count_status = rl_completion_get_dynamic_count(&dynamic_count);
    const int32_t reset_status = rl_set_completion_callback(ft_nullptr, ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session_status);
    FT_ASSERT_EQ(FT_TRUE, no_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, count_status);
    FT_ASSERT_EQ(0, dynamic_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reset_status);
    return (1);
}
