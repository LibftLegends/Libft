#include "file_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Compatebility/compatebility_internal.hpp"

static int32_t file_recursive_ensure_directory(const char *path, mode_t mode,
    ft_bool native_path_matches)
{
    if (native_path_matches == FT_TRUE)
    {
        const file_type type = file_get_type(path);
        if (type == FILE_TYPE_DIRECTORY)
            return (FT_ERR_SUCCESS);
        if (type == FILE_TYPE_REGULAR || type == FILE_TYPE_SYMLINK)
            return (FT_ERR_ALREADY_EXISTS);
    }
    const int32_t error_code = file_create_directory(path, mode);
    if (error_code == FT_ERR_ALREADY_EXISTS && file_get_type(path) == FILE_TYPE_DIRECTORY)
        return (FT_ERR_SUCCESS);
    return (error_code);
}

static int32_t file_recursive_create_parents(char *path, mode_t mode)
{
    ft_bool native_path_matches = FT_TRUE;
    if (cmp_path_separator() == '\\' && (path[0] == '/' || path[0] == '\\'))
        native_path_matches = FT_FALSE;
    ft_size_t index = 0;
    while (path[index] != '\0')
    {
        if ((path[index] == '/' || path[index] == '\\') && index > 0 && path[index - 1] != ':')
        {
            const char separator = path[index];
            path[index] = '\0';
            const int32_t error_code = file_recursive_ensure_directory(path, mode, native_path_matches);
            path[index] = separator;
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        ++index;
    }
    if (index == 0 || path[index - 1] == '/' || path[index - 1] == '\\')
    {
        if (index == 0)
            return (FT_ERR_INVALID_STATE);
        if (index > 1 && path[index - 2] != ':')
            return (FT_ERR_SUCCESS);
    }
    return (file_recursive_ensure_directory(path, mode, native_path_matches));
}

int32_t file_create_directories(const char *path, mode_t mode)
{
    if (path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string *normalized = file_path_normalize(path);
    if (normalized == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    char *data = normalized->data();
    int32_t error_code = FT_ERR_INVALID_STATE;
    if (data != ft_nullptr)
        error_code = file_recursive_create_parents(data, mode);
    delete normalized;
    return (error_code);
}

static int32_t file_recursive_delete_leaf(const char *path)
{
    uint32_t attempt = 0;
    int32_t error_code = FT_ERR_INVALID_OPERATION;
    while (attempt < 20U)
    {
        error_code = file_delete(path);
        if (error_code != FT_ERR_INVALID_OPERATION && error_code != FT_ERR_PERMISSION_DENIED)
            break ;
        ++attempt;
        if (attempt < 20U)
            (void)cmp_thread_sleep(100);
    }
    return (error_code);
}

static int32_t file_recursive_delete_native_path(const char *path);

static int32_t file_recursive_delete_entries(file_dir *stream, const char *path)
{
    ft_string child;
    int32_t error_code = child.initialize(path);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const char separator = cmp_path_separator();
    const char last = child.back();
    if (last != separator && last != '/')
    {
        error_code = child.append(separator);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    const ft_size_t prefix_length = child.size();
    const file_dirent *entry;
    while ((error_code = file_directory_read_entry(stream, &entry)) == FT_ERR_SUCCESS)
    {
        if (file_directory_is_dot_entry(entry->d_name) == FT_TRUE)
            continue ;
        // Keep the native entry name even when it contains a literal backslash.
        error_code = child.append(entry->d_name);
        if (error_code == FT_ERR_SUCCESS)
            error_code = file_recursive_delete_native_path(child.c_str());
        if (error_code == FT_ERR_SUCCESS)
            error_code = child.resize_length(prefix_length);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    if (error_code == FT_ERR_END_OF_FILE)
        return (FT_ERR_SUCCESS);
    return (error_code);
}

static int32_t file_recursive_delete_native_path(const char *path)
{
    const file_type type = file_get_type(path);
    if (type == FILE_TYPE_MISSING)
        return (FT_ERR_SUCCESS);
    if (type == FILE_TYPE_REGULAR || type == FILE_TYPE_SYMLINK)
        return (file_recursive_delete_leaf(path));
    if (type != FILE_TYPE_DIRECTORY)
        return (FT_ERR_UNSUPPORTED_TYPE);
    file_dir *stream = file_opendir(path);
    if (stream == ft_nullptr)
        return (FT_ERR_FILE_OPEN_FAILED);
    const int32_t error_code = file_recursive_delete_entries(stream, path);
    const int32_t close_error = file_closedir(stream);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (close_error != FT_ERR_SUCCESS)
        return (close_error);
    return (cmp_file_remove_directory(path));
}

int32_t file_delete_recursive(const char *path)
{
    if (path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string *normalized = file_path_normalize(path);
    if (normalized == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (cmp_path_separator() == '\\')
        cmp_normalize_slashes(normalized->data());
    const int32_t error_code = file_recursive_delete_native_path(normalized->c_str());
    delete normalized;
    return (error_code);
}

struct file_recursive_transfer_options
{
    file_path_filter_callback filter;
    void *user_context;
    ft_bool move_sources;
    char *buffer;
    ft_size_t buffer_size;
};

static ft_bool file_recursive_should_include(const file_recursive_transfer_options &options,
    const char *path, ft_bool is_directory) noexcept
{
    if (options.filter == ft_nullptr)
        return (FT_TRUE);
    return (options.filter(path, is_directory, options.user_context));
}

static int32_t file_recursive_transfer_directory(const char *source_path,
    const char *destination_path, file_recursive_transfer_options &options);

static int32_t file_recursive_prepare_destination_directory(const char *path)
{
    const file_type type = file_get_type(path);
    if (type == FILE_TYPE_DIRECTORY)
        return (FT_ERR_SUCCESS);
    if (type == FILE_TYPE_UNKNOWN)
        return (FT_ERR_IO);
    if (type == FILE_TYPE_MISSING)
        return (file_recursive_ensure_directory(path, 0755, FT_FALSE));
    return (FT_ERR_ALREADY_EXISTS);
}

static int32_t file_recursive_transfer_child(const char *source, const char *destination,
    file_type type, file_recursive_transfer_options &options)
{
    if (type == FILE_TYPE_DIRECTORY)
    {
        const int32_t error_code = file_recursive_prepare_destination_directory(destination);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        return (file_recursive_transfer_directory(source, destination, options));
    }
    if (type == FILE_TYPE_SYMLINK && options.move_sources == FT_TRUE)
        return (file_move(source, destination));
    if (type != FILE_TYPE_REGULAR)
        return (FT_ERR_UNSUPPORTED_TYPE);
    if (options.move_sources == FT_TRUE)
        return (file_move(source, destination));
    if (options.buffer == ft_nullptr)
    {
        options.buffer = static_cast<char *>(cma_malloc(options.buffer_size));
        if (options.buffer == ft_nullptr)
            return (FT_ERR_IO);
    }
    return (file_copy_using_buffer(source, destination, options.buffer, options.buffer_size));
}

static int32_t file_recursive_transfer_entries(file_dir *stream, const char *source_path,
    const char *destination_path, file_recursive_transfer_options &options)
{
    ft_string source_child;
    ft_string destination_child;
    int32_t error_code = source_child.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = destination_child.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    ft_string *source_prefix = file_path_join(source_path, "");
    if (source_prefix == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    ft_string *destination_prefix = file_path_join(destination_path, "");
    if (destination_prefix == ft_nullptr)
    {
        delete source_prefix;
        return (FT_ERR_NO_MEMORY);
    }
    const file_dirent *entry;
    while ((error_code = file_directory_read_entry(stream, &entry)) == FT_ERR_SUCCESS)
    {
        if (file_directory_is_dot_entry(entry->d_name) == FT_TRUE)
            continue ;
        ft_bool direct_entry;
        error_code = file_directory_child_path(source_path, *source_prefix,
            entry->d_name, source_child, direct_entry);
        if (error_code != FT_ERR_SUCCESS)
            break ;
        const file_type type = file_get_type(source_child.c_str());
        if (type == FILE_TYPE_UNKNOWN)
        {
            error_code = FT_ERR_IO;
            break ;
        }
        if (file_recursive_should_include(options, source_child.c_str(),
                type == FILE_TYPE_DIRECTORY) == FT_FALSE)
            continue ;
        error_code = file_directory_child_path(destination_path, *destination_prefix,
            entry->d_name, destination_child, direct_entry);
        if (error_code == FT_ERR_SUCCESS)
            error_code = file_recursive_transfer_child(source_child.c_str(),
                destination_child.c_str(), type, options);
        if (error_code != FT_ERR_SUCCESS)
            break ;
    }
    delete destination_prefix;
    delete source_prefix;
    if (error_code == FT_ERR_END_OF_FILE)
        return (FT_ERR_SUCCESS);
    return (error_code);
}

static int32_t file_recursive_transfer_directory(const char *source_path,
    const char *destination_path, file_recursive_transfer_options &options)
{
    file_dir *stream = file_opendir(source_path);
    if (stream == ft_nullptr)
        return (FT_ERR_FILE_OPEN_FAILED);
    const int32_t error_code = file_recursive_transfer_entries(stream, source_path,
        destination_path, options);
    const int32_t close_error = file_closedir(stream);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (close_error != FT_ERR_SUCCESS)
        return (close_error);
    if (options.move_sources == FT_TRUE)
        (void)cmp_file_remove_directory(source_path);
    return (FT_ERR_SUCCESS);
}

static int32_t file_recursive_transfer(const char *source_path, const char *destination_path,
    file_path_filter_callback filter, void *user_context, ft_bool move_sources)
{
    if (source_path == ft_nullptr || destination_path == ft_nullptr
        || file_get_type(source_path) != FILE_TYPE_DIRECTORY)
        return (FT_ERR_INVALID_ARGUMENT);
    file_recursive_transfer_options options{filter, user_context, move_sources,
        ft_nullptr, file_default_copy_buffer_size()};
    if (file_recursive_should_include(options, source_path, FT_TRUE) == FT_FALSE
        || file_path_equal(source_path, destination_path) == FT_TRUE)
        return (FT_ERR_SUCCESS);
    if (file_path_is_inside_root(source_path, destination_path) == FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    const int32_t prepare_error = file_recursive_prepare_destination_directory(destination_path);
    if (prepare_error != FT_ERR_SUCCESS)
        return (prepare_error);
    const int32_t result = file_recursive_transfer_directory(source_path, destination_path, options);
    cma_free(options.buffer);
    return (result);
}

int32_t file_copy_directory(const char *source_path, const char *destination_path)
{
    return (file_copy_directory_filtered(source_path, destination_path, ft_nullptr, ft_nullptr));
}

int32_t file_copy_directory_filtered(const char *source_path, const char *destination_path,
    file_path_filter_callback filter_callback, void *user_context)
{
    return (file_recursive_transfer(source_path, destination_path, filter_callback, user_context, FT_FALSE));
}

int32_t file_move_directory_filtered(const char *source_path, const char *destination_path,
    file_path_filter_callback filter_callback, void *user_context)
{
    return (file_recursive_transfer(source_path, destination_path, filter_callback, user_context, FT_TRUE));
}
