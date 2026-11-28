#include <climits>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../Printf/printf.hpp"
#include "../Basic/basic.hpp"
#include "readline_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

int32_t rl_handle_printable_char(readline_state_t *state, char character, const char *prompt)
{
    ft_bool lock_acquired;
    int32_t  result;
    int32_t  lock_error;
    const char *terminator;
    int32_t buffer_length;
    int32_t  length_after_cursor;

    if (state == ft_nullptr || prompt == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_acquired = FT_FALSE;
    result = FT_ERR_INTERNAL;
    lock_error = rl_state_lock(state, &lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (state->buffer == ft_nullptr || state->buffer_size <= 0)
        goto cleanup;
    if (state->position < 0)
        goto cleanup;
    terminator = static_cast<const char *>(ft_memchr(state->buffer, '\0', state->buffer_size));
    if (terminator == ft_nullptr)
        goto cleanup;
    buffer_length = static_cast<int32_t>(terminator - state->buffer);
    if (state->position > buffer_length)
        goto cleanup;
    if (buffer_length >= state->buffer_size - 1)
    {
        if (state->buffer_size > INT_MAX / 2)
            goto cleanup;
        const int32_t resize_error = rl_resize_buffer(&state->buffer, &state->buffer_size,
            state->buffer_size * 2);
        if (resize_error != FT_ERR_SUCCESS)
        {
            result = resize_error;
            goto cleanup;
        }
    }
    if (state->position < buffer_length)
        (void)ft_memmove(&state->buffer[state->position + 1], &state->buffer[state->position],
            static_cast<ft_size_t>(buffer_length - state->position) + 1);
    else
        state->buffer[buffer_length + 1] = '\0';
    state->buffer[state->position] = character;
    state->position++;
    if (rl_update_display_metrics(state) != FT_ERR_SUCCESS)
        goto cleanup;
    if (rl_clear_line_columns(prompt, state->prev_display_columns) != FT_ERR_SUCCESS)
        goto cleanup;
    pf_printf("%s%s", prompt, state->buffer);
    length_after_cursor = state->prev_display_columns - state->display_pos;
    if (length_after_cursor > 0)
        pf_printf("\033[%dD", length_after_cursor);
    fflush(stdout);
    result = FT_ERR_SUCCESS;
cleanup:
    (void)rl_state_unlock(state, lock_acquired);
    return (result);
}
