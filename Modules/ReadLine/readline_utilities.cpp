#include <stdlib.h>
#include <unistd.h>
#include "../Advanced/advanced.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Printf/printf.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "readline_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

int32_t rl_resize_buffer(char **buffer_pointer, int32_t *current_size_pointer, int32_t new_size)
{
    if (buffer_pointer == ft_nullptr || current_size_pointer == ft_nullptr || new_size <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t old_size = *current_size_pointer;
    if (*buffer_pointer == ft_nullptr || old_size < 0)
        old_size = 0;
    char *new_buffer = static_cast<char *>(cma_realloc(*buffer_pointer, static_cast<ft_size_t>(new_size)));
    if (new_buffer == ft_nullptr)
    {
        su_write(2, "Allocation error\n", 17);
        return (FT_ERR_NO_MEMORY);
    }
    if (new_size > old_size)
        ft_bzero(new_buffer + old_size, static_cast<ft_size_t>(new_size - old_size));
    *buffer_pointer = new_buffer;
    *current_size_pointer = new_size;
    return (FT_ERR_SUCCESS);
}

int32_t rl_clear_line(const char *prompt, const char *buffer)
{
    if (prompt == ft_nullptr || buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t buffer_columns;
    const int32_t result = rl_utf8_compute_columns(buffer, &buffer_columns);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (rl_clear_line_columns(prompt, buffer_columns));
}

int32_t rl_clear_line_columns(const char *prompt, int32_t buffer_columns)
{
    if (prompt == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t prompt_columns;
    const int32_t prompt_result = rl_utf8_compute_columns(prompt, &prompt_columns);
    if (prompt_result != FT_ERR_SUCCESS)
        return (prompt_result);
    const int64_t total_columns = static_cast<int64_t>(prompt_columns) + buffer_columns;
    pf_printf("\r");
    int32_t term_width;
    int32_t terminal_width_error;

    terminal_width_error = rl_get_terminal_width(&term_width);
    if (terminal_width_error != FT_ERR_SUCCESS || term_width <= 0)
        term_width = 1;
    const int64_t line_count = (total_columns / term_width) + 1;
    int64_t index = 0;
    while (index < line_count)
    {
        pf_printf("\033[2K");
        if (index < line_count - 1)
            pf_printf("\033[A");
        index++;
    }
    pf_printf("\r");
    return (FT_ERR_SUCCESS);
}

int32_t rl_read_key(char *character_out)
{
    int64_t bytes_read;
    char character;

    if (character_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);

    while (1)
    {
        bytes_read = su_read(0, &character, 1);
        if (bytes_read == 1)
        {
            *character_out = character;
            return (FT_ERR_SUCCESS);
        }
        if (bytes_read == FT_ERR_SUCCESS)
        {
            return (FT_ERR_TERMINATED);
        }
        if (bytes_read < 0)
        {
            return (FT_ERR_TERMINATED);
        }
    }
}

void rl_history_adopt_entry(char *entry)
{
    if (entry == ft_nullptr)
        return ;
    if (history_count == MAX_HISTORY)
    {
        cma_free(history[0]);
        (void)ft_memmove(history, history + 1, sizeof(history[0]) * (MAX_HISTORY - 1));
        --history_count;
    }
    history[history_count++] = entry;
    return ;
}

void rl_update_history(const char *buffer)
{
    char *entry = adv_strdup(buffer);
    if (entry == ft_nullptr)
        return ;
    rl_history_adopt_entry(entry);
    rl_history_notify_updated();
    return ;
}

int32_t rl_history_search(const char *query, int32_t start_index,
        ft_bool search_backward, int32_t *match_index)
{
    if (query == ft_nullptr || match_index == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *match_index = -1;
    const ft_size_t query_length = ft_strlen_size_t(query);
    if (query_length == 0 || ft_utf8_validate(query, query_length) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    const int32_t count = history_count;
    int32_t step = 1;
    int32_t current_index = start_index;
    if (search_backward)
        step = -1;
    if (current_index < 0 || current_index >= count)
    {
        current_index = 0;
        if (search_backward)
            current_index = count - 1;
    }
    while (current_index >= 0 && current_index < count)
    {
        const char *entry = history[current_index];
        if (entry != ft_nullptr)
        {
            const ft_size_t entry_length = ft_strlen_size_t(entry);
            if (ft_utf8_validate(entry, entry_length) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_ARGUMENT);
            // A valid UTF-8 query cannot start or end inside another code point.
            if (ft_memmem(entry, entry_length, query, query_length) != ft_nullptr)
            {
                *match_index = current_index;
                return (FT_ERR_SUCCESS);
            }
        }
        current_index += step;
    }
    return (FT_ERR_NOT_FOUND);
}
