# Storage

The `Storage` module provides `kv_store`, a lifecycle key/value database with JSON, JSON-lines, SQLite when available, and memory-mapped backends. It supports TTL expiration, optional encryption, snapshots, background compaction, metrics, and replication callbacks. Its background workers use the separate `Threading` module.

## Types

- `kv_store_backend_type` - Backend enum for JSON, JSON lines, SQLite when compiled in, and memory-mapped storage.
- `g_kv_store_ttl_prefix` - Prefix used internally to encode TTL metadata.
- `kv_store_metrics` - Counters for set/delete/get/prune operations and prune duration.
- `kv_store_snapshot_entry` - Snapshot record containing key, value, expiration flag, and expiration timestamp. It exposes `initialize`, move initialization, and `destroy`.
- `kv_store_entry` - Lifecycle value entry containing stored value plus optional expiration.
- `kv_store_operation_type` - Replication/apply operation kind: set or delete.
- `kv_store_operation` - Batched operation with type, key, value, value flag, TTL flag, and TTL seconds. It exposes `initialize`, move initialization, and `destroy`.
- `kv_store_replication_sink` - Operations callback, snapshot callback, and user data.

## `kv_store_entry`

- Lifecycle and thread-safety methods - `initialize`, copy/move initialization, `destroy`, `move`, `enable_thread_safety`, `disable_thread_safety`, and `is_thread_safe`.
- `set_value(const ft_string &value)` / `set_value(const char *value_string)` - Store a new value.
- `copy_value(ft_string &destination)` - Copies the stored value into a destination string.
- `get_value_pointer(const char **value_pointer)` - Returns a pointer to the stored C string.
- `configure_expiration(ft_bool has_expiration, int64_t expiration_timestamp)` - Sets expiration metadata.
- `has_expiration(...)` / `get_expiration(...)` - Read expiration metadata.
- `is_expired(int64_t current_time, ft_bool &expired)` - Reports whether the entry is expired at a timestamp.

## `kv_store`

- Lifecycle and thread-safety methods - constructor, destructor, `initialize`, copy/move initialization, `destroy`, `move`, `enable_thread_safety`, `disable_thread_safety`, and `is_thread_safe`.
- Copy and move preserve the configured backend and live entries, including unflushed edits. A move transfers the entries' storage and stops the source's background worker before destroying the source.
- `initialize(const char *file_path, const char *encryption_key, ft_bool enable_encryption, const char *encryption_algorithm_name)` - Opens or creates the store at a location and records the selected block-cipher name alongside the key.
- `kv_set(...)` - Stores a key/value pair with optional TTL seconds.
- `kv_get(...)` - Returns a stored value pointer or null on miss.
- `kv_delete(...)` - Deletes a key.
- `kv_flush()` - Writes current data to the configured backend. JSON snapshots use compact JSON with escaped string fields and retain support for existing snapshots.
- JSON, JSON Lines, and memory-mapped flushes write a sibling temporary file and atomically replace the previous snapshot once the write succeeds.
- JSON Lines writes one compact JSON record per line. The reader also accepts the multiline records written by earlier versions.
- SQLite loads encryption metadata and entries from the same read transaction. A failed write rolls back the full snapshot. Snapshot imports and exports preserve the full key and value lengths, including embedded NUL bytes.
- `prune_expired()` - Removes expired entries.
- Expiration checks skip the entry scan until the earliest pending expiration is due. Pruning compacts the surviving entries in insertion order without allocating.
- `configure_encryption(...)` - Updates encryption key, enable flag, and selected block-cipher name.
- Encrypted snapshots and JSON metadata record the store's configured encryption algorithm name, so each `kv_store` instance can keep its own encryption choice instead of sharing a global selection.
- `set_backend(...)` / `get_backend()` - Configure or inspect the active backend. Switching to a missing file keeps the current entries, and a failed load preserves the active backend.
- `kv_apply(const ft_vector<kv_store_operation> &operations)` - Applies a batch of set/delete operations.
- Batches that only update existing keys stage the replacement values before committing. A single deletion compacts the entries in place. Other changes to the set of keys apply to a separate map and publish it after the batch succeeds.
- `kv_compare_and_swap(...)` - Replaces a value only when the current value matches an expected value.
- `get_metrics(kv_store_metrics &out_metrics)` - Writes operation counters.
- `export_snapshot(...)` / `import_snapshot(...)` - Copy store contents to or from snapshot entries. Import replaces the current contents after the full snapshot is validated and built.
- `export_snapshot_to_file(...)` - Writes a snapshot to a file.
- `write_snapshot(ft_document_sink &sink)` / `read_snapshot(ft_document_source &source)` - Use abstract document IO for snapshots.
- `start_background_compaction(...)` / `stop_background_compaction()` - Manage periodic pruning. Call `enable_thread_safety()` before starting the worker. Stopping wakes a waiting worker immediately. Disabling thread safety stops the worker before releasing the store mutex.
- `register_replication_sink(...)` / `unregister_replication_sink(...)` - Add or remove replication listeners.
- `ship_replication_snapshot(...)` - Sends the current snapshot to a callback.

## Operation Helpers

- `kv_store_init_set_operation(...)` - Initializes a set operation.
- `kv_store_init_delete_operation(...)` - Initializes a delete operation.
## Additional API

- `kv_store::kv_contains(const char *key_string)` - Tests key existence without treating a stored empty string as missing.
