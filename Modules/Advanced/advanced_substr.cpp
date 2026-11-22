#include <cstddef>
#include "advanced.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"

char *adv_substr(const char *source, uint32_t start, ft_size_t length)
{
    if (source == ft_nullptr)
        return (ft_nullptr);
    if (length == 0U || ft_strnlen(source, start) != start)
        return (ft_span_dup(ft_nullptr, 0U));
    const char *substring = source + start;
    return (ft_span_dup(substring, ft_strnlen(substring, length)));
}
