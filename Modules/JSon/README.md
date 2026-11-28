# JSon

The `JSon` module provides a linked-list JSON tree, a lifecycle document wrapper, streaming reader/writer state, schema validation, schema migration registration, and DOM conversion.

## Tree Types

- `json_item` - Key/value item node with string value, optional `ft_big_number`, next pointer, mutex, and error code.
- `json_group` - Named group object containing item list, next pointer, mutex, and error code.
- `json_type` - Schema field type enum.
- `json_schema_field` - Field rule with key, type, required flag, next pointer, mutex, and error code.
- `json_schema` - Linked list of schema fields plus mutex and error code.

## Document Wrapper

- `json_document` - Lifecycle wrapper around a group list.
- Lifecycle/thread-safety methods - `initialize`, copy/move initialization, `destroy`, `move`, `enable_thread_safety`, `disable_thread_safety`, and `is_thread_safe`.
- Creation and mutation - `create_group`, `create_item` overloads for string, big number, integer, and boolean values, `add_item`, `append_group`, `remove_group`, `remove_item`, `update_item` overloads, and `clear`.
- IO - `write_to_file`, `write_to_backend`, `write_to_string`, `read_from_file`, streaming file read, `read_from_backend`, and `read_from_string`.
- Query - `find_group`, `find_item`, `find_item_by_pointer`, `get_value_by_pointer`, and `get_groups`.
- Error methods - `set_manual_error`, `get_error`, and `get_error_str`.

## Free Tree API

- Creation - `json_create_json_group`, `json_create_item` overloads.
- Mutation - `json_add_item_to_group`, `json_append_group`, `json_remove_group`, `json_remove_item`, and `json_update_item` overloads.
- Query - `json_find_group`, `json_find_item`, JSON pointer lookup helpers, and validation helpers.
- IO - `json_read_from_file`, `json_read_from_string`, `json_read_from_backend`, `json_write_to_file`, `json_write_to_string`, `json_write_to_backend`, and document wrapper IO helpers.
- Cleanup - `json_free_items` and `json_free_groups`.
- Thread/error helpers - `json_item_*`, `json_group_*`, `json_schema_field_*`, `json_schema_*`, and group-list lock helpers manage locking and stored error codes.

## Streaming

- `json_stream_scalar` - Scalar event value with boolean/string/length storage.
- `json_stream_event_type` - Event enum for object/array/scalar/key style events.
- `json_stream_event` - One streaming parse event.
- `json_stream_reader` - Callback or file backed reader state with buffer, cursor, end flag, mutex, and error code.
- `json_stream_writer` and `json_stream_writer_context` - Incremental writer state and nesting context.
- `json_stream_writer_reset` starts a fresh document with the same callback and nesting buffer. Discard any partial output before resetting an abandoned document.
- Reader helpers - Initialize from callback/file, parse to groups, traverse events, read events from stream/file, destroy, lock, thread-safety, and error helpers.
- Writer helpers - Initialize, begin/end document, begin/end object/array, write keys/scalars, destroy, lock, thread-safety, and error helpers.

## Schema, Migration, and DOM Bridge

- `json_validate_schema(json_group *group, const json_schema &schema)` - Validates a JSON group against schema rules.
- `json_register_schema_migration(...)` - Registers a migration callback from one schema version to another.
- `json_get_latest_schema_version(...)` - Returns the latest known version for a schema name.
- `json_apply_schema_migrations(...)` - Applies registered migrations to a document.
- `json_serialize_to_string`, `json_serialize_to_backend`, `json_deserialize_from_string`, and `json_deserialize_from_backend` - Callback-based serialization helpers.
- `json_document_to_dom(...)` and `json_document_from_dom(...)` - Convert between JSON documents and the generic DOM model.
