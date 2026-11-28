#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static char redraw_output[256];
static ft_size_t redraw_output_length;

static int64_t capture_readline_output(int32_t descriptor, const void *output, ft_size_t length)
{
    if (descriptor == 2)
        return (static_cast<int64_t>(length));
    if (descriptor != 1 || length > sizeof(redraw_output) - redraw_output_length)
        return (-1);
    (void)ft_memcpy(redraw_output + redraw_output_length, output, length);
    redraw_output_length += length;
    return (static_cast<int64_t>(length));
}

FT_TEST(test_readline_printable_char_grows_full_line_before_inserting_in_middle)
{
    readline_state_t state;
    const char original[] = "abcd";
    state.buffer = static_cast<char *>(cma_malloc(sizeof(original)));
    FT_ASSERT(state.buffer != ft_nullptr);
    (void)ft_memcpy(state.buffer, original, sizeof(original));
    state.buffer_size = sizeof(original);
    state.position = 2;
    redraw_output_length = 0;
    su_set_write_syscall_hook(capture_readline_output);
    cma_set_alloc_limit(1);
    const int32_t failed = rl_handle_printable_char(&state, 'X', "> ");
    cma_set_alloc_limit(0);
    su_reset_write_syscall_hook();
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed);
    FT_ASSERT_EQ(sizeof(original), state.buffer_size);
    FT_ASSERT_EQ(2, state.position);
    FT_ASSERT_EQ(0, ft_strcmp(state.buffer, original));
    FT_ASSERT_EQ(0U, redraw_output_length);

    su_set_write_syscall_hook(capture_readline_output);
    const int32_t inserted = rl_handle_printable_char(&state, 'X', "> ");
    su_reset_write_syscall_hook();
    const char expected_suffix[] = "\r> abXcd\033[2D";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inserted);
    FT_ASSERT_EQ(3, state.position);
    FT_ASSERT(state.buffer_size > static_cast<int32_t>(sizeof(original)));
    FT_ASSERT_EQ(0, ft_strcmp(state.buffer, "abXcd"));
    FT_ASSERT_EQ(5, state.prev_display_columns);
    FT_ASSERT_EQ(3, state.display_pos);
    FT_ASSERT(redraw_output_length >= sizeof(expected_suffix) - 1);
    FT_ASSERT_EQ(0, ft_memcmp(redraw_output + redraw_output_length - (sizeof(expected_suffix) - 1),
        expected_suffix, sizeof(expected_suffix) - 1));
    cma_free(state.buffer);
    return (1);
}

FT_TEST(test_readline_printable_char_rejects_invalid_buffer_and_cursor)
{
    readline_state_t state;
    char unterminated[] = {'a', 'b', 'c'};
    state.buffer = unterminated;
    state.buffer_size = sizeof(unterminated);
    FT_ASSERT_EQ(FT_ERR_INTERNAL, rl_handle_printable_char(&state, 'X', "> "));
    FT_ASSERT_EQ('a', state.buffer[0]);
    char short_line[4] = "a";
    state.buffer = short_line;
    state.buffer_size = sizeof(short_line);
    state.position = 2;
    FT_ASSERT_EQ(FT_ERR_INTERNAL, rl_handle_printable_char(&state, 'X', "> "));
    FT_ASSERT_EQ(0, ft_strcmp(short_line, "a"));
    return (1);
}
