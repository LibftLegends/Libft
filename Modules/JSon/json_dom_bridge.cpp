#include "json_dom_bridge.hpp"
#include "json_internal.hpp"
#include "../CPP_class/class_big_number.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "document.hpp"

static void json_dom_delete_node(ft_dom_node *node) noexcept
{
    if (!node)
        return ;
    delete node;
    return ;
}

static void json_dom_delete_string(ft_string *string) noexcept
{
    if (!string)
        return ;
    (void)string->destroy();
    delete string;
    return ;
}

static int32_t json_dom_append_item(const json_item *item, ft_dom_node *group_node) noexcept
{
    int32_t node_initialize_error;

    if (!item || !group_node)
    {
        return (FT_ERR_INVALID_STATE);
    }
    ft_dom_node *item_node;

    item_node = new(std::nothrow) ft_dom_node();
    if (!item_node)
    {
        return (FT_ERR_INVALID_STATE);
    }
    node_initialize_error = item_node->initialize();
    if (node_initialize_error != FT_ERR_SUCCESS)
    {
        json_dom_delete_node(item_node);
        return (node_initialize_error);
    }
    item_node->set_type(FT_DOM_NODE_VALUE);
    const char *item_key;

    item_key = item->key;
    if (!item_key)
        item_key = "";
    if (item_node->set_name(item_key) != FT_ERR_SUCCESS)
    {
        json_dom_delete_node(item_node);
        return (FT_ERR_INVALID_STATE);
    }
    if (item->is_big_number)
    {
        if (!item->big_number)
        {
            json_dom_delete_node(item_node);
            return (FT_ERR_INVALID_STATE);
        }
        ft_string *number_string;

        number_string = item->big_number->to_string_base(10);
        if (!number_string)
        {
            json_dom_delete_node(item_node);
            return (FT_ERR_INVALID_STATE);
        }
        if (number_string->get_error() != FT_ERR_SUCCESS)
        {
            json_dom_delete_string(number_string);
            json_dom_delete_node(item_node);
            return (FT_ERR_INVALID_STATE);
        }
        if (item_node->set_value(*number_string) != FT_ERR_SUCCESS)
        {
            json_dom_delete_string(number_string);
            json_dom_delete_node(item_node);
            return (FT_ERR_INVALID_STATE);
        }
        json_dom_delete_string(number_string);
        if (item_node->add_attribute("json:type", "big_number") != FT_ERR_SUCCESS)
        {
            json_dom_delete_node(item_node);
            return (FT_ERR_INVALID_STATE);
        }
    }
    else
    {
        const char *item_value;

        item_value = item->value;
        if (!item_value)
            item_value = "";
        if (item_node->set_value(item_value) != FT_ERR_SUCCESS)
        {
            json_dom_delete_node(item_node);
            return (FT_ERR_INVALID_STATE);
        }
    }
    if (group_node->add_child(item_node) != FT_ERR_SUCCESS)
    {
        json_dom_delete_node(item_node);
        return (FT_ERR_INVALID_STATE);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t json_dom_append_group(const json_group *group, ft_dom_node *root_node) noexcept
{
    int32_t node_initialize_error;

    if (!group || !root_node)
    {
        return (FT_ERR_INVALID_STATE);
    }
    ft_dom_node *group_node;

    group_node = new(std::nothrow) ft_dom_node();
    if (!group_node)
    {
        return (FT_ERR_INVALID_STATE);
    }
    node_initialize_error = group_node->initialize();
    if (node_initialize_error != FT_ERR_SUCCESS)
    {
        json_dom_delete_node(group_node);
        return (node_initialize_error);
    }
    group_node->set_type(FT_DOM_NODE_OBJECT);
    const char *group_name;

    group_name = group->name;
    if (!group_name)
        group_name = "";
    if (group_node->set_name(group_name) != FT_ERR_SUCCESS)
    {
        json_dom_delete_node(group_node);
        return (FT_ERR_INVALID_STATE);
    }
    const json_item *item_iterator;

    item_iterator = group->items;
    while (item_iterator)
    {
        if (json_dom_append_item(item_iterator, group_node) != FT_ERR_SUCCESS)
        {
            json_dom_delete_node(group_node);
            return (FT_ERR_INVALID_STATE);
        }
        item_iterator = item_iterator->next;
    }
    if (root_node->add_child(group_node) != FT_ERR_SUCCESS)
    {
        json_dom_delete_node(group_node);
        return (FT_ERR_INVALID_STATE);
    }
    return (FT_ERR_SUCCESS);
}

int32_t json_document_to_dom(const json_document &document, ft_dom_document &dom) noexcept
{
    int32_t root_initialize_error;

    ft_dom_node *root_node;

    root_node = new(std::nothrow) ft_dom_node();
    if (!root_node)
    {
        return (FT_ERR_INVALID_STATE);
    }
    root_initialize_error = root_node->initialize();
    if (root_initialize_error != FT_ERR_SUCCESS)
    {
        json_dom_delete_node(root_node);
        return (root_initialize_error);
    }
    root_node->set_type(FT_DOM_NODE_OBJECT);
    if (root_node->set_name("json" ) != FT_ERR_SUCCESS)
    {
        json_dom_delete_node(root_node);
        return (FT_ERR_INVALID_STATE);
    }
    json_group *group_iterator;

    group_iterator = document.get_groups();
    while (group_iterator)
    {
        if (json_dom_append_group(group_iterator, root_node) != FT_ERR_SUCCESS)
        {
            json_dom_delete_node(root_node);
            return (FT_ERR_INVALID_STATE);
        }
        group_iterator = group_iterator->next;
    }
    dom.set_root(root_node);
    return (FT_ERR_SUCCESS);
}

static int32_t json_dom_has_big_number_type(const ft_dom_node &node, ft_bool &is_big) noexcept
{
    is_big = FT_FALSE;
    ft_bool locked = FT_FALSE;
    const int32_t error = node.lock(&locked);
    if (error != FT_ERR_SUCCESS)
        return (error);
    const auto &keys = node.get_attribute_keys();
    const auto &values = node.get_attribute_values();
    const ft_size_t count = keys.size();
    ft_size_t index = 0U;
    while (index < count)
    {
        if (keys[index] == "json:type")
        {
            is_big = values[index] == "big_number";
            break ;
        }
        ++index;
    }
    node.unlock(locked);
    return (FT_ERR_SUCCESS);
}

static int32_t json_dom_normalize_number(const char *&value, ft_string &normalized) noexcept
{
    const ft_bool negative = *value == '-';
    const char *digits = value + negative;
    const char *end = digits;
    while (*end >= '0' && *end <= '9')
        ++end;
    if (end == digits || *end != '\0')
    {
        value = "0";
        return (FT_ERR_SUCCESS);
    }
    while (*digits == '0' && digits[1] != '\0')
        ++digits;
    if (!negative || *digits == '0')
    {
        value = digits;
        return (FT_ERR_SUCCESS);
    }
    if (digits == value + 1)
        return (FT_ERR_SUCCESS);
    int32_t error = normalized.initialize("-");
    if (error == FT_ERR_SUCCESS)
        error = normalized.append(digits);
    if (error == FT_ERR_SUCCESS)
        value = normalized.c_str();
    return (error);
}

static int32_t json_dom_apply_item(const ft_dom_node *item_node, json_item *&item) noexcept
{
    if (item_node == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const char *key = item_node->get_name().c_str();
    const char *value = item_node->get_value().c_str();
    if (key == ft_nullptr || value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bool is_big;
    int32_t error = json_dom_has_big_number_type(*item_node, is_big);
    if (error != FT_ERR_SUCCESS)
        return (error);
    ft_string normalized;
    if (is_big)
    {
        error = json_dom_normalize_number(value, normalized);
        if (error != FT_ERR_SUCCESS)
            return (error);
    }
    item = json_create_item(key, value);
    if (item == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    return (json_item_get_error(item));
}

static int32_t json_dom_apply_group(const ft_dom_node *group_node, json_group *&group) noexcept
{
    if (group_node == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const char *name = group_node->get_name().c_str();
    if (name == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    group = json_create_json_group(name);
    if (group == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    const auto &children = group_node->get_children();
    const ft_size_t count = children.size();
    ft_size_t index = 0U;
    json_item **next_item = &group->items;
    while (index < count)
    {
        const int32_t error = json_dom_apply_item(children[index], *next_item);
        if (error != FT_ERR_SUCCESS)
            return (error);
        next_item = &(*next_item)->next;
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

int32_t json_document_from_dom(const ft_dom_document &dom, json_document &document) noexcept
{
    const ft_dom_node *root = dom.get_root();
    if (root == ft_nullptr || root->get_type() != FT_DOM_NODE_OBJECT)
    {
        document.set_manual_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_STATE);
    }
    const auto &groups = root->get_children();
    const ft_size_t count = groups.size();
    ft_size_t index = 0U;
    json_group *head = ft_nullptr;
    json_group **next_group = &head;
    int32_t error = FT_ERR_SUCCESS;
    while (index < count)
    {
        error = json_dom_apply_group(groups[index], *next_group);
        if (error != FT_ERR_SUCCESS)
            break ;
        next_group = &(*next_group)->next;
        ++index;
    }
    if (error == FT_ERR_SUCCESS)
        error = json_document_replace_groups(document, head);
    if (error != FT_ERR_SUCCESS)
    {
        json_free_groups(head);
        document.set_manual_error(error);
        return (FT_ERR_INVALID_STATE);
    }
    return (FT_ERR_SUCCESS);
}
