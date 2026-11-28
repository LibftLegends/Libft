#include <new>
#include "json_internal.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include "../CPP_class/class_big_number.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_bool json_string_exceeds_signed_long_long(const char *value)
{
    if (value == ft_nullptr)
        return (FT_FALSE);
    const ft_bool negative = *value == '-';
    if (negative || *value == '+')
        ++value;
    while (*value == '0')
        ++value;
    const char *end = value;
    while (*end >= '0' && *end <= '9')
        ++end;
    if (*end != '\0' || end - value < 19)
        return (FT_FALSE);
    if (end - value > 19)
        return (FT_TRUE);
    if (negative)
        return (ft_strcmp(value, "9223372036854775808") > 0);
    return (ft_strcmp(value, "9223372036854775807") > 0);
}

int32_t json_build_item_number(const char *value, ft_big_number *&number)
{
    number = ft_nullptr;
    if (json_string_exceeds_signed_long_long(value) == FT_FALSE)
        return (FT_ERR_SUCCESS);
    ft_big_number *allocated = new (std::nothrow) ft_big_number;
    if (allocated == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    const int32_t initialize_error = allocated->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete allocated;
        return (initialize_error);
    }
    if (*value == '+')
        ++value;
    allocated->assign(value);
    // A freshly initialized big number has no digits if its first allocation fails.
    if (allocated->empty())
    {
        delete allocated;
        return (FT_ERR_NO_MEMORY);
    }
    number = allocated;
    return (FT_ERR_SUCCESS);
}

void json_item_refresh_numeric_state(json_item *item)
{
    if (item == ft_nullptr)
        return ;
    if (item->_mutex == ft_nullptr && json_item_enable_thread_safety(item) != FT_ERR_SUCCESS)
        return ;
    if (pt_recursive_mutex_lock_if_not_null(item->_mutex) != FT_ERR_SUCCESS)
        return ;
    ft_big_number *replacement;
    const int32_t error = json_build_item_number(item->value, replacement);
    delete item->big_number;
    item->big_number = replacement;
    item->is_big_number = replacement != ft_nullptr;
    json_item_set_error_unlocked(item, error);
    (void)pt_recursive_mutex_unlock_if_not_null(item->_mutex);
    return ;
}

void json_add_item_to_group(json_group *group, json_item *item)
{
    int32_t lock_error;

    if (group == ft_nullptr || item == ft_nullptr)
    {
        json_group_set_error(group, FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    if (group->_mutex == ft_nullptr)
        return ;
    lock_error = pt_recursive_mutex_lock_if_not_null(group->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (!group->items)
        group->items = item;
    else
    {
        json_item *current_item = group->items;

        while (current_item->next)
            current_item = current_item->next;
        current_item->next = item;
    }
    json_group_set_error_unlocked(group, FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    return ;
}

json_group *json_create_group_owned(char *name)
{
    json_group *group = new (std::nothrow) json_group{};
    if (!group)
        return (ft_nullptr);
    if (json_group_enable_thread_safety(group) != FT_ERR_SUCCESS)
    {
        delete group;
        return (ft_nullptr);
    }
    group->name = name;
    return (group);
}

json_group *json_create_json_group(const char *name)
{
    char *owned_name = adv_strdup(name);
    if (owned_name == ft_nullptr)
        return (ft_nullptr);
    json_group *group = json_create_group_owned(owned_name);
    if (group == ft_nullptr)
        cma_free(owned_name);
    return (group);
}
void json_append_group(json_group **head, json_group *new_group)
{
    int32_t lock_error;

    if (head == ft_nullptr || new_group == ft_nullptr)
    {
        json_group_set_error(new_group, FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    lock_error = json_group_list_lock_manual();
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (!(*head))
        *head = new_group;
    else
    {
        json_group *current_group = *head;

        while (current_group->next)
            current_group = current_group->next;
        current_group->next = new_group;
    }
    json_group_set_error(new_group, FT_ERR_SUCCESS);
    (void)json_group_list_unlock_manual();
    return ;
}

void json_free_items(json_item *item)
{
    while (item)
    {
        json_item *next_item = item->next;
        if (item->key)
            cma_free(item->key);
        if (item->value)
            cma_free(item->value);
        if (item->big_number)
            delete item->big_number;
        (void)json_item_disable_thread_safety(item);
        delete item;
        item = next_item;
    }
    return ;
}

void json_free_groups(json_group *group)
{
    int32_t lock_error;

    lock_error = json_group_list_lock_manual();
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    while (group)
    {
        json_group *next_group = group->next;

        if (group->name)
            cma_free(group->name);
        json_free_items(group->items);
        (void)json_group_disable_thread_safety(group);
        delete group;
        group = next_group;
    }
    (void)json_group_list_unlock_manual();
    return ;
}
