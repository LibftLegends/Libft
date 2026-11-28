# Parser

The `Parser` module provides reusable document sources/sinks and a generic DOM tree with schema validation. JSON, XML, YAML, and storage snapshot code use these abstractions for shared IO and conversion paths.

## Document Sources and Sinks

- `ft_document_source` - Abstract lifecycle input source with `read_all(ft_string &output)`.
- `ft_document_sink` - Abstract lifecycle output sink with `write_all(const char *data_pointer, ft_size_t data_length)`.
- `ft_file_document_source` / `ft_file_document_sink` - File-backed source and sink with `set_path` and `get_path`.
- `ft_memory_document_source` - Memory-backed source with `set_data`, `get_data`, and `get_length`.
- `ft_memory_document_sink` - String-backed sink with `set_storage` and `get_storage`.
- `ft_http_document_source` / `ft_http_document_sink` - HTTP/HTTPS-backed source and sink when OpenSSL networking support is available.

All source/sink classes expose lifecycle methods, optional thread-safety methods, `lock`/`unlock`, and their backend-specific configuration or IO override.
Lifecycle and synchronization calls through a source or sink reference use the same state and mutex as calls through the concrete backend.

The file source transfers its read buffer into the output string once the read succeeds. A failed read preserves the previous output.

The HTTP source transfers the body of a successful response into the output string. A failed request preserves the previous output. The sink sends the caller's bytes and discards the response body. Both require a 2xx HTTP status. When thread safety is enabled, HTTP requests use a copy of the endpoint configuration so reconfiguration can proceed while a request is in flight.

The loopback probe in `Test/tools/parser_http_backend_probe.cpp` exercises the OpenSSL-enabled client. Run the compiled probe with `Test/tools/verify_parser_http_backend.py`. Pass `--openssl` with an OpenSSL executable to test HTTPS with a temporary certificate trusted only by the probe process.

## DOM Types

- `ft_dom_node_type` - Node kind enum: null, object, array, value, or element.
- `ft_dom_node` - Lifecycle DOM node with type, name, value, children, attributes, and optional locking.
- `ft_dom_document` - Lifecycle document wrapper that owns a root node.
- `ft_dom_schema_rule` - Schema path/type/required rule with copy and move initialization and destroy.
- `ft_dom_validation_error` - Validation error path/message pair with copy initialization and destroy.
- `ft_dom_validation_report` - Lifecycle report containing validity and validation errors.
- `ft_dom_schema` - Lifecycle schema containing validation rules.

## DOM Node API

- Lifecycle and synchronization - `initialize`, `destroy`, `enable_thread_safety`, `disable_thread_safety`, `is_thread_safe`, `lock`, and `unlock`.
- Type/name/value - `get_type`, `set_type`, `set_name`, `get_name`, `set_value`, and `get_value`.
- Tree operations - `add_child`, `get_children`, and `find_child`.
- Attributes - `add_attribute`, `has_attribute`, `get_attribute`, `get_attribute_keys`, and `get_attribute_values`.

## DOM Document, Schema, and Validation

- `ft_dom_document::set_root(...)`, `replace_root(...)`, `get_root()`, and `clear()` - Manage document root ownership. `replace_root` reports failure and transfers ownership on success.
- `ft_dom_validation_report::reset()`, `mark_valid()`, `mark_invalid()`, `valid()`, `add_error(...)`, and `errors()` - Build and inspect validation results.
- `ft_dom_schema::add_rule(...)` - Adds a required/typed path rule.
- `ft_dom_schema::validate(...)` - Resets the supplied report and validates a document into it.
- `ft_dom_find_path(...)` - Finds a DOM node from a root node using slash-separated path segments without allocating temporary strings.
