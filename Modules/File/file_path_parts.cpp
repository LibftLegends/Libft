#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../CMA/CMA.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "file_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static char *file_path_duplicate_span(const char *source, ft_size_t length)
{
    char *result = static_cast<char *>(cma_malloc(length + 1));
    if (result == ft_nullptr)
        return (ft_nullptr);
    ft_memcpy(result, source, length);
    result[length] = '\0';
    return (result);
}

static char *file_path_duplicate_empty(void)
{
    return (file_path_duplicate_span("", 0));
}

static ft_string *file_path_to_heap_string(char *path_value)
{
    ft_string *result;
    int32_t initialization_error;

    if (path_value == ft_nullptr)
        return (ft_nullptr);
    result = new (std::nothrow) ft_string();
    if (result == ft_nullptr)
    {
        cma_free(path_value);
        return (ft_nullptr);
    }
    initialization_error = ft_string_take_owned_buffer(*result, path_value,
        ft_strlen_size_t(path_value));
    if (initialization_error != FT_ERR_SUCCESS)
    {
        cma_free(path_value);
        delete result;
        return (ft_nullptr);
    }
    return (result);
}

static ft_bool file_is_path_separator(char character) noexcept
{
    if (character == '/')
        return (FT_TRUE);
    if (character == '\\')
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool file_is_drive_letter(char character) noexcept
{
    if (character >= 'A' && character <= 'Z')
        return (FT_TRUE);
    if (character >= 'a' && character <= 'z')
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_size_t file_path_trimmed_length(const ft_string &path_string) noexcept
{
    ft_size_t trimmed_length;

    const char *path = path_string.c_str();
    trimmed_length = path_string.size();
    while (trimmed_length > 1
        && file_is_path_separator(path[trimmed_length - 1]) == FT_TRUE)
    {
        if (trimmed_length == 3 && path[1] == ':')
            break ;
        trimmed_length--;
    }
    return (trimmed_length);
}

static ft_size_t file_path_last_separator(const ft_string &path_string,
    ft_size_t trimmed_length) noexcept
{
    ft_size_t index;

    const char *path = path_string.c_str();
    index = trimmed_length;
    while (index > 0)
    {
        index--;
        if (file_is_path_separator(path[index]) == FT_TRUE)
            return (index);
    }
    return (ft_string::npos);
}

static ft_size_t file_path_extension_dot(const char *path,
    ft_size_t basename_start, ft_size_t trimmed_length) noexcept
{
    ft_size_t index;

    if (trimmed_length <= basename_start)
        return (ft_string::npos);
    index = trimmed_length;
    while (index > basename_start)
    {
        index--;
        if (path[index] == '.')
        {
            if (index == basename_start)
                return (ft_string::npos);
            if (index + 1 == trimmed_length)
                return (ft_string::npos);
            return (index);
        }
    }
    return (ft_string::npos);
}

ft_bool file_path_is_absolute(const char *path) noexcept
{
    if (path == ft_nullptr)
        return (FT_FALSE);
    if (path[0] == '\0')
        return (FT_FALSE);
    if (file_is_path_separator(path[0]) == FT_TRUE)
        return (FT_TRUE);
    if (file_is_drive_letter(path[0]) == FT_TRUE && path[1] == ':')
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool file_path_is_relative(const char *path) noexcept
{
    if (file_path_is_absolute(path) == FT_TRUE)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_size_t file_path_basename_start(ft_size_t trimmed_length,
    ft_size_t separator_index) noexcept
{
    if (separator_index == ft_string::npos || trimmed_length == 1)
        return (0);
    return (separator_index + 1);
}

char *file_path_basename(const char *path)
{
    ft_string *normalized_path = file_path_normalize(path);
    if (normalized_path == ft_nullptr)
        return (ft_nullptr);
    const ft_size_t trimmed_length = file_path_trimmed_length(*normalized_path);
    const ft_size_t separator_index = file_path_last_separator(*normalized_path, trimmed_length);
    const ft_size_t basename_start = file_path_basename_start(trimmed_length, separator_index);
    char *result = file_path_duplicate_span(normalized_path->c_str() + basename_start,
        trimmed_length - basename_start);
    delete normalized_path;
    return (result);
}

char *file_path_dirname(const char *path)
{
    ft_string *directory = file_path_dirname_string(path);
    if (directory == ft_nullptr)
        return (ft_nullptr);
    char *result = file_path_duplicate_span(directory->c_str(), directory->size());
    delete directory;
    return (result);
}

char *file_path_extension(const char *path)
{
    ft_string *normalized_path;
    ft_size_t trimmed_length;
    ft_size_t separator_index;
    ft_size_t basename_start;
    ft_size_t dot_index;
    char *result;

    normalized_path = file_path_normalize(path);
    if (normalized_path == ft_nullptr)
        return (ft_nullptr);
    result = ft_nullptr;
    if (normalized_path->get_error() != FT_ERR_SUCCESS)
        goto cleanup;
    trimmed_length = file_path_trimmed_length(*normalized_path);
    separator_index = file_path_last_separator(*normalized_path, trimmed_length);
    basename_start = 0;
    if (separator_index != ft_string::npos)
        basename_start = separator_index + 1;
    dot_index = file_path_extension_dot(normalized_path->c_str(), basename_start,
            trimmed_length);
    if (dot_index == ft_string::npos)
        result = file_path_duplicate_empty();
    else
        result = file_path_duplicate_span(normalized_path->c_str() + dot_index,
                trimmed_length - dot_index);
cleanup:
    (void)normalized_path->destroy();
    delete normalized_path;
    return (result);
}

char *file_path_stem(const char *path)
{
    char *basename;
    basename = file_path_basename(path);
    if (basename == ft_nullptr)
        return (ft_nullptr);
    const ft_size_t dot_index = file_path_extension_dot(basename, 0,
        ft_strlen_size_t(basename));
    if (dot_index != ft_string::npos)
        basename[dot_index] = '\0';
    return (basename);
}

ft_string *file_path_basename_string(const char *path)
{
    return (file_path_to_heap_string(file_path_basename(path)));
}

static int32_t file_path_keep_directory(ft_string &path, ft_size_t separator_index)
{
    if (path.empty() == FT_TRUE)
        return (FT_ERR_SUCCESS);
    if (separator_index == ft_string::npos)
        return (path.assign(".", 1));
    ft_size_t directory_length = separator_index;
    if (separator_index == 0 || (separator_index == 2 && path.c_str()[1] == ':'))
        ++directory_length;
    return (path.resize_length(directory_length));
}

ft_string *file_path_dirname_string(const char *path)
{
    ft_string *directory = file_path_normalize(path);
    if (directory == ft_nullptr)
        return (ft_nullptr);
    const ft_size_t trimmed_length = file_path_trimmed_length(*directory);
    const ft_size_t separator_index = file_path_last_separator(*directory, trimmed_length);
    if (file_path_keep_directory(*directory, separator_index) != FT_ERR_SUCCESS)
    {
        delete directory;
        return (ft_nullptr);
    }
    return (directory);
}

ft_string *file_path_split_directory(const char *path, ft_string &basename)
{
    ft_string *directory = file_path_normalize(path);
    if (directory == ft_nullptr)
        return (ft_nullptr);
    const ft_size_t trimmed_length = file_path_trimmed_length(*directory);
    const ft_size_t separator_index = file_path_last_separator(*directory, trimmed_length);
    const ft_size_t basename_start = file_path_basename_start(trimmed_length, separator_index);
    int32_t error_code = basename.assign(directory->c_str() + basename_start,
        trimmed_length - basename_start);
    if (error_code == FT_ERR_SUCCESS)
        error_code = file_path_keep_directory(*directory, separator_index);
    if (error_code != FT_ERR_SUCCESS)
    {
        delete directory;
        return (ft_nullptr);
    }
    return (directory);
}

ft_string *file_path_extension_string(const char *path)
{
    return (file_path_to_heap_string(file_path_extension(path)));
}

ft_string *file_path_stem_string(const char *path)
{
    return (file_path_to_heap_string(file_path_stem(path)));
}

ft_bool file_path_equal(const char *path_left, const char *path_right)
{
    ft_string *normalized_left;
    ft_string *normalized_right;
    ft_bool equal_result;

    if (path_left == ft_nullptr || path_right == ft_nullptr)
        return (FT_FALSE);
    normalized_left = file_path_normalize(path_left);
    if (normalized_left == ft_nullptr)
        return (FT_FALSE);
    normalized_right = file_path_normalize(path_right);
    if (normalized_right == ft_nullptr)
    {
        (void)normalized_left->destroy();
        delete normalized_left;
        return (FT_FALSE);
    }
    if (normalized_left->get_error() != FT_ERR_SUCCESS
        || normalized_right->get_error() != FT_ERR_SUCCESS)
        equal_result = FT_FALSE;
    else
        equal_result = cmp_path_equal(normalized_left->c_str(), normalized_right->c_str());
    (void)normalized_left->destroy();
    delete normalized_left;
    (void)normalized_right->destroy();
    delete normalized_right;
    return (equal_result);
}
