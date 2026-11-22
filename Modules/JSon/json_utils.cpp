#include <charconv>
#include "json_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_big_number.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static thread_local int32_t g_json_utils_last_error = FT_ERR_SUCCESS;

static void json_utils_set_error(int32_t error_code)
{
    g_json_utils_last_error = error_code;
    return ;
}

json_item *json_find_item_locked(json_group *group, const char *key)
{
    json_item *current;

    current = group->items;
    while (current)
    {
        if (current->key && ft_strcmp(current->key, key) == FT_ERR_SUCCESS)
            return (current);
        current = current->next;
    }
    return (ft_nullptr);
}

json_group *json_find_group(json_group *head, const char *name)
{
    int32_t lock_error;
    json_group *current;

    if (name == ft_nullptr)
    {
        json_group_set_error(head, FT_ERR_INVALID_ARGUMENT);
        json_utils_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    lock_error = json_group_list_lock_manual();
    if (lock_error != FT_ERR_SUCCESS)
    {
        json_utils_set_error(lock_error);
        return (ft_nullptr);
    }
    current = head;
    while (current)
    {
        if (current->name && ft_strcmp(current->name, name) == FT_ERR_SUCCESS)
        {
            json_group_set_error(current, FT_ERR_SUCCESS);
            json_utils_set_error(FT_ERR_SUCCESS);
            (void)json_group_list_unlock_manual();
            return (current);
        }
        current = current->next;
    }
    json_group_set_error(head, FT_ERR_NOT_FOUND);
    (void)json_group_list_unlock_manual();
    json_utils_set_error(FT_ERR_NOT_FOUND);
    return (ft_nullptr);
}

json_item *json_find_item(json_group *group, const char *key)
{
    int32_t lock_error;
    json_item *current;

    if (group == ft_nullptr || key == ft_nullptr)
    {
        json_group_set_error(group, FT_ERR_INVALID_ARGUMENT);
        json_utils_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(group->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        json_utils_set_error(lock_error);
        return (ft_nullptr);
    }
    current = json_find_item_locked(group, key);
    if (current != ft_nullptr)
    {
        json_group_set_error_unlocked(group, FT_ERR_SUCCESS);
        (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
        json_utils_set_error(FT_ERR_SUCCESS);
        return (current);
    }
    json_group_set_error_unlocked(group, FT_ERR_NOT_FOUND);
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    json_utils_set_error(FT_ERR_NOT_FOUND);
    return (ft_nullptr);
}

void json_remove_item(json_group *group, const char *key)
{
    int32_t lock_error;
    json_item *current;
    json_item *previous;

    if (group == ft_nullptr || key == ft_nullptr)
    {
        json_group_set_error(group, FT_ERR_INVALID_ARGUMENT);
        json_utils_set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(group->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        json_utils_set_error(lock_error);
        return ;
    }
    current = group->items;
    previous = ft_nullptr;
    while (current)
    {
        if (current->key && ft_strcmp(current->key, key) == FT_ERR_SUCCESS)
        {
            if (previous)
                previous->next = current->next;
            else
                group->items = current->next;
            if (current->key)
                cma_free(current->key);
            if (current->value)
                cma_free(current->value);
            if (current->big_number)
                delete current->big_number;
            (void)json_item_disable_thread_safety(current);
            delete current;
            json_group_set_error_unlocked(group, FT_ERR_SUCCESS);
            (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
            json_utils_set_error(FT_ERR_SUCCESS);
            return ;
        }
        previous = current;
        current = current->next;
    }
    json_group_set_error_unlocked(group, FT_ERR_NOT_FOUND);
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    json_utils_set_error(FT_ERR_NOT_FOUND);
    return ;
}

static int32_t json_replace_item_value(json_item *item, const char *value)
{
    if (item->_mutex == ft_nullptr)
    {
        const int32_t mutex_error = json_item_enable_thread_safety(item);
        if (mutex_error != FT_ERR_SUCCESS)
            return (mutex_error);
    }
    if (item->_error_code == FT_ERR_SUCCESS && item->value != ft_nullptr
        && ft_strcmp(item->value, value) == 0)
        return (FT_ERR_SUCCESS);

    char *replacement = adv_strdup(value);
    if (replacement == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    ft_big_number *number;
    const int32_t error = json_build_item_number(replacement, number);
    if (error != FT_ERR_SUCCESS)
    {
        cma_free(replacement);
        return (error);
    }
    char *previous_value = item->value;
    ft_big_number *previous_number = item->big_number;
    item->value = replacement;
    item->big_number = number;
    item->is_big_number = number != ft_nullptr;
    cma_free(previous_value);
    delete previous_number;
    return (FT_ERR_SUCCESS);
}

int32_t json_update_item_checked(json_group *group, const char *key, const char *value)
{
    if (group == ft_nullptr || key == ft_nullptr || value == ft_nullptr)
    {
        json_group_set_error(group, FT_ERR_INVALID_ARGUMENT);
        json_utils_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(group->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        json_utils_set_error(lock_error);
        return (lock_error);
    }
    json_item *item = json_find_item_locked(group, key);
    int32_t error = FT_ERR_NOT_FOUND;
    if (item != ft_nullptr)
    {
        error = json_replace_item_value(item, value);
        json_item_set_error_unlocked(item, error);
    }
    json_group_set_error_unlocked(group, error);
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    json_utils_set_error(error);
    return (error);
}

int32_t json_update_item_checked(json_group *group, const char *key, int32_t value)
{
    char buffer[12U];
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer) - 1U, value);
    *converted.ptr = '\0';
    return (json_update_item_checked(group, key, buffer));
}

int32_t json_update_item_checked(json_group *group, const char *key, ft_bool value)
{
    if (value == FT_TRUE)
        return (json_update_item_checked(group, key, "true"));
    return (json_update_item_checked(group, key, "false"));
}

int32_t json_update_item_checked(json_group *group, const char *key, const ft_big_number &value)
{
    return (json_update_item_checked(group, key, value.c_str()));
}

void json_update_item(json_group *group, const char *key, const char *value)
{
    (void)json_update_item_checked(group, key, value);
    return ;
}

void json_update_item(json_group *group, const char *key, int32_t value)
{
    (void)json_update_item_checked(group, key, value);
    return ;
}

void json_update_item(json_group *group, const char *key, ft_bool value)
{
    (void)json_update_item_checked(group, key, value);
    return ;
}

void json_update_item(json_group *group, const char *key, const ft_big_number &value)
{
    (void)json_update_item_checked(group, key, value);
    return ;
}

void json_remove_group(json_group **head, const char *name)
{
    int32_t lock_error;
    json_group *current;
    json_group *previous;

    if (!head || !(*head) || !name)
    {
        json_utils_set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    lock_error = json_group_list_lock_manual();
    if (lock_error != FT_ERR_SUCCESS)
    {
        json_utils_set_error(lock_error);
        return ;
    }
    current = *head;
    previous = ft_nullptr;
    while (current)
    {
        if (current->name && ft_strcmp(current->name, name) == FT_ERR_SUCCESS)
        {
            if (previous)
                previous->next = current->next;
            else
                *head = current->next;
            if (current->name)
                cma_free(current->name);
            json_free_items(current->items);
            (void)json_group_disable_thread_safety(current);
            delete current;
            (void)json_group_list_unlock_manual();
            json_utils_set_error(FT_ERR_SUCCESS);
            return ;
        }
        previous = current;
        current = current->next;
    }
    (void)json_group_list_unlock_manual();
    json_utils_set_error(FT_ERR_NOT_FOUND);
    return ;
}
