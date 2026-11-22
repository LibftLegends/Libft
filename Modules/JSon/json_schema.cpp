#include "json_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <algorithm>
#include <new>
#include <string_view>

static constexpr ft_size_t JSON_SCHEMA_INDEX_THRESHOLD = 64U;

struct json_schema_lookup_entry
{
    std::string_view key;
    ft_size_t order;
    ft_bool is_number;
    ft_bool is_boolean;
};

static ft_bool json_is_number(const char *string)
{
    if (string == ft_nullptr)
        return (FT_FALSE);
    while (*string == ' ' || (*string >= '\t' && *string <= '\r'))
        ++string;
    if (*string == '-' || *string == '+')
        ++string;
    const char *digits = string;
    while (*string >= '0' && *string <= '9')
        ++string;
    if (string == digits || *string != '\0')
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool json_is_bool(const char *string)
{
    if (ft_strcmp(string, "true") == FT_ERR_SUCCESS)
        return (FT_TRUE);
    if (ft_strcmp(string, "false") == FT_ERR_SUCCESS)
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool json_schema_has_many_fields(const json_schema_field *field)
{
    ft_size_t count = 0U;
    while (field != ft_nullptr && count < JSON_SCHEMA_INDEX_THRESHOLD)
    {
        ++count;
        field = field->next;
    }
    return (count == JSON_SCHEMA_INDEX_THRESHOLD);
}

static json_schema_lookup_entry *json_schema_snapshot_group(json_group *group, ft_size_t &count)
{
    count = 0U;
    if (pt_recursive_mutex_lock_if_not_null(group->_mutex) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    ft_size_t key_bytes = 0U;
    const json_item *item = group->items;
    while (item != ft_nullptr)
    {
        if (item->key != ft_nullptr)
        {
            const ft_size_t length = ft_strlen_size_t(item->key);
            if (length > FT_SYSTEM_SIZE_MAX - key_bytes)
                break ;
            key_bytes += length;
            ++count;
        }
        item = item->next;
    }
    json_schema_lookup_entry *entries = ft_nullptr;
    if (item == ft_nullptr && count >= JSON_SCHEMA_INDEX_THRESHOLD
        && count <= (FT_SYSTEM_SIZE_MAX - key_bytes) / sizeof(*entries))
    {
        entries = static_cast<json_schema_lookup_entry *>(
            cma_malloc(count * sizeof(*entries) + key_bytes));
    }
    if (entries == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
        return (ft_nullptr);
    }
    // Keep the copied keys and type checks independent of subsequent group edits.
    char *key_storage = reinterpret_cast<char *>(entries) + count * sizeof(*entries);
    ft_size_t index = 0U;
    item = group->items;
    while (item != ft_nullptr)
    {
        if (item->key != ft_nullptr)
        {
            const ft_size_t length = ft_strlen_size_t(item->key);
            ft_memcpy(key_storage, item->key, length);
            const ft_bool is_number = json_is_number(item->value);
            ft_bool is_boolean = FT_FALSE;
            if (!is_number)
                is_boolean = json_is_bool(item->value);
            new (entries + index) json_schema_lookup_entry{
                std::string_view(key_storage, length), index, is_number, is_boolean};
            key_storage += length;
            ++index;
        }
        item = item->next;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    std::sort(entries, entries + count, [](const json_schema_lookup_entry &left,
        const json_schema_lookup_entry &right)
    {
        const int32_t order = left.key.compare(right.key);
        if (order == 0)
            return (left.order < right.order);
        return (order < 0);
    });
    return (entries);
}

static int32_t json_schema_lookup(const json_schema_lookup_entry *entries,
    ft_size_t count, const json_schema_field *field)
{
    const std::string_view key(field->key);
    const json_schema_lookup_entry *end = entries + count;
    const json_schema_lookup_entry *entry = std::lower_bound(entries, end, key,
        [](const json_schema_lookup_entry &candidate, std::string_view match)
        {
            return (candidate.key < match);
        });
    if (entry == end || entry->key != key)
        return (FT_ERR_NOT_FOUND);
    if ((field->type == JSON_NUMBER && entry->is_number == FT_FALSE)
        || (field->type == JSON_BOOL && entry->is_boolean == FT_FALSE))
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

static int32_t json_schema_lookup_linear(json_group *group, const json_schema_field *field)
{
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(group->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const json_item *item = json_find_item_locked(group, field->key);
    int32_t result = FT_ERR_SUCCESS;
    if (item == ft_nullptr)
    {
        result = FT_ERR_NOT_FOUND;
        json_group_set_error_unlocked(group, FT_ERR_NOT_FOUND);
    }
    else
    {
        if ((field->type == JSON_NUMBER && json_is_number(item->value) == FT_FALSE)
            || (field->type == JSON_BOOL && json_is_bool(item->value) == FT_FALSE))
            result = FT_ERR_INVALID_ARGUMENT;
        json_group_set_error_unlocked(group, FT_ERR_SUCCESS);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    return (result);
}

ft_bool json_validate_schema(json_group *group, const json_schema &schema)
{
    if (group == ft_nullptr)
        return (FT_FALSE);
    json_schema *schema_mutable = const_cast<json_schema *>(&schema);
    if (schema_mutable->_mutex == ft_nullptr
        && json_schema_enable_thread_safety(schema_mutable) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (pt_recursive_mutex_lock_if_not_null(schema_mutable->_mutex) != FT_ERR_SUCCESS)
        return (FT_FALSE);

    json_schema_lookup_entry *entries = ft_nullptr;
    ft_size_t count = 0U;
    if (json_schema_has_many_fields(schema_mutable->fields))
    {
        (void)pt_recursive_mutex_unlock_if_not_null(schema_mutable->_mutex);
        entries = json_schema_snapshot_group(group, count);
        if (pt_recursive_mutex_lock_if_not_null(schema_mutable->_mutex) != FT_ERR_SUCCESS)
        {
            if (entries != ft_nullptr)
                cma_free(entries);
            return (FT_FALSE);
        }
    }
    int32_t validation_error = FT_ERR_SUCCESS;
    int32_t group_error = FT_ERR_SUCCESS;
    ft_bool looked_up_field = FT_FALSE;
    json_schema_field *field = schema_mutable->fields;
    while (field != ft_nullptr)
    {
        if (field->_mutex == ft_nullptr)
        {
            validation_error = json_schema_field_enable_thread_safety(field);
            if (validation_error != FT_ERR_SUCCESS)
                break ;
        }
        validation_error = pt_recursive_mutex_lock_if_not_null(field->_mutex);
        if (validation_error != FT_ERR_SUCCESS)
            break ;
        int32_t field_error = FT_ERR_INVALID_ARGUMENT;
        if (field->key != ft_nullptr)
        {
            if (entries != ft_nullptr)
            {
                field_error = json_schema_lookup(entries, count, field);
                looked_up_field = FT_TRUE;
                group_error = FT_ERR_SUCCESS;
                if (field_error == FT_ERR_NOT_FOUND)
                    group_error = FT_ERR_NOT_FOUND;
            }
            else
                field_error = json_schema_lookup_linear(group, field);
        }
        json_schema_field_set_error_unlocked(field, field_error);
        if (field_error != FT_ERR_NOT_FOUND || field->required == FT_TRUE)
            validation_error = field_error;
        json_schema_field *next_field = field->next;
        (void)pt_recursive_mutex_unlock_if_not_null(field->_mutex);
        if (validation_error != FT_ERR_SUCCESS)
            break ;
        field = next_field;
    }
    json_schema_set_error_unlocked(schema_mutable, validation_error);
    (void)pt_recursive_mutex_unlock_if_not_null(schema_mutable->_mutex);
    if (entries != ft_nullptr)
    {
        cma_free(entries);
        if (looked_up_field && pt_recursive_mutex_lock_if_not_null(group->_mutex) == FT_ERR_SUCCESS)
        {
            json_group_set_error_unlocked(group, group_error);
            (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
        }
    }
    return (validation_error == FT_ERR_SUCCESS);
}
