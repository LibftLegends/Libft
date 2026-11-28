#ifndef HTML_PARSER_HPP
#define HTML_PARSER_HPP

#include "../Errno/errno.hpp"
typedef struct html_attr
{
    char *key;
    char *value;
    struct html_attr *next;
} html_attr;

typedef struct html_node
{
    char *tag;
    char *text;
    html_attr *attributes;
    struct html_node *children;
    struct html_node *next;
} html_node;

html_node   *html_create_node(const char *tag_name, const char *text_content);
void        html_add_child(html_node *parent_node, html_node *child_node);
void        html_append_node(html_node **head_node, html_node *new_node);
html_attr   *html_create_attr(const char *key, const char *value);
void        html_add_attr(html_node *target_node, html_attr *new_attribute);
void        html_remove_attr(html_node *target_node, const char *key);
int32_t     html_write_to_file(const char *file_path, html_node *node_list);
char        *html_write_to_string(html_node *node_list);
void        html_free_nodes(html_node *node_list);
void        html_remove_nodes_by_tag(html_node **node_list, const char *tag_name);
void        html_remove_nodes_by_attr(html_node **node_list, const char *key, const char *value);
void        html_remove_nodes_by_text(html_node **node_list, const char *text_content);
html_node   *html_find_by_tag(html_node *node_list, const char *tag_name);
html_node   *html_find_by_attr(html_node *node_list, const char *key, const char *value);
html_node   *html_find_by_text(html_node *node_list, const char *text_content);
// Matches tag, ID, class, attribute presence, or exact attribute value in document order.
html_node   *html_find_by_selector(html_node *node_list, const char *selector);

// Matches tag, ID, or class in document order.
html_node   *html_query_selector(html_node *node_list, const char *selector);
ft_size_t   html_count_nodes_by_tag(html_node *node_list, const char *tag_name);

#endif
