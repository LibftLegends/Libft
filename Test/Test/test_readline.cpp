#include "../test_internal.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstring>
#include <thread>
#include "../../Modules/ReadLine/readline.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Advanced/advanced.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static void test_readline_cleanup_state(readline_state_t *state)
{
    ft_bool lock_acquired;
    int32_t lock_result;

    if (state == ft_nullptr)
        return ;
    lock_acquired = false;
    lock_result = rl_state_lock(state, &lock_acquired);
    if (lock_result == 0 && state->buffer != ft_nullptr)
    {
        cma_free(state->buffer);
        state->buffer = ft_nullptr;
    }
    if (lock_result == 0)
    {
        if (lock_acquired == true)
            rl_state_unlock(state, lock_acquired);
    }
    rl_state_teardown_thread_safety(state);
    return ;
}

static void test_readline_suppress_stderr(int *backup_descriptor)
{
    int null_descriptor;

    if (backup_descriptor == ft_nullptr)
        return ;
    *backup_descriptor = -1;
    null_descriptor = open(cmp_service_null_device_path(), O_WRONLY);
    if (null_descriptor == -1)
        return ;
    *backup_descriptor = dup(STDERR_FILENO);
    if (*backup_descriptor == -1)
    {
        close(null_descriptor);
        return ;
    }
    if (dup2(null_descriptor, STDERR_FILENO) == -1)
    {
        close(null_descriptor);
        close(*backup_descriptor);
        *backup_descriptor = -1;
        return ;
    }
    close(null_descriptor);
    return ;
}

static void test_readline_restore_stderr(int backup_descriptor)
{
    if (backup_descriptor == -1)
        return ;
    dup2(backup_descriptor, STDERR_FILENO);
    close(backup_descriptor);
    return ;
}

FT_TEST(test_readline_clear_line_null_prompt)
{
    int clear_result;
    const char *buffer;

    buffer = "";
    clear_result = rl_clear_line(ft_nullptr, buffer);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, clear_result);
    return (1);
}

FT_TEST(test_readline_clear_line_null_buffer)
{
    int clear_result;
    const char *prompt;

    prompt = "> ";
    clear_result = rl_clear_line(prompt, ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, clear_result);
    return (1);
}

FT_TEST(test_readline_initialize_state_null_pointer)
{
    int init_result;

    init_result = rl_initialize_state(ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, init_result);
    return (1);
}

FT_TEST(test_readline_initialize_state_allocation_failure)
{
    readline_state_t state;
    int init_result;

    cma_set_alloc_limit(1);
    init_result = rl_initialize_state(&state);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, init_result);
    return (1);
}

FT_TEST(test_readline_initialize_state_success)
{
    readline_state_t state;
    int init_result;
    char *allocated_buffer;
    int buffer_size;
    int buffer_position;
    int buffer_history_index;

    init_result = rl_initialize_state(&state);
    allocated_buffer = state.buffer;
    buffer_size = state.buffer_size;
    buffer_position = state.position;
    buffer_history_index = state.history_index;
    rl_disable_raw_mode();
    if (allocated_buffer != ft_nullptr)
        cma_free(allocated_buffer);
    rl_state_teardown_thread_safety(&state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, init_result);
    FT_ASSERT(allocated_buffer != ft_nullptr);
    FT_ASSERT_EQ(INITIAL_BUFFER_SIZE, buffer_size);
    FT_ASSERT_EQ(0, buffer_position);
    FT_ASSERT_EQ(history_count, buffer_history_index);
    return (1);
}

FT_TEST(test_readline_printable_char_preserves_buffer_on_resize_failure)
{
    readline_state_t state = {};
    const char *prompt;
    char *initial_buffer;
    int handle_result;
    int stderr_backup_descriptor;

    prompt = "> ";
    initial_buffer = static_cast<char *>(cma_malloc(4));
    if (initial_buffer == ft_nullptr)
        return (0);
    initial_buffer[0] = 'x';
    initial_buffer[1] = '\0';
    state.buffer = initial_buffer;
    state.buffer_size = 2;
    state.position = state.buffer_size - 1;
    state.prev_buffer_length = 1;
    state.display_pos = 1;
    state.prev_display_columns = 1;
    state.history_index = 0;
    state.in_completion_mode = 0;
    state.current_match_count = 0;
    state.current_match_index = 0;
    state.word_start = 0;
    stderr_backup_descriptor = -1;
    test_readline_suppress_stderr(&stderr_backup_descriptor);
    cma_set_alloc_limit(1);
    handle_result = rl_handle_printable_char(&state, 'a', prompt);
    cma_set_alloc_limit(0);
    test_readline_restore_stderr(stderr_backup_descriptor);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, handle_result);
    FT_ASSERT_EQ(initial_buffer, state.buffer);
    FT_ASSERT_EQ(2, state.buffer_size);
    FT_ASSERT_EQ(1, state.position);
    FT_ASSERT_EQ('x', state.buffer[0]);
    FT_ASSERT_EQ('\0', state.buffer[1]);
    cma_free(initial_buffer);
    return (1);
}

FT_TEST(test_readline_utf8_backspace_removes_grapheme)
{
    readline_state_t state;
    const char *prompt;
    char *buffer;
    int handle_result;
    const char initial_sequence[] = { 'A', static_cast<char>(0xC3), static_cast<char>(0xA9), 'B', '\0' };
    const char expected_first[] = { 'A', static_cast<char>(0xC3), static_cast<char>(0xA9), '\0' };
    const char expected_second[] = { 'A', '\0' };

    prompt = "> ";
    buffer = adv_strdup(initial_sequence);
    if (buffer == ft_nullptr)
        return (0);
    state.buffer = buffer;
    state.buffer_size = ft_strlen(buffer) + 1;
    state.position = ft_strlen(buffer);
    state.prev_buffer_length = ft_strlen(buffer);
    state.display_pos = 3;
    state.prev_display_columns = 3;
    state.history_index = 0;
    state.in_completion_mode = 0;
    state.current_match_count = 0;
    state.current_match_index = 0;
    state.word_start = 0;
    handle_result = rl_handle_backspace(&state, prompt);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle_result);
    FT_ASSERT(std::memcmp(state.buffer, expected_first, sizeof(expected_first)) == 0);
    FT_ASSERT_EQ(ft_strlen(state.buffer), state.position);
    FT_ASSERT_EQ(2, state.prev_display_columns);
    handle_result = rl_handle_backspace(&state, prompt);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle_result);
    FT_ASSERT(std::memcmp(state.buffer, expected_second, sizeof(expected_second)) == 0);
    FT_ASSERT_EQ(ft_strlen(state.buffer), state.position);
    FT_ASSERT_EQ(1, state.prev_display_columns);
    cma_free(buffer);
    return (1);
}

FT_TEST(test_readline_utf8_printable_updates_display)
{
    readline_state_t state;
    const char *prompt;
    char *buffer;

    prompt = "> ";
    buffer = static_cast<char *>(cma_malloc(16));
    if (buffer == ft_nullptr)
        return (0);
    state.buffer = buffer;
    state.buffer_size = 16;
    state.position = 0;
    state.prev_buffer_length = 0;
    state.display_pos = 0;
    state.prev_display_columns = 0;
    state.history_index = 0;
    state.in_completion_mode = 0;
    state.current_match_count = 0;
    state.current_match_index = 0;
    state.word_start = 0;
    ft_bzero(buffer, 16);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_handle_printable_char(&state, static_cast<char>(0xC3), prompt));
    FT_ASSERT_EQ(1, state.position);
    FT_ASSERT(state.prev_display_columns >= 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_handle_printable_char(&state, static_cast<char>(0xA9), prompt));
    FT_ASSERT_EQ(2, state.position);
    FT_ASSERT_EQ(1, state.prev_display_columns);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_handle_printable_char(&state, '!', prompt));
    FT_ASSERT_EQ(3, state.position);
    FT_ASSERT_EQ(2, state.prev_display_columns);
    cma_free(buffer);
    return (1);
}

FT_TEST(test_readline_state_insert_and_delete_text)
{
    readline_state_t state;
    int initialize_result;
    int insert_result;
    int delete_result;
    const char *buffer_pointer;
    int position;

    ft_bzero(&state, sizeof(state));
    initialize_result = rl_initialize_state(&state);
    if (initialize_result != FT_ERR_SUCCESS)
        return (0);
    state.position = 0;
    insert_result = rl_state_insert_text(&state, "hello");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, insert_result);
    FT_ASSERT(state.buffer_size >= 6);
    FT_ASSERT_EQ(5, state.position);
    FT_ASSERT_EQ(0, ft_strcmp(state.buffer, "hello"));
    state.position = ft_strlen("h\xC3\xA9");
    ft_strlcpy(state.buffer, "h\xC3\xA9", static_cast<size_t>(state.buffer_size));
    delete_result = rl_state_delete_previous_grapheme(&state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, delete_result);
    FT_ASSERT_EQ(1, state.position);
    FT_ASSERT_EQ(0, ft_strcmp(state.buffer, "h"));
    delete_result = rl_state_delete_previous_grapheme(&state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, delete_result);
    FT_ASSERT_EQ(0, state.position);
    buffer_pointer = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_state_get_buffer(&state, &buffer_pointer));
    FT_ASSERT(buffer_pointer == state.buffer);
    position = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_state_get_cursor(&state, &position));
    FT_ASSERT_EQ(0, position);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_state_set_cursor(&state, 0));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, rl_state_set_cursor(&state, INT_MAX));
    rl_disable_raw_mode();
    test_readline_cleanup_state(&state);
    return (1);
}
