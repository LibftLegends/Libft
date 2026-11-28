#include "filesystem.hpp"
#include "../File/file_internal.hpp"

static int32_t filesystem_walk_entries(file_dir *stream, const char *path,
    const ft_string &prefix, filesystem_walk_callback callback, void *user_context)
{
    ft_string child_path;
    int32_t error_code = child_path.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const ft_bool prefix_matches = file_directory_prefix_matches(path, prefix);
    const file_dirent *entry;
    while ((error_code = file_directory_read_entry(stream, &entry)) == FT_ERR_SUCCESS)
    {
        if (file_directory_is_dot_entry(entry->d_name) == FT_TRUE)
            continue ;
        ft_bool direct_child;
        error_code = file_directory_child_path(path, prefix, entry->d_name, child_path, direct_child);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        file_type type = FILE_TYPE_UNKNOWN;
        if (prefix_matches == FT_TRUE && direct_child == FT_TRUE)
            type = file_directory_entry_type(*entry);
        const ft_bool is_directory = type == FILE_TYPE_DIRECTORY || (type != FILE_TYPE_REGULAR
            && file_dir_exists(child_path.c_str()) == FT_ERR_SUCCESS);
        error_code = callback(child_path.c_str(), is_directory, user_context);
        if (error_code == FT_ERR_SUCCESS && is_directory == FT_TRUE)
            error_code = filesystem_walk_recursive(child_path.c_str(), callback, user_context);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    if (error_code == FT_ERR_END_OF_FILE)
        return (FT_ERR_SUCCESS);
    return (error_code);
}

int32_t filesystem_walk_recursive(const char *root_path,
    filesystem_walk_callback callback, void *user_context)
{
    if (root_path == ft_nullptr || callback == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    file_dir *stream = file_opendir(root_path);
    if (stream == ft_nullptr)
        return (FT_ERR_FILE_OPEN_FAILED);
    ft_string *prefix = file_path_join(root_path, "");
    int32_t error_code = FT_ERR_INVALID_STATE;
    if (prefix != ft_nullptr)
    {
        error_code = filesystem_walk_entries(stream, root_path, *prefix, callback, user_context);
        delete prefix;
    }
    const int32_t close_error = file_closedir(stream);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (close_error);
}
