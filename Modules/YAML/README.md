# YAML

The `YAML` module provides a small YAML value tree with scalar, list, and map nodes, plus file/string/backend IO and DOM conversion helpers.

## Types

- `yaml_type` - Node type enum: scalar, list, or map.
- `yaml_value` - Lifecycle-managed YAML node. It stores one scalar string, list children, map children, map key order, and optional synchronization.
- `yaml_serialize_callback` - Callback that fills a root `yaml_value` before serialization.
- `yaml_deserialize_callback` - Callback that consumes a parsed root `yaml_value`.

## `yaml_value`

- `yaml_value()` / `~yaml_value()` - Construct and destroy a YAML node.
- `initialize()` / `destroy()` / `move(yaml_value &other)` - Manage lifecycle and explicit state transfer.
- `initialize(const yaml_value &other)` / `initialize(yaml_value &&other)` - Copy or move initialize from another value.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` - Manage optional recursive locking.
- `set_type(yaml_type type)` / `get_type()` - Set or read the node kind.
- `set_scalar(const ft_string &value)` / `get_scalar()` - Set or read scalar text.
- `add_list_item(yaml_value *item)` / `get_list()` - Append and inspect list children.
- `add_map_item(const ft_string &key, yaml_value *value)` / `get_map()` / `get_map_keys()` - Add map entries and inspect map storage plus insertion key order.

## Parsing Helpers

- `yaml_find_char(const ft_string &string, char character)` - Finds a character in a string.
- `yaml_substr(const ft_string &string, ft_size_t start, ft_size_t length)` - Allocates a substring.
- `yaml_substr_from(const ft_string &string, ft_size_t start)` - Allocates a substring from an offset to the end.
- `yaml_count_indent(const ft_string &line)` - Counts leading indentation.
- `yaml_trim(ft_string &string)` - Trims whitespace in place.
- `yaml_split_lines(const ft_string &content, ft_vector<ft_string> &lines)` - Splits YAML content into lines.

## IO, Serialization, and DOM Bridge

- `yaml_read_from_string(const ft_string &content)` - Parses YAML text into an allocated root value.
- `yaml_read_from_file(const char *file_path)` - Parses YAML from a file.
- `yaml_read_from_backend(ft_document_source &source)` - Parses YAML from an abstract source.
- `yaml_write_to_string(const yaml_value *value)` - Serializes a YAML tree into an `ft_string`.
- `yaml_write_to_string(const yaml_value *value, ft_string &output)` - Replaces the output with the serialized YAML after serialization succeeds.
- `yaml_write_to_file(const char *file_path, const yaml_value *value)` - Writes YAML to a file.
- `yaml_write_to_backend(ft_document_sink &sink, const yaml_value *value)` - Writes YAML to an abstract sink.
- `yaml_free(yaml_value *value)` - Recursively frees a YAML tree.
- `yaml_serialize_to_string(...)` - Builds a root with a callback and serializes it into an `ft_string`.
- `yaml_serialize_to_backend(...)` - Builds a root with a callback and writes it to a sink.
- `yaml_deserialize_from_string(...)` - Parses YAML text and passes the root to a callback.
- `yaml_deserialize_from_backend(...)` - Parses from a source and passes the root to a callback.
- `yaml_value_from_dom(const ft_dom_document &dom)` - Converts a DOM document into a YAML value tree.
- `yaml_value_to_dom(const yaml_value *value, ft_dom_document &dom)` - Converts a YAML value tree into the DOM model.
