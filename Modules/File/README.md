# File

The `File` module provides filesystem operations, directory iteration, path helpers, safe replacement helpers, recursive copy/move helpers with filters, hash helpers, metadata comparison helpers, and file watching. The watcher uses the separate `Threading` module for its background worker.

## File Types and Directory Entries

- `file_type` - Classifies paths as regular files, directories, symlinks, missing paths, or unknown.
- `file_list_flags` - Flags controlling directory listing: include files, directories, hidden entries, follow symlinks, or include all common entries.
- `file_dirent` - Portable directory entry with inode, fixed name buffer, and type byte.
- `file_dir` - Opaque directory stream handle.

## File Operations

- `file_copy(...)` / `file_copy_with_buffer(...)` - Copy one file to another, optionally with a caller-selected buffer size.
- `file_default_copy_buffer_size()` - Returns the module's default copy buffer size.
- `file_move(...)` - Moves or renames a file.
- `file_exists(...)` - Reports whether a path exists.
- `file_delete(...)` - Deletes a file.
- `file_get_type(...)` - Returns path type classification.
- `file_get_size(...)` - Writes a file size to an output pointer.
- `file_get_permissions(...)` - Writes mode bits for a path.
- `file_create_directory(...)` / `file_create_directories(...)` - Creates one directory or a directory tree.
- `file_delete_recursive(...)` - Recursively deletes a directory tree.
- `file_copy_directory(...)` - Recursively copies a directory tree.
- `file_copy_directory_filtered(...)` - Recursively copies a directory tree while calling a filter for each entry.
- `file_move_directory_filtered(...)` - Recursively moves selected entries from a directory tree while preserving skipped entries in place.
- `file_read_all(...)` - Reads a file into an `ft_string`.
- `file_hash_sha1(...)` / `file_hash_sha256(...)` - Hash a file's contents into a raw digest buffer.
- `file_metadata_diff(...)` - Compares file type, size, permissions, and missing-state metadata into a bitmask.
- `file_write_all(...)` - Writes a full buffer to a file.
- `file_write_all_atomic(...)` - Writes data through a temporary file and replaces the target.
- `file_secure_temp_file(...)` - Creates a temporary file and returns both path and descriptor.
- `file_close_descriptor(...)` - Closes a raw file descriptor.
- `file_replace_safe(...)` - Safely replaces file content with caller data.

## Root Safety and Paths

- `file_path_is_inside_root(...)` - Reports whether a path stays inside a root.
- `file_validate_path_inside_root(...)` - Returns an error if a candidate path escapes the root.
- `file_validate_regular_file_inside_root(...)` - Validates that a path resolves to a regular file inside the canonical root, rejecting missing paths, directories, symlinks, and root escapes.
- `file_path_join(...)` - Allocates a joined path.
- `file_path_normalize(...)` - Allocates a normalized path.
- `file_path_is_absolute(...)` / `file_path_is_relative(...)` - Check path form.
- `file_path_equal(...)` - Compares paths after module normalization rules.
- `file_path_basename(...)` / `file_path_dirname(...)` / `file_path_extension(...)` / `file_path_stem(...)` - Return allocated C string path components.
- `file_path_basename_string(...)` / `file_path_dirname_string(...)` / `file_path_extension_string(...)` / `file_path_stem_string(...)` - Return allocated `ft_string` path components.

## Directory Iteration

- `file_opendir(const char *directory_path)` - Opens a directory stream.
- `file_closedir(file_dir *directory_stream)` - Closes a directory stream.
- `file_readdir(file_dir *directory_stream)` - Reads the next directory entry.
- `file_dir_exists(const char *rel_path)` - Reports whether a directory exists.
- `file_readdir_string(file_dir *directory_stream, ft_string *entry_name)` - Reads the next entry name into an `ft_string`.
- `file_list_directory(...)` - Lists one directory into a vector of strings.
- `file_list_directory_recursive(...)` - Recursively lists a directory tree into a vector of strings.

## C File Compatibility

- `ft_fopen(const char *filename, const char *mode)` - Opens a C `FILE *`.
- `ft_fclose(FILE *stream)` - Closes a C `FILE *`.
- `ft_fgets(char *string, int32_t size, FILE *stream)` - Reads one line from a C `FILE *`.

## File Watch

- `file_watch_event_type` - Event enum for create, modify, and delete.
- `cmp_file_watch_event` - Low-level event payload with event type, optional name flag, and name buffer.
- `file_watch_callback` - Typed watch callback.
- `file_watch_legacy_callback` - Legacy integer event callback.
- `ft_file_watch` - Lifecycle class that watches one directory on a background thread.
- `ft_file_watch` lifecycle methods - `initialize`, copy/move initialization, `destroy`, and `move`.
- `ft_file_watch` thread-safety methods - `enable_thread_safety`, `disable_thread_safety`, and `is_thread_safe`.
- `watch_directory(...)` - Starts watching a directory with either legacy or typed callback.
- `set_debounce_milliseconds(...)` / `get_debounce_milliseconds()` - Configure or read event debounce timing.
- `stop()` - Stops the watcher thread.
## Additional API

- `file_path_relative(const char *from_path, const char *to_path)` - Returns a normalized relative path from one path to another.
