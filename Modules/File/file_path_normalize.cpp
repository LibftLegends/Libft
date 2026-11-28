#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "file_utils.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static void file_path_delete_string(ft_string *string) noexcept
{
    if (string == ft_nullptr)
        return ;
    (void)string->destroy();
    delete string;
    return ;
}

static ft_bool file_path_is_separator(char character) noexcept
{
    if (character == '/' || character == '\\')
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool file_path_is_drive_letter(char character) noexcept
{
    if (character >= 'A' && character <= 'Z')
        return (FT_TRUE);
    if (character >= 'a' && character <= 'z')
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool file_path_segment_is_dot(const char *data, ft_size_t start,
    ft_size_t length) noexcept
{
    if (length == 1 && data[start] == '.')
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool file_path_segment_is_dot_dot(const char *data, ft_size_t start,
    ft_size_t length) noexcept
{
    if (length == 2 && data[start] == '.' && data[start + 1] == '.')
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool file_path_result_ends_with_separator(const ft_string &result,
    char path_separator) noexcept
{
    if (result.size() == 0)
        return (FT_FALSE);
    if (result.c_str()[result.size() - 1] == path_separator)
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool file_path_last_segment_is_dot_dot(const ft_string &result,
    ft_size_t root_length, char path_separator) noexcept
{
    ft_size_t index;
    ft_size_t segment_start;

    if (result.size() <= root_length)
        return (FT_FALSE);
    index = result.size();
    while (index > root_length && result.c_str()[index - 1] == path_separator)
        --index;
    segment_start = index;
    while (segment_start > root_length
        && result.c_str()[segment_start - 1] != path_separator)
        --segment_start;
    if (index - segment_start == 2 && result.c_str()[segment_start] == '.'
        && result.c_str()[segment_start + 1] == '.')
        return (FT_TRUE);
    return (FT_FALSE);
}

static int32_t file_path_remove_last_segment(ft_string &result,
    ft_size_t root_length, char path_separator) noexcept
{
    ft_size_t erase_start;

    if (result.size() <= root_length)
        return (FT_ERR_NOT_FOUND);
    if (file_path_last_segment_is_dot_dot(result, root_length, path_separator) == FT_TRUE)
        return (FT_ERR_NOT_FOUND);
    erase_start = result.size();
    while (erase_start > root_length
        && result.c_str()[erase_start - 1] != path_separator)
        --erase_start;
    if (erase_start > root_length && result.c_str()[erase_start - 1] == path_separator)
        --erase_start;
    return (result.erase(erase_start, result.size() - erase_start));
}

static int32_t file_path_append_segment(ft_string &result, const char *data,
    ft_size_t start, ft_size_t length, char path_separator) noexcept
{
    int32_t error_code;

    if (result.size() != 0
        && file_path_result_ends_with_separator(result, path_separator) == FT_FALSE)
    {
        error_code = result.append(path_separator);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    return (result.append(data + start, length));
}

static int32_t file_path_append_root(ft_string &result, const char *data,
    char path_separator, ft_size_t *root_length_out) noexcept
{
    ft_size_t index;
    int32_t error_code;

    if (file_path_is_drive_letter(data[0]) == FT_TRUE && data[1] == ':')
    {
        error_code = result.append(data[0]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = result.append(':');
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (file_path_is_separator(data[2]) == FT_TRUE)
        {
            error_code = result.append(path_separator);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            if (root_length_out != ft_nullptr)
                *root_length_out = 3;
            return (FT_ERR_SUCCESS);
        }
        if (root_length_out != ft_nullptr)
            *root_length_out = 2;
        return (FT_ERR_SUCCESS);
    }
    if (file_path_is_separator(data[0]) == FT_TRUE)
    {
        if (file_path_is_separator(data[1]) == FT_TRUE
            && file_path_is_separator(data[2]) == FT_FALSE)
        {
            error_code = result.append(path_separator);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            error_code = result.append(path_separator);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            if (root_length_out != ft_nullptr)
                *root_length_out = 2;
            return (FT_ERR_SUCCESS);
        }
        error_code = result.append(path_separator);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index = 0;
        while (file_path_is_separator(data[index]) == FT_TRUE)
            ++index;
        if (root_length_out != ft_nullptr)
            *root_length_out = index;
        return (FT_ERR_SUCCESS);
    }
    if (root_length_out != ft_nullptr)
        *root_length_out = 0;
    return (FT_ERR_SUCCESS);
}

static ft_size_t file_path_root_length(const ft_string &result,
    char path_separator) noexcept
{
    if (result.size() >= 3 && result.c_str()[1] == ':'
        && result.c_str()[2] == path_separator)
        return (3);
    if (result.size() >= 2 && result.c_str()[0] == path_separator
        && result.c_str()[1] == path_separator)
        return (2);
    if (result.size() >= 1 && result.c_str()[0] == path_separator)
        return (1);
    if (result.size() >= 2 && result.c_str()[1] == ':')
        return (2);
    return (0);
}

static int32_t file_path_process_segment(ft_string &result, const char *data,
    ft_size_t start, ft_size_t length, char path_separator) noexcept
{
    ft_size_t root_length;
    int32_t error_code;

    if (length == 0 || file_path_segment_is_dot(data, start, length) == FT_TRUE)
        return (FT_ERR_SUCCESS);
    root_length = file_path_root_length(result, path_separator);
    if (file_path_segment_is_dot_dot(data, start, length) == FT_TRUE)
    {
        error_code = file_path_remove_last_segment(result, root_length,
                path_separator);
        if (error_code == FT_ERR_SUCCESS)
            return (FT_ERR_SUCCESS);
        if (root_length != 0)
            return (FT_ERR_SUCCESS);
    }
    return (file_path_append_segment(result, data, start, length, path_separator));
}

ft_string *file_path_normalize(const char *path)
{
    ft_string *result;
    char path_separator;
    ft_bool use_native_separator;
    ft_size_t index;
    ft_size_t root_length;
    ft_size_t segment_start;
    int32_t error_code;

    result = new (std::nothrow) ft_string();
    if (result == ft_nullptr)
        return (ft_nullptr);
    if (result->initialize() != FT_ERR_SUCCESS)
    {
        delete result;
        return (ft_nullptr);
    }
    if (path == ft_nullptr || path[0] == '\0')
        return (result);
    if (path[0] != '\0' && file_path_is_separator(path[0]) == FT_TRUE
        && path[1] == '\0')
    {
        if (result->append('/') != FT_ERR_SUCCESS)
            return (file_path_delete_string(result), ft_nullptr);
        return (result);
    }
    use_native_separator = FT_FALSE;
    if (path[0] != '\0')
    {
        if (file_path_is_drive_letter(path[0]) == FT_TRUE && path[1] == ':')
            use_native_separator = FT_TRUE;
        else if (file_path_is_separator(path[0]) == FT_TRUE
            && path[1] != '\0')
            use_native_separator = FT_TRUE;
    }
    path_separator = '/';
    if (use_native_separator == FT_TRUE)
        path_separator = cmp_path_separator();
    error_code = file_path_append_root(*result, path, path_separator, &root_length);
    if (error_code != FT_ERR_SUCCESS)
    {
        file_path_delete_string(result);
        return (ft_nullptr);
    }
    index = root_length;
    segment_start = index;
    while (path[index] != '\0')
    {
        if (file_path_is_separator(path[index]) == FT_TRUE)
        {
            error_code = file_path_process_segment(*result, path, segment_start,
                    index - segment_start, path_separator);
            if (error_code != FT_ERR_SUCCESS)
            {
                file_path_delete_string(result);
                return (ft_nullptr);
            }
            while (file_path_is_separator(path[index]) == FT_TRUE)
                ++index;
            segment_start = index;
        }
        else
            ++index;
    }
    error_code = file_path_process_segment(*result, path, segment_start,
            index - segment_start, path_separator);
    if (error_code != FT_ERR_SUCCESS)
    {
        file_path_delete_string(result);
        return (ft_nullptr);
    }
    if (result->size() == 0)
    {
        if (result->append('.') != FT_ERR_SUCCESS)
        {
            file_path_delete_string(result);
            return (ft_nullptr);
        }
    }
    if (file_path_is_separator(path[index - 1]) == FT_TRUE && result->size() > 1
        && file_path_result_ends_with_separator(*result, path_separator) == FT_FALSE)
    {
        if (result->append(path_separator) != FT_ERR_SUCCESS)
        {
            file_path_delete_string(result);
            return (ft_nullptr);
        }
    }
    return (result);
}
