#include "advanced.hpp"
#include <cstddef>
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"

char *adv_strndup(const char *string, ft_size_t maximum_length)
{
    if (string == ft_nullptr)
        return (ft_nullptr);
    return (ft_span_dup(string, ft_strnlen(string, maximum_length)));
}
