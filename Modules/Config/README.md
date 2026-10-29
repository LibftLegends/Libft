# Config

The `Config` module loads, stores, merges, writes, and synchronizes key/value configuration entries. Configuration data is represented as simple public structs so callers can inspect and manipulate entries directly.

It also exposes lightweight reload and file-watch helpers for call chains that want save/reload/watch naming without changing the underlying storage model.

## Types

- `config_entry` - One configuration value. Public fields are `mutex`, `section`, `key`, and `value`.
- `config_data` - A configuration table. Public fields are the entry array, entry count, and table mutex.

## Config Data API

- `config_data_create()` - Allocates an empty configuration table.
- `config_data_prepare_thread_safety(config_data *config)` - Initializes the table mutex.
- `config_data_teardown_thread_safety(config_data *config)` - Destroys the table mutex. The caller must hold exclusive ownership: no thread may be using or locking the table or its entries during teardown.
- `config_parse(const char *filename)` - Parses a configuration file into a new `config_data`.
- `config_data_free(config_data *config)` - Frees a configuration table and its entries.
- `config_load_env()` - Loads configuration values from the process environment.
- `config_load_file(const char *filename)` - Loads configuration values from a file.
- `config_reload_file(const char *filename)` - Alias for `config_load_file(...)` for reload-style call chains.
- `config_write_file(const config_data *config, const char *filename)` - Writes a configuration table to disk.
- `config_save_file(const config_data *config, const char *filename)` - Alias for `config_write_file(...)` for save-style call chains.
- `config_merge(const config_data *base_config, const config_data *override_config)` - Allocates a merged configuration where override entries replace base entries.
- `config_watch_file(const char *filename, file_watch_callback callback, void *user_data)` - Creates a file watcher for the directory that contains `filename`.

## Entry Synchronization

- `config_entry_prepare_thread_safety(config_entry *entry)` - Initializes an entry mutex.
- `config_entry_teardown_thread_safety(config_entry *entry)` - Destroys an entry mutex. The caller must hold exclusive ownership of the entry and ensure no concurrent lock or access remains.
- `config_entry_lock(config_entry *entry, ft_bool *lock_acquired)` - Locks an entry when thread safety is available.
- `config_entry_unlock(config_entry *entry, ft_bool lock_acquired)` - Unlocks an entry previously locked by `config_entry_lock`.
