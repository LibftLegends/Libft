#ifndef FILE_FILE_UTILS_HPP
# define FILE_FILE_UTILS_HPP

#include "../CPP_class/class_string.hpp"
#include "../Template/vector.hpp"
#include "open_dir.hpp"
#include <cstddef>
#include <cstdio>
#include <sys/types.h>

enum file_type
{
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_SYMLINK,
    FILE_TYPE_MISSING,
#ifdef FILE_TYPE_UNKNOWN
# undef FILE_TYPE_UNKNOWN
#endif
    FILE_TYPE_UNKNOWN
};

int32_t       file_copy(const char *source_path, const char *destination_path);
int32_t       file_copy_with_buffer(const char *source_path, const char *destination_path, ft_size_t buffer_size);
ft_size_t    file_default_copy_buffer_size(void) noexcept;
int32_t       file_move(const char *source_path, const char *destination_path);
int32_t       file_exists(const char *path);
int32_t       file_delete(const char *path);
file_type     file_get_type(const char *path) noexcept;
int32_t       file_get_size(const char *path, ft_size_t *size);
int32_t       file_get_permissions(const char *path, mode_t *mode);
int32_t       file_create_directories(const char *path, mode_t mode);
int32_t       file_delete_recursive(const char *path);
int32_t       file_copy_directory(const char *source_path, const char *destination_path);
int32_t       file_read_all(const char *path, ft_string &output);
int32_t       file_write_all(const char *path, const char *data, ft_size_t size);
int32_t       file_write_all_atomic(const char *path, const char *data, ft_size_t size);
int32_t       file_secure_temp_file(const char *directory_path, const char *prefix, ft_string *path_out, int32_t *file_descriptor_out);
int32_t       file_close_descriptor(int32_t file_descriptor);
ft_bool       file_path_is_inside_root(const char *root_path, const char *candidate_path);
int32_t       file_validate_path_inside_root(const char *root_path, const char *candidate_path);
int32_t       file_validate_regular_file_inside_root(const char *root_path,
                const char *candidate_path);
int32_t       file_replace_safe(const char *path, const char *data, ft_size_t size);
int32_t       file_readdir_string(file_dir *directory_stream, ft_string *entry_name);
enum file_list_flags : uint32_t
{
    FILE_LIST_INCLUDE_FILES = 1U,
    FILE_LIST_INCLUDE_DIRECTORIES = 2U,
    FILE_LIST_INCLUDE_HIDDEN = 4U,
    FILE_LIST_FOLLOW_SYMLINKS = 8U,
    FILE_LIST_INCLUDE_ALL = FILE_LIST_INCLUDE_FILES | FILE_LIST_INCLUDE_DIRECTORIES | FILE_LIST_INCLUDE_HIDDEN
};

typedef ft_bool (*file_path_filter_callback)(const char *path,
    ft_bool is_directory, void *user_context);

enum file_metadata_diff_flags : uint32_t
{
    FILE_METADATA_DIFF_NONE = 0U,
    FILE_METADATA_DIFF_TYPE = 1U,
    FILE_METADATA_DIFF_SIZE = 2U,
    FILE_METADATA_DIFF_PERMISSIONS = 4U,
    FILE_METADATA_DIFF_MISSING = 8U
};

int32_t       file_list_directory(const char *path, ft_vector<ft_string> &entries, uint32_t flags);
int32_t       file_list_directory_recursive(const char *path, ft_vector<ft_string> &entries, uint32_t flags);
int32_t       file_copy_directory_filtered(const char *source_path, const char *destination_path,
                file_path_filter_callback filter_callback, void *user_context);
int32_t       file_move_directory_filtered(const char *source_path, const char *destination_path,
                file_path_filter_callback filter_callback, void *user_context);
int32_t       file_hash_sha1(const char *path, uint8_t digest[20]);
int32_t       file_hash_sha256(const char *path, uint8_t digest[32]);
int32_t       file_metadata_diff(const char *left_path, const char *right_path, uint32_t *difference_out);
ft_string *file_path_join(const char *path_left, const char *path_right);
ft_string *file_path_relative(const char *from_path, const char *to_path);
ft_string *file_path_normalize(const char *path);
ft_bool   file_path_is_absolute(const char *path) noexcept;
ft_bool   file_path_is_relative(const char *path) noexcept;
ft_bool   file_path_equal(const char *path_left, const char *path_right);
char      *file_path_basename(const char *path);
char      *file_path_dirname(const char *path);
char      *file_path_extension(const char *path);
char      *file_path_stem(const char *path);
ft_string *file_path_basename_string(const char *path);
ft_string *file_path_dirname_string(const char *path);
ft_string *file_path_extension_string(const char *path);
ft_string *file_path_stem_string(const char *path);

FILE      *ft_fopen(const char *filename, const char *mode);
int32_t        ft_fclose(FILE *stream);
char       *ft_fgets(char *string, int32_t size, FILE *stream);

#endif
