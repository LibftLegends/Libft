#include "readline_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

s_readline_state::s_readline_state(void)
{
    this->buffer = ft_nullptr;
    this->buffer_size = 0;
    this->position = 0;
    this->prev_buffer_length = 0;
    this->display_pos = 0;
    this->prev_display_columns = 0;
    this->history_index = 0;
    this->in_completion_mode = 0;
    this->current_match_count = 0;
    this->current_match_index = 0;
    this->word_start = 0;
    this->mutex = ft_nullptr;
    return ;
}

s_readline_state::~s_readline_state(void)
{
    return ;
}
