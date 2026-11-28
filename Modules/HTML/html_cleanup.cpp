#include "html_parser.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"

static void release_html_string(char *string)
{
    if (!string)
        return ;
    const int32_t release_result = cma_checked_free(string);
    if (release_result != 0)
        return ;
    return ;
}

void html_free_nodes(html_node *node_list)
{
    while (node_list)
    {
        html_node *next_node = node_list->next;
        if (node_list->children)
        {
            if (next_node)
            {
                // Put the remaining siblings after this node's children.
                html_node *last_child = node_list->children;
                while (last_child->next)
                    last_child = last_child->next;
                last_child->next = next_node;
            }
            next_node = node_list->children;
        }

        html_attr *current_attribute = node_list->attributes;
        while (current_attribute)
        {
            html_attr *next_attribute = current_attribute->next;
            release_html_string(current_attribute->key);
            release_html_string(current_attribute->value);
            delete current_attribute;
            current_attribute = next_attribute;
        }
        release_html_string(node_list->tag);
        release_html_string(node_list->text);
        delete node_list;
        node_list = next_node;
    }
    return ;
}

template <typename Predicate>
static void html_remove_matching_nodes(html_node **node_list, const Predicate &matches)
{
    while (*node_list)
    {
        html_node *current_node = *node_list;
        if (matches(current_node))
        {
            *node_list = current_node->next;
            current_node->next = ft_nullptr;
            html_free_nodes(current_node);
            continue ;
        }
        if (current_node->children)
            html_remove_matching_nodes(&current_node->children, matches);
        node_list = &current_node->next;
    }
    return ;
}

void html_remove_nodes_by_tag(html_node **node_list, const char *tag_name)
{
    if (node_list == ft_nullptr || tag_name == ft_nullptr)
        return ;
    html_remove_matching_nodes(node_list, [tag_name](const html_node *node) -> ft_bool
    {
        return (node->tag && ft_strcmp(node->tag, tag_name) == 0);
    });
    return ;
}

void html_remove_nodes_by_attr(html_node **node_list, const char *key, const char *value)
{
    if (node_list == ft_nullptr || key == ft_nullptr || value == ft_nullptr)
        return ;
    html_remove_matching_nodes(node_list, [key, value](const html_node *node) -> ft_bool
    {
        const html_attr *attribute = node->attributes;
        while (attribute)
        {
            if (attribute->key && attribute->value &&
                ft_strcmp(attribute->key, key) == 0 &&
                ft_strcmp(attribute->value, value) == 0)
                return (FT_TRUE);
            attribute = attribute->next;
        }
        return (FT_FALSE);
    });
    return ;
}

void html_remove_nodes_by_text(html_node **node_list, const char *text_content)
{
    if (node_list == ft_nullptr || text_content == ft_nullptr)
        return ;
    html_remove_matching_nodes(node_list, [text_content](const html_node *node) -> ft_bool
    {
        return (node->text && ft_strcmp(node->text, text_content) == 0);
    });
    return ;
}
