#ifndef FILE_INTERNAL_HPP
# define FILE_INTERNAL_HPP

# include "file_utils.hpp"
# include "../Observability/observability.hpp"

int32_t file_replace_contents(const char *path, const char *data, ft_size_t size,
    ft_bool durable);
typedef int32_t (*file_replace_write_callback)(int32_t descriptor, void *context);
// The callback writes to a fresh sibling file. Ownership of the descriptor stays here.
int32_t file_replace_stream(const char *path, ft_bool durable,
    file_replace_write_callback write_contents, void *context);
ft_bool file_directory_is_dot_entry(const char *name) noexcept;
int32_t file_directory_read_entry(file_dir *stream, const file_dirent **entry_out);
ft_bool file_directory_prefix_matches(const char *path, const ft_string &prefix) noexcept;
int32_t file_directory_child_path(const char *parent, const ft_string &prefix,
    const char *name, ft_string &child_path, ft_bool &is_direct_child);
file_type file_directory_entry_type(const file_dirent &entry) noexcept;
ft_string *file_path_split_directory(const char *path, ft_string &basename);
int32_t file_path_append_normalized_relative(ft_string &left, ft_string &right);
int32_t file_copy_using_buffer(const char *source_path, const char *destination_path,
    char *buffer, ft_size_t buffer_size);
void file_observability_emit(const char *operation, const char *path,
    ft_observability_trace_phase phase, int32_t error_code,
    ft_size_t bytes_read, ft_size_t bytes_written);

#endif
