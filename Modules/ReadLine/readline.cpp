#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Printf/printf.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "readline_internal.hpp"
#include "readline.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

char    *history[MAX_HISTORY];
int32_t        history_count = 0;
char    *suggestions[MAX_SUGGESTIONS];
int32_t        suggestion_count = 0;

static void rl_cleanup_state(readline_state_t *state)
{
    ft_bool lock_acquired;

    if (state == ft_nullptr)
        return ;
    lock_acquired = FT_FALSE;
    if (rl_state_lock(state, &lock_acquired) == FT_ERR_SUCCESS)
    {
        if (state->buffer)
        {
            cma_free(state->buffer);
            state->buffer = ft_nullptr;
        }
        rl_state_unlock(state, lock_acquired);
    }
    rl_state_teardown_thread_safety(state);
    return ;
}

static char *rl_error(readline_state_t *state)
{
    rl_completion_reset_dynamic_matches();
    rl_cleanup_state(state);
    (void)rl_disable_raw_mode();
    return (ft_nullptr);
}

char *rl_readline(const char *prompt)
{
    readline_state_t state;
    if (rl_initialize_state(&state) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    pf_printf("%s", prompt);
    fflush(stdout);
    while (1)
    {
        char character;
        if (rl_read_key(&character) != FT_ERR_SUCCESS)
            return (rl_error(&state));
        const int32_t key_code = static_cast<uint8_t>(character);
        ft_bool custom_key_handled = FT_FALSE;
        if (rl_dispatch_custom_key(&state, prompt, key_code, &custom_key_handled) != FT_ERR_SUCCESS)
            return (rl_error(&state));
        if (custom_key_handled == FT_TRUE)
            continue ;

        if (character != '\t' && state.in_completion_mode)
        {
            state.in_completion_mode = 0;
            state.current_match_count = 0;
            state.current_match_index = 0;
            rl_completion_reset_dynamic_matches();
        }
        if (character == '\r' || character == '\n')
        {
            pf_printf("\n");
            break ;
        }
        int32_t result = FT_ERR_SUCCESS;
        if (character == 127 || character == '\b')
            result = rl_handle_backspace(&state, prompt);
        else if (character == 27)
            result = rl_handle_escape_sequence(&state, prompt);
        else if (character == '\t')
            result = rl_handle_tab_completion(&state, prompt);
        else if (character >= 32 && character <= 126)
            result = rl_handle_printable_char(&state, character, prompt);
        if (result != FT_ERR_SUCCESS)
            return (rl_error(&state));
    }
    rl_update_history(state.buffer);
    (void)rl_disable_raw_mode();
    rl_state_teardown_thread_safety(&state);
    if (DEBUG == 1)
        pf_printf("returning %s\n", state.buffer);
    return (state.buffer);
}
