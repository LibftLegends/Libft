#include "config.hpp"
#include "config_entries_internal.hpp"
#include "../Advanced/advanced.hpp"
#include "../PThread/pthread_internal.hpp"
#include <functional>

static ft_bool config_keys_equal(const char *left, const char *right)
{
    return (left == right || (left != ft_nullptr && right != ft_nullptr
        && ft_strcmp(left, right) == 0));
}

static uint64_t config_hash_key(uint64_t hash, const char *text)
{
    constexpr uint64_t HASH_MULTIPLIER = 1099511628211ULL;
    if (text == ft_nullptr)
        return ((hash ^ 1U) * HASH_MULTIPLIER);
    while (*text != '\0')
    {
        hash = (hash ^ static_cast<uint8_t>(*text)) * HASH_MULTIPLIER;
        text += 1U;
    }
    return (hash * HASH_MULTIPLIER);
}

static ft_size_t *config_make_merge_index(ft_size_t entry_count, ft_size_t &slot_count)
{
    if (entry_count < 16U || entry_count > SIZE_MAX / sizeof(ft_size_t) / 4U)
        return (ft_nullptr);
    slot_count = 32U;
    while (slot_count < entry_count * 2U)
        slot_count *= 2U;
    return (static_cast<ft_size_t *>(adv_calloc(slot_count, sizeof(ft_size_t))));
}

static ft_size_t config_find_merge_slot(const config_data &result,
    const config_entry &source, const ft_size_t *slots, ft_size_t slot_count)
{
    uint64_t hash = config_hash_key(14695981039346656037ULL, source.section);
    hash = config_hash_key(hash, source.key);
    const ft_size_t mask = slot_count - 1U;
    ft_size_t slot = hash & mask;
    while (slots[slot] != 0U)
    {
        const config_entry &candidate = result.entries[slots[slot] - 1U];
        if (config_keys_equal(candidate.section, source.section)
            && config_keys_equal(candidate.key, source.key))
            break ;
        slot = (slot + 1U) & mask;
    }
    return (slot);
}

static ft_size_t config_find_merge_entry(const config_data &result,
    const config_entry &source)
{
    ft_size_t index = 0U;
    while (index < result.entry_count)
    {
        const config_entry &entry = result.entries[index];
        if (config_keys_equal(entry.section, source.section)
            && config_keys_equal(entry.key, source.key))
            break ;
        index += 1U;
    }
    return (index);
}

static int32_t config_append_copy(config_data &result, ft_size_t &capacity,
    const config_entry &source)
{
    int32_t error = config_reserve_entries(result, capacity, result.entry_count + 1U);
    if (error != FT_ERR_SUCCESS)
        return (error);
    config_entry &copy = result.entries[result.entry_count];
    copy = {};
    result.entry_count += 1U;
    if (source.section != ft_nullptr)
        copy.section = adv_strdup(source.section);
    if (source.key != ft_nullptr)
        copy.key = adv_strdup(source.key);
    if (source.value != ft_nullptr)
        copy.value = adv_strdup(source.value);
    if ((source.section != ft_nullptr && copy.section == ft_nullptr)
        || (source.key != ft_nullptr && copy.key == ft_nullptr)
        || (source.value != ft_nullptr && copy.value == ft_nullptr))
        return (FT_ERR_NO_MEMORY);
    return (config_entry_prepare_thread_safety(&copy));
}

static int32_t config_merge_override(config_data &result, ft_size_t &capacity,
    const config_entry &source, ft_size_t *slots, ft_size_t slot_count)
{
    ft_size_t slot = 0U;
    ft_size_t index = result.entry_count;
    if (slots == ft_nullptr)
        index = config_find_merge_entry(result, source);
    else
    {
        slot = config_find_merge_slot(result, source, slots, slot_count);
        if (slots[slot] != 0U)
            index = slots[slot] - 1U;
    }
    if (index == result.entry_count)
    {
        int32_t error = config_append_copy(result, capacity, source);
        if (error == FT_ERR_SUCCESS && slots != ft_nullptr)
            slots[slot] = index + 1U;
        return (error);
    }
    char *value = ft_nullptr;
    if (source.value != ft_nullptr)
    {
        value = adv_strdup(source.value);
        if (value == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    cma_free(result.entries[index].value);
    result.entries[index].value = value;
    return (FT_ERR_SUCCESS);
}

static int32_t config_merge_entries(config_data &result, ft_size_t &capacity,
    const config_data &source, ft_bool is_override, ft_size_t *slots, ft_size_t slot_count)
{
    ft_size_t index = 0U;
    while (index < source.entry_count)
    {
        config_entry *entry = &source.entries[index];
        ft_bool locked = FT_FALSE;
        int32_t error = config_entry_lock(entry, &locked);
        if (error != FT_ERR_SUCCESS)
            return (error);
        if (is_override == FT_TRUE)
            error = config_merge_override(result, capacity, *entry, slots, slot_count);
        else
        {
            const ft_size_t result_index = result.entry_count;
            error = config_append_copy(result, capacity, *entry);
            if (error == FT_ERR_SUCCESS && slots != ft_nullptr)
            {
                const ft_size_t slot = config_find_merge_slot(result, *entry, slots, slot_count);
                // Preserve duplicate base entries and index the first match.
                if (slots[slot] == 0U)
                    slots[slot] = result_index + 1U;
            }
        }
        config_entry_unlock(entry, locked);
        if (error != FT_ERR_SUCCESS)
            return (error);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t config_build_merge(config_data &result, const config_data *base_config,
    const config_data *override_config)
{
    ft_size_t base_count = 0U;
    ft_size_t override_count = 0U;
    if (base_config != ft_nullptr)
        base_count = base_config->entry_count;
    if (override_config != ft_nullptr)
        override_count = override_config->entry_count;
    if ((base_count != 0U && base_config->entries == ft_nullptr)
        || (override_count != 0U && override_config->entries == ft_nullptr)
        || base_count > SIZE_MAX / sizeof(config_entry)
        || override_count > SIZE_MAX / sizeof(config_entry) - base_count)
        return (FT_ERR_INVALID_STATE);
    ft_size_t capacity = 0U;
    int32_t error = config_reserve_entries(result, capacity, base_count);
    if (error != FT_ERR_SUCCESS)
        return (error);
    ft_size_t slot_count = 0U;
    ft_size_t *slots = ft_nullptr;
    // The index is optional. A failed allocation keeps the linear lookup available.
    if (override_count != 0U)
        slots = config_make_merge_index(base_count + override_count, slot_count);
    if (base_count != 0U)
        error = config_merge_entries(result, capacity, *base_config, FT_FALSE, slots, slot_count);
    if (error == FT_ERR_SUCCESS && override_count != 0U)
        error = config_merge_entries(result, capacity, *override_config, FT_TRUE, slots, slot_count);
    cma_free(slots);
    return (error);
}

config_data *config_merge(const config_data *base_config, const config_data *override_config)
{
    if (base_config == ft_nullptr && override_config == ft_nullptr)
        return (ft_nullptr);
    pt_mutex *first = ft_nullptr;
    pt_mutex *second = ft_nullptr;
    if (base_config != ft_nullptr)
        first = base_config->mutex;
    if (override_config != ft_nullptr)
        second = override_config->mutex;
    if (first == second)
        second = ft_nullptr;
    else if (first == ft_nullptr || (second != ft_nullptr
        && std::less<const pt_mutex *>{}(second, first)))
    {
        pt_mutex *previous_first = first;
        first = second;
        second = previous_first;
    }
    if (pt_mutex_lock_if_not_null(first) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (pt_mutex_lock_if_not_null(second) != FT_ERR_SUCCESS)
    {
        (void)pt_mutex_unlock_if_not_null(first);
        return (ft_nullptr);
    }
    config_data *result = config_data_create();
    int32_t error = FT_ERR_NO_MEMORY;
    if (result != ft_nullptr)
        error = config_build_merge(*result, base_config, override_config);
    (void)pt_mutex_unlock_if_not_null(second);
    (void)pt_mutex_unlock_if_not_null(first);
    if (error != FT_ERR_SUCCESS)
    {
        config_data_free(result);
        return (ft_nullptr);
    }
    return (result);
}
