#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "readline_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static void rl_open_log_file(readline_state_t *state)
{
    static int32_t file_reset;

    if (file_reset == 0 && DEBUG == 1)
    {
        state->error_file.open("data/data--log", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        file_reset = 1;
    }
    else if (DEBUG == 1)
        state->error_file.open("data/data--log", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    return ;
}

static int32_t rl_initialize_buffer_locked(readline_state_t *state)
{
    char *buffer = state->buffer;
    if (buffer == ft_nullptr || state->buffer_size != INITIAL_BUFFER_SIZE)
        buffer = static_cast<char *>(cma_malloc(INITIAL_BUFFER_SIZE));
    if (buffer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);

    const int32_t result = rl_bind_key(RL_KEY_CTRL_R, rl_handle_reverse_history_search_key, ft_nullptr);
    if (result != FT_ERR_SUCCESS)
    {
        if (buffer != state->buffer)
            cma_free(buffer);
        return (result);
    }
    if (buffer != state->buffer)
        cma_free(state->buffer);
    state->buffer = buffer;
    state->buffer_size = INITIAL_BUFFER_SIZE;
    ft_bzero(state->buffer, INITIAL_BUFFER_SIZE);
    state->position = 0;
    state->prev_buffer_length = 0;
    state->display_pos = 0;
    state->prev_display_columns = 0;
    state->history_index = history_count;
    state->in_completion_mode = 0;
    state->current_match_count = 0;
    state->current_match_index = 0;
    state->word_start = 0;
    rl_open_log_file(state);
    return (FT_ERR_SUCCESS);
}

int32_t rl_initialize_state(readline_state_t *state)
{
    if (state == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_bool had_mutex = (state->mutex != ft_nullptr);
    int32_t result = rl_state_prepare_thread_safety(state);
    if (result != FT_ERR_SUCCESS)
        return (result);
    const ft_bool thread_safety_created = (!had_mutex && state->mutex != ft_nullptr);
    ft_bool raw_mode_activated = FT_FALSE;
    if (rl_enable_raw_mode(&raw_mode_activated) != FT_ERR_SUCCESS)
    {
        if (thread_safety_created == FT_TRUE)
            rl_state_teardown_thread_safety(state);
        return (FT_ERR_TERMINATED);
    }
    ft_bool lock_acquired = FT_FALSE;
    result = rl_state_lock(state, &lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = rl_initialize_buffer_locked(state);
        (void)rl_state_unlock(state, lock_acquired);
    }
    if (result != FT_ERR_SUCCESS)
    {
        if (raw_mode_activated == FT_TRUE)
            (void)rl_disable_raw_mode();
        if (thread_safety_created == FT_TRUE)
            rl_state_teardown_thread_safety(state);
    }
    return (result);
}
