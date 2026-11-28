# XML

The `XML` module provides an XML document tree, file/string/backend IO, callback-based serialization helpers, and conversion to and from the parser DOM model.

## Types

- `xml_node` - XML tree node. Public fields include synchronization state, full name, namespace prefix/local name/URI, namespace bindings, text, child vector, and attribute map.
- `xml_document` - Lifecycle-managed XML document that owns a root node and reports thread-local errors.
- `xml_serialize_callback` - Callback that fills an `xml_document` during serialization.
- `xml_deserialize_callback` - Callback that consumes an `xml_document` during deserialization.

## `xml_node`

- `xml_node()` / `~xml_node()` - Construct and destroy a node and its owned containers.
- `xml_node_prepare_thread_safety(xml_node *node)` - Enables per-node synchronization.
- `xml_node_teardown_thread_safety(xml_node *node)` - Tears down per-node synchronization.
- `xml_node_lock(const xml_node *node, ft_bool *lock_acquired)` - Locks a node when synchronization is enabled.
- `xml_node_unlock(const xml_node *node, ft_bool lock_acquired)` - Unlocks a node previously locked by `xml_node_lock`.

## `xml_document`

- `xml_document()` / `~xml_document()` - Construct and destroy a document wrapper.
- `initialize()` / `destroy()` / `move(xml_document &other)` - Manage lifecycle and explicit state transfer.
- `initialize(const xml_document &other)` / `initialize(xml_document &&other)` - Copy or move initialize from another document.
- Copy initialization preserves attributes, child order, namespace bindings, and the source's document synchronization setting. The copy owns its parsed strings and namespace bindings. A custom URI assigned directly to a node remains borrowed.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` - Manage document-level optional synchronization.
- `load_from_string(const char *xml)` - Parses XML from a C string into the document.
- `load_from_file(const char *file_path)` - Loads XML from a file.
- `load_from_backend(ft_document_source &source)` - Loads XML from an abstract document source.
- `write_to_string()` - Serializes the document into an allocated C string.
- `write_to_string(ft_string &output)` - Serializes into a Libft string. Serialization failure preserves the existing contents.
- `write_to_file(const char *file_path)` - Writes XML to a file.
- `write_to_backend(ft_document_sink &sink)` - Writes XML to an abstract document sink.
- `get_root()` - Returns a borrowed root node pointer. Coordinate its use with any thread that replaces or destroys the tree.
- `set_manual_error(int32_t error_code)` - Manually records an error code on the document.
- `get_error()` / `get_error_str()` - Return the last document error.

## Serialization and DOM Bridge

- `xml_serialize_to_string(...)` - Creates a document with a callback and serializes it into an `ft_string`.
- `xml_serialize_to_backend(...)` - Creates a document with a callback and writes it to a document sink.
- `xml_deserialize_from_string(...)` - Parses XML text and passes the document to a callback.
- `xml_deserialize_from_backend(...)` - Reads XML from a source and passes the document to a callback.
- `xml_document_to_dom(const xml_document &document, ft_dom_document &dom)` - Converts XML to the generic DOM model.
- `xml_document_from_dom(const ft_dom_document &dom, xml_document &document)` - Builds XML from the generic DOM model.

DOM conversion builds the new tree directly and replaces the destination only after successful conversion. Import preserves attribute values containing quotes and nodes that have both text and children.
