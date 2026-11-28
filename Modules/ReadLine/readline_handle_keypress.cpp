#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../Advanced/advanced.hpp"
#include "../CMA/CMA.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Basic/basic.hpp"
#include "readline_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

int32_t rl_handle_backspace(readline_state_t *state, const char *prompt)
{
    ft_bool lock_acquired;
    int32_t  result;
    int32_t  lock_error;

    if (state == ft_nullptr || prompt == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_acquired = FT_FALSE;
    result = FT_ERR_SUCCESS;
    lock_error = rl_state_lock(state, &lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (state->position > 0)
    {
        int32_t grapheme_start;
        int32_t grapheme_end;
        int32_t grapheme_columns;
        int32_t len_after_cursor;
        int32_t tail_length;

        if (rl_utf8_find_previous_grapheme(state->buffer, state->position,
                &grapheme_start, &grapheme_end, &grapheme_columns) == FT_ERR_SUCCESS)
        {
            (void)grapheme_columns;
            tail_length = ft_strlen(state->buffer) - grapheme_end + 1;
            ft_memmove(&state->buffer[grapheme_start],
                &state->buffer[grapheme_end], tail_length);
            state->position = grapheme_start;
            if (rl_update_display_metrics(state) != FT_ERR_SUCCESS
                || rl_clear_line_columns(prompt, state->prev_display_columns) != FT_ERR_SUCCESS)
            {
                state->error_file.printf("clear line failed");
                result = FT_ERR_INTERNAL;
                goto cleanup;
            }
            pf_printf("%s%s", prompt, state->buffer);
            len_after_cursor = state->prev_display_columns - state->display_pos;
            if (len_after_cursor > 0)
                pf_printf("\033[%dD", len_after_cursor);
            fflush(stdout);
        }
    }
cleanup:
    (void)rl_state_unlock(state, lock_acquired);
    return (result);
}

static void rl_handle_left_arrow(readline_state_t *state, const char *prompt)
{
    if (state->position > 0)
    {
        int32_t grapheme_start;
        int32_t grapheme_end;
        int32_t grapheme_columns;

        if (rl_utf8_find_previous_grapheme(state->buffer, state->position,
                &grapheme_start, &grapheme_end, &grapheme_columns) == FT_ERR_SUCCESS)
        {
            (void)grapheme_end;
            (void)grapheme_columns;
            state->position = grapheme_start;
            if (rl_update_display_metrics(state) != FT_ERR_SUCCESS)
                return ;
            if (rl_clear_line_columns(prompt, state->prev_display_columns) != FT_ERR_SUCCESS)
                return ;
            pf_printf("%s%s", prompt, state->buffer);
            int32_t len_after_cursor = state->prev_display_columns - state->display_pos;
            if (len_after_cursor > 0)
                pf_printf("\033[%dD", len_after_cursor);
            fflush(stdout);
        }
    }
    return ;
}

static void rl_handle_right_arrow(readline_state_t *state, const char *prompt)
{
    int32_t grapheme_start;
    int32_t grapheme_end;
    int32_t grapheme_columns;
    if (rl_utf8_find_next_grapheme(state->buffer, state->position, &grapheme_start,
            &grapheme_end, &grapheme_columns) != FT_ERR_SUCCESS)
        return ;
    state->position = grapheme_end;
    if (rl_update_display_metrics(state) != FT_ERR_SUCCESS)
        return ;
    if (rl_clear_line_columns(prompt, state->prev_display_columns) != FT_ERR_SUCCESS)
        return ;
    pf_printf("%s%s", prompt, state->buffer);
    const int32_t columns_after_cursor = state->prev_display_columns - state->display_pos;
    if (columns_after_cursor > 0)
        pf_printf("\033[%dD", columns_after_cursor);
    fflush(stdout);
    return ;
}

static int32_t rl_copy_history_entry_to_buffer(readline_state_t *state, const char *history_entry)
{
    if (history_entry == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_size_t entry_length = ft_strlen_size_t(history_entry);
    if (entry_length >= static_cast<ft_size_t>(FT_INT32_MAX))
        return (FT_ERR_OUT_OF_RANGE);
    const int32_t required_size = static_cast<int32_t>(entry_length) + 1;
    if (state->buffer == ft_nullptr || required_size > state->buffer_size)
    {
        int32_t new_size = state->buffer_size;
        if (new_size <= 0)
            new_size = 1;
        while (new_size < required_size)
        {
            if (new_size > FT_INT32_MAX / 2)
                new_size = required_size;
            else
                new_size *= 2;
        }
        const int32_t resize_error = rl_resize_buffer(&state->buffer, &state->buffer_size, new_size);
        if (resize_error != FT_ERR_SUCCESS)
            return (resize_error);
    }
    (void)ft_memcpy(state->buffer, history_entry, entry_length + 1);
    state->position = static_cast<int32_t>(entry_length);
    return (FT_ERR_SUCCESS);
}

static void rl_reset_completion_mode_locked(readline_state_t *state)
{
    if (state == ft_nullptr)
        return ;
    state->in_completion_mode = 0;
    state->current_match_count = 0;
    state->current_match_index = 0;
    rl_completion_reset_dynamic_matches();
    return ;
}

void rl_reset_completion_mode(readline_state_t *state)
{
    ft_bool lock_acquired;

    if (state == ft_nullptr)
        return ;
    lock_acquired = FT_FALSE;
    if (rl_state_lock(state, &lock_acquired) != FT_ERR_SUCCESS)
        return ;
    rl_reset_completion_mode_locked(state);
    rl_state_unlock(state, lock_acquired);
    return ;
}

static int32_t rl_reverse_history_search_redraw(readline_state_t *state, const char *prompt)
{
    int32_t len_after_cursor;

    if (rl_update_display_metrics(state) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (rl_clear_line_columns(prompt, state->prev_display_columns) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    pf_printf("%s%s", prompt, state->buffer);
    len_after_cursor = state->prev_display_columns - state->display_pos;
    if (len_after_cursor > 0)
        pf_printf("\033[%dD", len_after_cursor);
    fflush(stdout);
    return (FT_ERR_SUCCESS);
}

static int32_t rl_reverse_history_search_apply_current_match(readline_state_t *state, const char *prompt,
        const char *query, int32_t query_length, int32_t *match_index_out, ft_bool *has_match_out)
{
    int32_t search_result;
    int32_t match_index;

    (void)prompt;
    if (match_index_out != ft_nullptr)
        *match_index_out = -1;
    if (has_match_out != ft_nullptr)
        *has_match_out = FT_FALSE;
    if (query == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (query_length <= 0)
        return (FT_ERR_SUCCESS);
    match_index = -1;
    search_result = rl_history_search(query, history_count - 1, FT_TRUE, &match_index);
    if (search_result == FT_ERR_SUCCESS && match_index >= 0)
    {
        int32_t copy_result;

        copy_result = rl_copy_history_entry_to_buffer(state, history[match_index]);
        if (copy_result != FT_ERR_SUCCESS)
            return (copy_result);
        state->history_index = match_index;
        if (match_index_out != ft_nullptr)
            *match_index_out = match_index;
        if (has_match_out != ft_nullptr)
            *has_match_out = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t rl_handle_reverse_history_search_key(readline_state_t *state, const char *prompt, void *user_data)
{
    char query[INITIAL_BUFFER_SIZE];
    char *original_buffer;
    int32_t original_position;
    int32_t query_length;
    ft_bool lock_acquired;
    ft_bool keep_searching;
    ft_bool have_match;
    int32_t last_match_index;
    int32_t result;

    (void)user_data;
    if (state == ft_nullptr || prompt == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    original_buffer = ft_nullptr;
    lock_acquired = FT_FALSE;
    result = FT_ERR_SUCCESS;
    keep_searching = FT_TRUE;
    have_match = FT_FALSE;
    last_match_index = -1;
    if (rl_state_lock(state, &lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (state->buffer != ft_nullptr)
        original_buffer = adv_strdup(state->buffer);
    if (original_buffer == ft_nullptr)
    {
        result = FT_ERR_NO_MEMORY;
        goto cleanup;
    }
    original_position = state->position;
    query_length = 0;
    query[0] = '\0';
    if (state->in_completion_mode)
        rl_reset_completion_mode_locked(state);
    while (keep_searching == FT_TRUE)
    {
        char key_character;

        if (rl_reverse_history_search_redraw(state, prompt) != FT_ERR_SUCCESS)
        {
            result = FT_ERR_INTERNAL;
            break ;
        }
        if (rl_read_key(&key_character) != FT_ERR_SUCCESS)
        {
            result = FT_ERR_INTERNAL;
            break ;
        }
        if (key_character == '\r' || key_character == '\n')
        {
            if (have_match == FT_FALSE && query_length == 0)
            {
                int32_t restore_result;

                restore_result = rl_copy_history_entry_to_buffer(state, original_buffer);
                if (restore_result != FT_ERR_SUCCESS)
                {
                    result = restore_result;
                    break ;
                }
                state->position = original_position;
            }
            keep_searching = FT_FALSE;
            break ;
        }
        if (key_character == 27 || key_character == 7)
        {
            int32_t restore_result;

            restore_result = rl_copy_history_entry_to_buffer(state, original_buffer);
            if (restore_result != FT_ERR_SUCCESS)
            {
                result = restore_result;
                break ;
            }
            state->position = original_position;
            keep_searching = FT_FALSE;
            break ;
        }
        if (key_character == 127 || key_character == '\b')
        {
            if (query_length > 0)
            {
                query_length -= 1;
                query[query_length] = '\0';
                if (query_length == 0)
                {
                    int32_t restore_result;

                    restore_result = rl_copy_history_entry_to_buffer(state, original_buffer);
                    if (restore_result != FT_ERR_SUCCESS)
                    {
                        result = restore_result;
                        break ;
                    }
                    state->position = original_position;
                    have_match = FT_FALSE;
                    last_match_index = -1;
                }
                else
                {
                    int32_t match_result;
                    int32_t current_match_index;
                    ft_bool current_has_match;

                    current_match_index = -1;
                    current_has_match = FT_FALSE;
                    match_result = rl_reverse_history_search_apply_current_match(state, prompt,
                            query, query_length, &current_match_index, &current_has_match);
                    if (match_result == FT_ERR_SUCCESS)
                    {
                        have_match = current_has_match;
                        last_match_index = current_match_index;
                    }
                    else if (match_result != FT_ERR_NOT_FOUND)
                    {
                        result = match_result;
                        break ;
                    }
                }
            }
            continue ;
        }
        if (key_character >= 32 && key_character <= 126)
        {
            int32_t match_result;
            int32_t current_match_index;
            ft_bool current_has_match;

            if (query_length + 1 >= INITIAL_BUFFER_SIZE)
            {
                result = FT_ERR_OUT_OF_RANGE;
                break ;
            }
            query[query_length] = key_character;
            query_length += 1;
            query[query_length] = '\0';
            current_match_index = -1;
            current_has_match = FT_FALSE;
            match_result = rl_reverse_history_search_apply_current_match(state, prompt,
                    query, query_length, &current_match_index, &current_has_match);
            if (match_result == FT_ERR_SUCCESS)
            {
                have_match = current_has_match;
                last_match_index = current_match_index;
            }
            else if (match_result == FT_ERR_NOT_FOUND)
            {
                pf_printf("\a");
                fflush(stdout);
            }
            else
            {
                result = match_result;
                break ;
            }
            continue ;
        }
    }
    if (result != FT_ERR_SUCCESS)
    {
        int32_t restore_result;

        restore_result = rl_copy_history_entry_to_buffer(state, original_buffer);
        if (restore_result == FT_ERR_SUCCESS)
            state->position = original_position;
    }
    if (have_match == FT_TRUE && last_match_index >= 0)
        state->history_index = last_match_index;
    cma_free(original_buffer);
cleanup:
    (void)rl_state_unlock(state, lock_acquired);
    return (result);
}

int32_t rl_read_escape_sequence(char escape_sequence[2])
{
    ft_size_t received = 0;
    while (received < 2)
    {
        const int64_t count = su_read(STDIN_FILENO, escape_sequence + received, 2 - received);
        if (count <= 0)
            return (FT_ERR_INTERNAL);
        received += static_cast<ft_size_t>(count);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t rl_show_history_entry(readline_state_t *state, const char *prompt, int32_t history_index)
{
    if (rl_clear_line(prompt, state->buffer) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    const int32_t result = rl_copy_history_entry_to_buffer(state, history[history_index]);
    if (result != FT_ERR_SUCCESS)
        return (result);
    state->history_index = history_index;
    pf_printf("%s%s", prompt, state->buffer);
    if (rl_update_display_metrics(state) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    fflush(stdout);
    return (FT_ERR_SUCCESS);
}

static int32_t rl_handle_up_arrow(readline_state_t *state, const char *prompt)
{
    if (state->history_index > 0)
        return (rl_show_history_entry(state, prompt, state->history_index - 1));
    return (FT_ERR_SUCCESS);
}

static int32_t rl_handle_down_arrow(readline_state_t *state, const char *prompt)
{
    if (state->history_index < history_count - 1)
        return (rl_show_history_entry(state, prompt, state->history_index + 1));
    if (state->history_index == history_count - 1)
    {
        if (rl_clear_line(prompt, state->buffer) != FT_ERR_SUCCESS)
            return (FT_ERR_INTERNAL);
        state->history_index++;
        state->position = 0;
        state->buffer[0] = '\0';
        pf_printf("%s", prompt);
        state->display_pos = 0;
        state->prev_display_columns = 0;
        state->prev_buffer_length = 0;
        fflush(stdout);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t rl_handle_arrow_keys(readline_state_t *state, const char *prompt, char direction)
{
    if (direction == 'A')
        return (rl_handle_up_arrow(state, prompt));
    else if (direction == 'B')
        return (rl_handle_down_arrow(state, prompt));
    else if (direction == 'C')
        rl_handle_right_arrow(state, prompt);
    else if (direction == 'D')
        rl_handle_left_arrow(state, prompt);
    return (FT_ERR_SUCCESS);
}

int32_t rl_handle_escape_sequence(readline_state_t *state, const char *prompt)
{
    ft_bool lock_acquired;
    int32_t  result;
    int32_t  lock_error;
    char escape_sequence[2];

    if (state == ft_nullptr || prompt == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_acquired = FT_FALSE;
    result = FT_ERR_SUCCESS;
    lock_error = rl_state_lock(state, &lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (state->in_completion_mode)
        rl_reset_completion_mode_locked(state);
    if (rl_read_escape_sequence(escape_sequence) != FT_ERR_SUCCESS)
    {
        result = FT_ERR_INTERNAL;
        goto cleanup;
    }
    if (escape_sequence[0] == '[')
        result = rl_handle_arrow_keys(state, prompt, escape_sequence[1]);
cleanup:
    (void)rl_state_unlock(state, lock_acquired);
    return (result);
}
