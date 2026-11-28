#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../Printf/printf.hpp"
#include "../Basic/limits.hpp"
#include "../Basic/basic.hpp"
#include "../Template/algorithm.hpp"
#include "../Errno/errno.hpp"
#include "readline_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t rl_find_word_start_and_prefix(readline_state_t *state, char *prefix, int32_t *prefix_len, int32_t prefix_capacity)
{
    state->word_start = state->position - 1;
    while (state->word_start >= 0 && state->buffer[state->word_start] != ' ')
        state->word_start--;
    state->word_start++;
    *prefix_len = state->position - state->word_start;
    if (*prefix_len == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    if (*prefix_len >= prefix_capacity)
    {
        prefix[0] = '\0';
        *prefix_len = 0;
        return (FT_ERR_OUT_OF_RANGE);
    }
    ft_memcpy(prefix, &state->buffer[state->word_start], *prefix_len);
    prefix[*prefix_len] = '\0';
    return (FT_ERR_SUCCESS);
}

static int32_t rl_completion_score_prefix_match(const char *candidate, const char *prefix, int32_t prefix_len)
{
    ft_size_t candidate_length;

    if (candidate == ft_nullptr || prefix == ft_nullptr)
        return (-1);
    if (prefix_len <= 0)
        return (-1);
    if (ft_strncmp(candidate, prefix, prefix_len) != FT_ERR_SUCCESS)
        return (-1);
    candidate_length = ft_strlen(candidate);
    if (candidate_length > static_cast<ft_size_t>(FT_INT32_MAX))
        return (-1);
    return (100000 - static_cast<int32_t>(candidate_length));
}

static int32_t rl_completion_score_fuzzy_match(const char *candidate, const char *prefix, int32_t prefix_len)
{
    int32_t candidate_index;
    int32_t prefix_index;
    int32_t previous_match_index;
    int32_t gap_penalty;
    ft_size_t candidate_length;

    if (candidate == ft_nullptr || prefix == ft_nullptr)
        return (-1);
    if (prefix_len <= 0)
        return (-1);
    candidate_index = 0;
    prefix_index = 0;
    previous_match_index = -1;
    gap_penalty = 0;
    while (candidate[candidate_index] != '\0' && prefix_index < prefix_len)
    {
        if (candidate[candidate_index] == prefix[prefix_index])
        {
            if (previous_match_index >= 0)
                gap_penalty += candidate_index - previous_match_index - 1;
            previous_match_index = candidate_index;
            prefix_index += 1;
        }
        candidate_index += 1;
    }
    if (prefix_index != prefix_len)
        return (-1);
    candidate_length = ft_strlen(candidate);
    if (candidate_length > static_cast<ft_size_t>(FT_INT32_MAX))
        return (-1);
    return (50000 - gap_penalty - static_cast<int32_t>(candidate_length));
}

struct rl_ranked_match
{
    char *candidate;
    int32_t score;
    int32_t original_order;
};

static int32_t rl_gather_matching_suggestions(readline_state_t *state, const char *prefix, int32_t prefix_len)
{
    state->current_match_count = 0;
    int32_t dynamic_count;
    if (rl_completion_get_dynamic_count(&dynamic_count) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    rl_ranked_match matches[MAX_SUGGESTIONS];
    int32_t match_count = 0;
    int32_t candidate_index = 0;
    const int32_t candidate_count = dynamic_count + suggestion_count;
    while (candidate_index < candidate_count)
    {
        char *candidate;
        if (candidate_index < dynamic_count)
            candidate = rl_completion_get_dynamic_match(candidate_index);
        else
            candidate = suggestions[candidate_index - dynamic_count];
        int32_t score = rl_completion_score_prefix_match(candidate, prefix, prefix_len);
        if (score < 0)
            score = rl_completion_score_fuzzy_match(candidate, prefix, prefix_len);
        if (score >= 0)
        {
            if (match_count == MAX_SUGGESTIONS)
                return (FT_ERR_INTERNAL);
            matches[match_count] = {candidate, score, candidate_index};
            ++match_count;
        }
        ++candidate_index;
    }
    ft_sort(matches, matches + match_count, [](const rl_ranked_match &left,
        const rl_ranked_match &right) noexcept
    {
        if (left.score != right.score)
            return (left.score > right.score);
        const int32_t name_comparison = ft_strcmp(left.candidate, right.candidate);
        if (name_comparison != 0)
            return (name_comparison < 0);
        return (left.original_order < right.original_order);
    });
    int32_t match_index = 0;
    while (match_index < match_count)
    {
        state->current_matches[match_index] = matches[match_index].candidate;
        state->current_match_scores[match_index] = matches[match_index].score;
        ++match_index;
    }
    state->current_match_count = match_count;
    return (FT_ERR_SUCCESS);
}

static int32_t rl_resize_buffer_if_needed(readline_state_t *state, int32_t required_size)
{
    if (required_size >= state->buffer_size)
    {
        int32_t new_bufsize;

        new_bufsize = state->buffer_size;
        if (new_bufsize <= 0)
            new_bufsize = 1;
        while (required_size >= new_bufsize)
        {
            if (new_bufsize > FT_INT32_MAX / 2)
                return (FT_ERR_OUT_OF_RANGE);
            new_bufsize *= 2;
        }
        int32_t resize_error = rl_resize_buffer(&state->buffer, &state->buffer_size, new_bufsize);
        if (resize_error != FT_ERR_SUCCESS)
            return (resize_error);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t rl_apply_completion(readline_state_t *state, const char *completion)
{
    int32_t completion_length;
    int32_t original_position;
    int32_t suffix_length;
    int64_t total_length;
    int32_t required_size;

    completion_length = ft_strlen(completion);
    original_position = state->position;
    suffix_length = ft_strlen(&state->buffer[original_position]);
    total_length = static_cast<int64_t>(state->word_start)
        + static_cast<int64_t>(completion_length)
        + static_cast<int64_t>(suffix_length);
    if (total_length > static_cast<int64_t>(FT_INT32_MAX))
        return (FT_ERR_OUT_OF_RANGE);
    required_size = static_cast<int32_t>(total_length);
    int32_t resize_error = rl_resize_buffer_if_needed(state, required_size);
    if (resize_error != FT_ERR_SUCCESS)
        return (resize_error);
    state->position = state->word_start;
    ft_memmove(&state->buffer[state->position + completion_length],
        &state->buffer[original_position],
        static_cast<ft_size_t>(suffix_length) + 1);
    if (completion_length > 0)
        ft_memcpy(&state->buffer[state->position], completion,
            static_cast<ft_size_t>(completion_length));
    state->position += completion_length;
    return (FT_ERR_SUCCESS);
}

static void rl_update_display(const char *prompt, readline_state_t *state)
{
    int32_t columns_after_cursor;

    if (rl_update_display_metrics(state) != FT_ERR_SUCCESS)
        return ;
    if (rl_clear_line_columns(prompt, state->prev_display_columns) != FT_ERR_SUCCESS)
        return ;
    pf_printf("%s%s", prompt, state->buffer);
    columns_after_cursor = state->prev_display_columns - state->display_pos;
    if (columns_after_cursor > 0)
        pf_printf("\033[%dD", columns_after_cursor);
    fflush(stdout);
    return ;
}

int32_t rl_handle_tab_completion(readline_state_t *state, const char *prompt)
{
    ft_bool        lock_acquired;
    int32_t         result;
    const char *completion;
    int32_t         lock_result;

    if (state == ft_nullptr || prompt == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_acquired = FT_FALSE;
    result = FT_ERR_SUCCESS;
    completion = ft_nullptr;
    lock_result = rl_state_lock(state, &lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    if (!state->in_completion_mode)
    {
        char prefix[INITIAL_BUFFER_SIZE];
        int32_t prefix_len;
        int32_t prefix_status;

        prefix_status = rl_find_word_start_and_prefix(state, prefix, &prefix_len, INITIAL_BUFFER_SIZE);
        if (prefix_status != FT_ERR_SUCCESS)
        {
            result = prefix_status;
            goto cleanup;
        }
        if (prefix_len == FT_ERR_SUCCESS)
            goto cleanup_success;
        if (rl_completion_prepare_candidates(state->buffer, state->position, prefix, prefix_len) != FT_ERR_SUCCESS)
        {
            result = FT_ERR_INTERNAL;
            goto cleanup;
        }
        if (rl_gather_matching_suggestions(state, prefix, prefix_len) != FT_ERR_SUCCESS)
        {
            result = FT_ERR_INTERNAL;
            goto cleanup;
        }
        if (state->current_match_count == FT_ERR_SUCCESS)
        {
            pf_printf("\a");
            fflush(stdout);
            goto cleanup_success;
        }
        state->in_completion_mode = 1;
        state->current_match_index = 0;
    }
    if (state->in_completion_mode && state->current_match_count > 0)
    {
        completion = state->current_matches[state->current_match_index];
        int32_t completion_result = rl_apply_completion(state, completion);
        if (completion_result != FT_ERR_SUCCESS)
        {
            result = completion_result;
            goto cleanup;
        }
        rl_update_display(prompt, state);
        state->current_match_index = (state->current_match_index + 1) % state->current_match_count;
    }
cleanup_success:
    result = FT_ERR_SUCCESS;
cleanup:
    (void)rl_state_unlock(state, lock_acquired);
    return (result);
}
