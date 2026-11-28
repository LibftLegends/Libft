#include <new>
#include "html_parser.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include "../Advanced/advanced.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/limits.hpp"

static void html_release_string(char *string)
{
    if (!string)
        return ;
    int32_t release_result = cma_checked_free(string);
    if (release_result != 0)
        return ;
    return ;
}


html_node *html_create_node(const char *tag_name, const char *text_content)
{
    if (tag_name == ft_nullptr)
        return (ft_nullptr);
    html_node *new_node = new(std::nothrow) html_node;
    if (!new_node)
        return (ft_nullptr);
    new_node->tag = adv_strdup(tag_name);
    if (!new_node->tag)
    {
        delete new_node;
        return (ft_nullptr);
    }
    if (text_content)
        new_node->text = adv_strdup(text_content);
    else
        new_node->text = ft_nullptr;
    if (text_content && !new_node->text)
    {
        html_release_string(new_node->tag);
        delete new_node;
        return (ft_nullptr);
    }
    new_node->attributes = ft_nullptr;
    new_node->children = ft_nullptr;
    new_node->next = ft_nullptr;
    return (new_node);
}

html_attr *html_create_attr(const char *key, const char *value)
{
    if (key == ft_nullptr || value == ft_nullptr)
        return (ft_nullptr);
    html_attr *new_attr = new(std::nothrow) html_attr;
    if (!new_attr)
        return (ft_nullptr);
    new_attr->key = adv_strdup(key);
    if (!new_attr->key)
    {
        delete new_attr;
        return (ft_nullptr);
    }
    new_attr->value = adv_strdup(value);
    if (!new_attr->value)
    {
        html_release_string(new_attr->key);
        delete new_attr;
        return (ft_nullptr);
    }
    new_attr->next = ft_nullptr;
    return (new_attr);
}

void html_add_attr(html_node *target_node, html_attr *new_attribute)
{
    html_attr *current_attribute;

    if (target_node == ft_nullptr || new_attribute == ft_nullptr)
        return ;
    if (!target_node->attributes)
    {
        target_node->attributes = new_attribute;
        return ;
    }
    current_attribute = target_node->attributes;
    while (current_attribute->next)
        current_attribute = current_attribute->next;
    current_attribute->next = new_attribute;
    return ;
}

void html_remove_attr(html_node *target_node, const char *key)
{
    if (target_node == ft_nullptr || key == ft_nullptr)
        return ;
    html_attr **attribute_link = &target_node->attributes;
    while (*attribute_link)
    {
        html_attr *current_attribute = *attribute_link;
        if (current_attribute->key && ft_strcmp(current_attribute->key, key) == 0)
        {
            *attribute_link = current_attribute->next;
            html_release_string(current_attribute->key);
            html_release_string(current_attribute->value);
            delete current_attribute;
            return ;
        }
        attribute_link = &current_attribute->next;
    }
    return ;
}

void html_add_child(html_node *parent_node, html_node *child_node)
{
    if (parent_node == ft_nullptr)
        return ;
    html_append_node(&parent_node->children, child_node);
    return ;
}

void html_append_node(html_node **head_node, html_node *new_node)
{
    if (!head_node || new_node == ft_nullptr)
        return ;
    while (*head_node)
        head_node = &(*head_node)->next;
    *head_node = new_node;
    return ;
}
