#include "advanced.hpp"
#include <cstddef>
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"

void* adv_memdup(const void* source, ft_size_t size)
{
    if (size == 0)
        return (cma_malloc(0));
    if (source == ft_nullptr)
        return (ft_nullptr);
    void *duplicate = cma_malloc(size);
    if (duplicate == ft_nullptr)
        return (ft_nullptr);
    (void)ft_memcpy(duplicate, source, size);
    return (duplicate);
}
