#include <cstddef>
#include "advanced.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"

static ft_bool is_in_set(char character, const char *set)
{
    if (set == ft_nullptr)
        return (FT_FALSE);
    ft_size_t index = 0;
    while (set[index] != '\0')
    {
        if (set[index] == character)
            return (FT_TRUE);
        index += 1;
    }
    return (FT_FALSE);
}

char *adv_strtrim(const char *input_string, const char *set)
{
    if (input_string == ft_nullptr || set == ft_nullptr)
        return (ft_nullptr);
    ft_size_t length = ft_strlen_size_t(input_string);
    ft_size_t start = 0;
    while (start < length && is_in_set(input_string[start], set))
        start += 1;
    ft_size_t end = length;
    while (end > start && is_in_set(input_string[end - 1], set))
        end -= 1;
    return (ft_span_dup(input_string + start, end - start));
}
