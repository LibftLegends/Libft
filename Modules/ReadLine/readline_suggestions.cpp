#include "../Advanced/advanced.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Printf/printf.hpp"
#include "readline.hpp"
#include "readline_internal.hpp"

void rl_add_suggestion(const char *word)
{
    if (word == ft_nullptr)
        return ;
    int32_t begin = 0;
    int32_t end = suggestion_count;
    while (begin < end)
    {
        const int32_t middle = begin + (end - begin) / 2;
        const int32_t comparison = ft_strcmp(suggestions[middle], word);
        if (comparison == 0)
            return ;
        if (comparison < 0)
            begin = middle + 1;
        else
            end = middle;
    }
    if (suggestion_count == MAX_SUGGESTIONS)
    {
        pf_printf_fd(2, "Suggestion list full\n");
        return ;
    }
    char *new_suggestion = adv_strdup(word);
    if (new_suggestion == ft_nullptr)
        return ;
    if (begin < suggestion_count)
        (void)ft_memmove(suggestions + begin + 1, suggestions + begin,
            static_cast<ft_size_t>(suggestion_count - begin) * sizeof(suggestions[0]));
    suggestions[begin] = new_suggestion;
    ++suggestion_count;
    return ;
}

void rl_clear_suggestions()
{
    while (suggestion_count > 0)
    {
        --suggestion_count;
        cma_free(suggestions[suggestion_count]);
        suggestions[suggestion_count] = ft_nullptr;
    }
    return ;
}
