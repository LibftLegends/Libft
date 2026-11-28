#include "file_internal.hpp"
#include "../Compatebility/compatebility_internal.hpp"

ft_bool file_directory_is_dot_entry(const char *name) noexcept
{
    return (name[0] == '.' && (name[1] == '\0'
        || (name[1] == '.' && name[2] == '\0')));
}

int32_t file_directory_read_entry(file_dir *stream, const file_dirent **entry_out)
{
    int32_t error_code = FT_ERR_SUCCESS;
    *entry_out = cmp_dir_read(stream, &error_code);
    if (error_code == FT_ERR_SUCCESS && *entry_out == ft_nullptr)
        return (FT_ERR_END_OF_FILE);
    return (error_code);
}

int32_t file_readdir_string(file_dir *directory_stream, ft_string *entry_name)
{
    if (directory_stream == ft_nullptr || entry_name == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    const file_dirent *entry;
    const int32_t error_code = file_directory_read_entry(directory_stream, &entry);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (entry_name->assign(entry->d_name, ft_strlen_size_t(entry->d_name)));
}

ft_bool file_directory_prefix_matches(const char *path, const ft_string &prefix) noexcept
{
    const ft_size_t length = ft_strlen_size_t(path);
    if (prefix.size() < length || prefix.size() - length > 1)
        return (FT_FALSE);
    const char *normalized_path = prefix.c_str();
    if (ft_memcmp(normalized_path, path, length) == 0)
        return (FT_TRUE);
    if (cmp_path_separator() != '\\')
        return (FT_FALSE);
    ft_size_t index = 0;
    while (index < length)
    {
        if (path[index] != normalized_path[index]
            && (path[index] != '/' || normalized_path[index] != '\\'))
            return (FT_FALSE);
        ++index;
    }
    return (FT_TRUE);
}

int32_t file_directory_child_path(const char *parent, const ft_string &prefix,
    const char *name, ft_string &child_path, ft_bool &is_direct_child)
{
    ft_size_t name_length = 0;
    while (name[name_length] != '\0')
    {
        if (name[name_length] == '\\' || name[name_length] == '/' || name[name_length] == ':')
        {
            is_direct_child = FT_FALSE;
            ft_string *joined = file_path_join(parent, name);
            if (joined == ft_nullptr)
                return (FT_ERR_NO_MEMORY);
            const int32_t error_code = child_path.move(*joined);
            delete joined;
            return (error_code);
        }
        ++name_length;
    }
    is_direct_child = FT_TRUE;
    const int32_t error_code = child_path.assign(prefix.c_str(), prefix.size());
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (child_path.append(name, name_length));
}

file_type file_directory_entry_type(const file_dirent &entry) noexcept
{
    return (cmp_dir_entry_type(entry));
}

static ft_bool file_list_should_include(file_type type_value, uint32_t flags) noexcept
{
    if (type_value == FILE_TYPE_DIRECTORY)
        return ((flags & FILE_LIST_INCLUDE_DIRECTORIES) != 0);
    if (type_value == FILE_TYPE_REGULAR)
        return ((flags & FILE_LIST_INCLUDE_FILES) != 0);
    if (type_value == FILE_TYPE_SYMLINK && (flags & FILE_LIST_FOLLOW_SYMLINKS) == 0)
        return ((flags & FILE_LIST_INCLUDE_FILES) != 0);
    return (FT_FALSE);
}

static int32_t file_list_directory_internal(const char *path,
    ft_vector<ft_string> &entries, uint32_t flags, ft_bool recursive);

static int32_t file_list_entries(file_dir *stream, const char *path,
    const ft_string &prefix, ft_vector<ft_string> &entries, uint32_t flags,
    ft_bool recursive)
{
    ft_string child_path;
    int32_t error_code = child_path.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const ft_bool prefix_matches = file_directory_prefix_matches(path, prefix);
    const file_dirent *entry;
    while ((error_code = file_directory_read_entry(stream, &entry)) == FT_ERR_SUCCESS)
    {
        const char *name = entry->d_name;
        if (file_directory_is_dot_entry(name) == FT_TRUE
            || (name[0] == '.' && (flags & FILE_LIST_INCLUDE_HIDDEN) == 0))
            continue ;
        ft_bool direct_child;
        error_code = file_directory_child_path(path, prefix, name, child_path, direct_child);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        file_type child_type = FILE_TYPE_UNKNOWN;
        if (direct_child == FT_TRUE && prefix_matches == FT_TRUE)
            child_type = file_directory_entry_type(*entry);
        if (child_type == FILE_TYPE_UNKNOWN)
            child_type = file_get_type(child_path.c_str());
        if (file_list_should_include(child_type, flags) == FT_TRUE)
            error_code = entries.push_back(child_path);
        if (error_code == FT_ERR_SUCCESS && recursive == FT_TRUE && child_type == FILE_TYPE_DIRECTORY)
            error_code = file_list_directory_internal(child_path.c_str(), entries, flags, recursive);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    if (error_code == FT_ERR_END_OF_FILE)
        return (FT_ERR_SUCCESS);
    return (error_code);
}

static int32_t file_list_directory_internal(const char *path,
    ft_vector<ft_string> &entries, uint32_t flags, ft_bool recursive)
{
    if (file_get_type(path) != FILE_TYPE_DIRECTORY)
        return (FT_ERR_NOT_DIRECTORY);
    file_dir *stream = file_opendir(path);
    if (stream == ft_nullptr)
        return (FT_ERR_FILE_OPEN_FAILED);
    ft_string *prefix = file_path_join(path, "");
    int32_t error_code = FT_ERR_NO_MEMORY;
    if (prefix != ft_nullptr)
    {
        error_code = file_list_entries(stream, path, *prefix, entries, flags, recursive);
        delete prefix;
    }
    const int32_t close_error = file_closedir(stream);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (close_error);
}

int32_t file_list_directory(const char *path, ft_vector<ft_string> &entries,
    uint32_t flags)
{
    if (path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (file_list_directory_internal(path, entries, flags, FT_FALSE));
}

int32_t file_list_directory_recursive(const char *path, ft_vector<ft_string> &entries,
    uint32_t flags)
{
    if (path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (file_list_directory_internal(path, entries, flags, FT_TRUE));
}
