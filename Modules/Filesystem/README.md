# Filesystem

The `Filesystem` module provides path manipulation, safety checks, glob matching, atomic writes, temporary-path helpers, and recursive walking helpers. Functions returning `ft_string *` allocate a lifecycle string that the caller owns.

## Types

- `filesystem_walk_callback` - Callback used by recursive walking. It receives the path, whether the path is a directory, and caller context.

## Path Construction and Normalization

- `filesystem_normalize_path(const char *path)` - Returns a normalized copy of a path with redundant separators and path components cleaned up.
- `filesystem_join_path(const char *path_left, const char *path_right)` - Joins two path fragments into one allocated path string.
- `filesystem_safe_join_path(const char *root_path, const char *relative_path)` - Joins a root with a relative path while rejecting lexical traversal outside the root. This is not a symlink/junction sandbox; use canonical containment validation before security-sensitive access.
- `filesystem_canonical_path(const char *path)` - Resolves a path to its canonical form where the platform can provide one.

## Path Components

- `filesystem_basename(const char *path)` - Returns the final filename component using portable `/` separators in the returned string.
- `filesystem_dirname(const char *path)` - Returns the parent directory component using portable `/` separators in the returned string.
- `filesystem_extension(const char *path)` - Returns the extension portion of the final path component.
- `filesystem_stem(const char *path)` - Returns the final path component without its extension.
- `filesystem_split_path(const char *path, ft_string *directory_out, ft_string *basename_out)` - Fills two caller-owned strings with the directory and basename components.

## Path Checks

- `filesystem_is_absolute(const char *path)` - Reports whether a path is absolute.
- `filesystem_is_rooted(const char *path)` - Alias for the absolute-path check.
- `filesystem_is_relative(const char *path)` - Reports whether a path is relative.
- `filesystem_is_safe_relative_path(const char *path)` - Reports whether a relative path avoids absolute roots and parent traversal.
- `filesystem_is_inside_root(const char *root_path, const char *candidate_path)` - Reports whether a candidate path stays inside a root directory.
- `filesystem_validate_inside_root(const char *root_path, const char *candidate_path)` - Returns an error code if a candidate path escapes the root.
- `filesystem_is_hidden(const char *path)` - Reports whether the final path component is a dotfile-style hidden name.
- `filesystem_is_reserved_name(const char *path)` - Reports whether the final path component matches a Windows device-reserved name.
- `filesystem_has_extension(const char *path, const char *extension)` - Checks whether a path ends with the requested extension.
- `filesystem_path_has_wildcards(const char *path)` - Reports whether a path contains `*` or `?`.
- `filesystem_match_glob(const char *pattern, const char *path)` - Matches a path against wildcard syntax with `*`, `?`, and `**`.

## File Operations

- `filesystem_temp_path(const char *prefix, const char *extension, ft_string *output)` - Creates a temporary path string in `output`, using the compatibility temp-directory helper and portable separators in the returned path.
- `filesystem_atomic_write(const char *path, const void *data, ft_size_t size)` - Writes data through a unique temporary file in the target directory and replaces the target atomically where supported. It guarantees atomic visibility, not crash durability.
- `filesystem_walk_recursive(const char *root_path, filesystem_walk_callback callback, void *user_context)` - Visits files and directories under a root and calls the callback for each entry.
