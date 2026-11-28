#include "advanced.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"


char    *adv_strdup(const char *string)
{
    if (string == ft_nullptr)
        return (ft_nullptr);
    return (ft_span_dup(string, ft_strlen_size_t(string)));
}
