#include "readline_internal.hpp"
#include "../Compatebility/compatebility_internal.hpp"

int32_t rl_get_terminal_width(int32_t *terminal_width)
{
    constexpr int32_t DEFAULT_COLUMNS = 80;
    if (terminal_width == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (cmp_readline_terminal_width(terminal_width) != FT_ERR_SUCCESS || *terminal_width <= 0)
        *terminal_width = DEFAULT_COLUMNS;
    return (FT_ERR_SUCCESS);
}
