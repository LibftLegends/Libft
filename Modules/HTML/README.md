# HTML

The `HTML` module provides a lightweight linked-node HTML tree, free functions for direct tree manipulation, and `html_document`, a lifecycle wrapper that owns a root list and offers thread-local error state.

## Types

- `html_attr` - Linked-list attribute node with `key`, `value`, and `next`.
- `html_node` - Linked-list tree node with `tag`, `text`, attributes, children, and sibling `next`.
- `html_document` - Lifecycle-managed document wrapper around an `html_node` root.

## Free-Function Tree API

- `html_create_node(const char *tag_name, const char *text_content)` - Allocates one node with optional text.
- `html_add_child(html_node *parent_node, html_node *child_node)` - Appends a child to a parent node.
- `html_append_node(html_node **head_node, html_node *new_node)` - Appends a node to a sibling list.
- `html_create_attr(const char *key, const char *value)` - Allocates one attribute.
- `html_add_attr(html_node *target_node, html_attr *new_attribute)` - Adds an attribute to a node.
- `html_remove_attr(html_node *target_node, const char *key)` - Removes an attribute by key.
- `html_write_to_file(const char *file_path, html_node *node_list)` - Serializes a node list to a file.
- `html_write_to_string(html_node *node_list)` - Serializes a node list into an allocated string.
- `html_free_nodes(html_node *node_list)` - Frees a node list and its descendants without recursion.
- `html_remove_nodes_by_tag(html_node **node_list, const char *tag_name)` - Removes matching nodes by tag.
- `html_remove_nodes_by_attr(html_node **node_list, const char *key, const char *value)` - Removes nodes with a matching attribute.
- `html_remove_nodes_by_text(html_node **node_list, const char *text_content)` - Removes nodes with matching text.
- `html_find_by_tag(html_node *node_list, const char *tag_name)` - Finds the first node with a tag.
- `html_find_by_attr(html_node *node_list, const char *key, const char *value)` - Finds the first node with a matching attribute.
- `html_find_by_text(html_node *node_list, const char *text_content)` - Finds the first node with matching text.
- `html_find_by_selector(html_node *node_list, const char *selector)` - Finds the first node matching a tag, ID, class, or attribute selector without allocating. Attribute values can use matching single or double quotes.
- `html_query_selector(html_node *node_list, const char *selector)` - Finds the first node matching a tag, ID, or class selector.
- `html_count_nodes_by_tag(html_node *node_list, const char *tag_name)` - Counts matching nodes.

## `html_document`

- `html_document()` / `~html_document()` - Construct and destroy a document wrapper.
- `initialize()` / `destroy()` / `move(html_document &other)` - Manage lifecycle and explicit state transfer.
- `initialize(const html_document &other)` / `initialize(html_document &&other)` - Copy or move initialize from another document.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` - Manage optional synchronization.
- `create_node(...)` / `create_attr(...)` - Allocate document-compatible nodes and attributes.
- `add_attr(...)` / `remove_attr(...)` - Mutate node attributes.
- `add_child(...)` / `append_node(...)` - Mutate tree structure.
- `write_to_file(...)` / `write_to_string()` - Serialize the document.
- `remove_nodes_by_tag(...)` / `remove_nodes_by_attr(...)` / `remove_nodes_by_text(...)` - Remove matching nodes.
- `find_by_tag(...)` / `find_by_attr(...)` / `find_by_text(...)` / `find_by_selector(...)` - Find the first matching node.
- `count_nodes_by_tag(...)` - Count matching nodes.
- `get_root()` - Returns a borrowed, mutable pointer to the root list. Coordinate direct access with any thread that mutates the document.
- `get_error()` / `get_error_str()` - Return the document's last error.
- `clear()` - Frees all document nodes and resets the root.

Node links are publicly mutable. When constructing a wide sibling list, keep a pointer to the last node while linking the list, then pass the completed list to `append_node` or `add_child` once.
