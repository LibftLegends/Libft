#include "html_parser.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/basic.hpp"

template <typename Matches>
static html_node *html_find_node(html_node *node, const Matches &matches)
{
    while (node != ft_nullptr)
    {
        if (matches(*node))
            return (node);
        if (node->children != ft_nullptr)
        {
            html_node *found = html_find_node(node->children, matches);
            if (found != ft_nullptr)
                return (found);
        }
        node = node->next;
    }
    return (ft_nullptr);
}

template <typename Matches>
static html_node *html_find_attribute(html_node *nodes, const Matches &matches)
{
    return (html_find_node(nodes, [&](const html_node &node)
    {
        const html_attr *attribute = node.attributes;
        while (attribute != ft_nullptr)
        {
            if (matches(*attribute))
                return (FT_TRUE);
            attribute = attribute->next;
        }
        return (FT_FALSE);
    }));
}

html_node *html_find_by_tag(html_node *node_list, const char *tag_name)
{
    if (tag_name == ft_nullptr)
        return (ft_nullptr);
    return (html_find_node(node_list, [&](const html_node &node)
    {
        return (node.tag != ft_nullptr && ft_strcmp(node.tag, tag_name) == 0);
    }));
}

html_node *html_find_by_attr(html_node *node_list, const char *key, const char *value)
{
    if (key == ft_nullptr)
        return (ft_nullptr);
    return (html_find_attribute(node_list, [&](const html_attr &attribute)
    {
        return (attribute.key != ft_nullptr && ft_strcmp(attribute.key, key) == 0
            && (value == ft_nullptr
                || (attribute.value != ft_nullptr && ft_strcmp(attribute.value, value) == 0)));
    }));
}

html_node *html_find_by_text(html_node *node_list, const char *text_content)
{
    if (text_content == ft_nullptr)
        return (ft_nullptr);
    return (html_find_node(node_list, [&](const html_node &node)
    {
        return (node.text != ft_nullptr && ft_strcmp(node.text, text_content) == 0);
    }));
}

ft_size_t html_count_nodes_by_tag(html_node *node_list, const char *tag_name)
{
    if (tag_name == ft_nullptr)
        return (0);
    ft_size_t count = 0;
    const html_node *node = node_list;
    while (node != ft_nullptr)
    {
        if (node->tag != ft_nullptr && ft_strcmp(node->tag, tag_name) == 0)
            ++count;
        if (node->children != ft_nullptr)
            count += html_count_nodes_by_tag(node->children, tag_name);
        node = node->next;
    }
    return (count);
}

static ft_bool html_selector_matches(const char *string, const char *span, ft_size_t length)
{
    if (string == ft_nullptr)
        return (FT_FALSE);
    const char *const end = span + length;
    // Selector spans exclude the terminator, so a shorter candidate mismatches at its NUL.
    while (span != end && *string == *span)
    {
        ++span;
        ++string;
    }
    return (span == end && *string == '\0');
}

static html_node *html_find_by_attribute_selector(html_node *nodes, const char *selector)
{
    const char *equal_sign = ft_nullptr;
    const char *close_bracket = selector;
    while (*close_bracket != '\0' && *close_bracket != ']')
    {
        if (*close_bracket == '=' && equal_sign == ft_nullptr)
            equal_sign = close_bracket;
        ++close_bracket;
    }
    if (*close_bracket != ']')
        return (ft_nullptr);

    const char *key_end = close_bracket;
    const char *value = ft_nullptr;
    ft_size_t value_length = 0;
    if (equal_sign != ft_nullptr)
    {
        key_end = equal_sign;
        value = equal_sign + 1;
        value_length = static_cast<ft_size_t>(close_bracket - value);
        if (value_length >= 2 && (value[0] == '"' || value[0] == '\'')
            && value[0] == value[value_length - 1])
        {
            ++value;
            value_length -= 2;
        }
    }
    const ft_size_t key_length = static_cast<ft_size_t>(key_end - selector);
    return (html_find_attribute(nodes, [&](const html_attr &attribute)
    {
        return (html_selector_matches(attribute.key, selector, key_length)
            && (value == ft_nullptr || html_selector_matches(attribute.value, value, value_length)));
    }));
}

html_node *html_find_by_selector(html_node *node_list, const char *selector)
{
    if (selector != ft_nullptr && selector[0] == '[')
        return (html_find_by_attribute_selector(node_list, selector + 1));
    return (html_query_selector(node_list, selector));
}

html_node *html_query_selector(html_node *node_list, const char *selector)
{
    if (!selector)
        return (ft_nullptr);
    if (selector[0] == '#')
        return (html_find_by_attr(node_list, "id", selector + 1));
    if (selector[0] == '.')
        return (html_find_by_attr(node_list, "class", selector + 1));
    return (html_find_by_tag(node_list, selector));
}
