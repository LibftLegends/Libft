#ifndef CONFIG_ENTRIES_INTERNAL_HPP
# define CONFIG_ENTRIES_INTERNAL_HPP

#include "config.hpp"
#include "../CMA/CMA.hpp"

static inline int32_t config_reserve_entries(config_data &config,
    ft_size_t &capacity, ft_size_t required)
{
    constexpr ft_size_t MAX_ENTRIES = SIZE_MAX / sizeof(config_entry);
    if (required <= capacity)
        return (FT_ERR_SUCCESS);
    if (required > MAX_ENTRIES)
        return (FT_ERR_OUT_OF_RANGE);
    ft_size_t new_capacity = required;
    if (capacity == 0U && required < 8U)
        new_capacity = 8U;
    else if (capacity <= MAX_ENTRIES / 2U && capacity * 2U > required)
        new_capacity = capacity * 2U;
    auto *entries = static_cast<config_entry *>(
        cma_realloc(config.entries, new_capacity * sizeof(config_entry)));
    if (entries == ft_nullptr && new_capacity != required)
    {
        new_capacity = required;
        entries = static_cast<config_entry *>(
            cma_realloc(config.entries, new_capacity * sizeof(config_entry)));
    }
    if (entries == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    config.entries = entries;
    capacity = new_capacity;
    return (FT_ERR_SUCCESS);
}

#endif
