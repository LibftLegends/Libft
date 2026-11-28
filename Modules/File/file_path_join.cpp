#include "../CPP_class/class_string.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "file_internal.hpp"
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

static void file_path_convert_separator(ft_string &path_value,
    char source_separator, char destination_separator) noexcept
{
    char *data;
    ft_size_t index;

    if (source_separator == destination_separator)
        return ;
    data = path_value.print();
    if (data == ft_nullptr)
        return ;
    const ft_size_t length = path_value.size();
    index = 0;
    while (index < length)
    {
        if (data[index] == source_separator)
            data[index] = destination_separator;
        index++;
    }
    return ;
}

int32_t file_path_append_normalized_relative(ft_string &left, ft_string &right)
{
    char separator = '/';
    if (left.empty() == FT_TRUE || file_path_is_absolute(left.c_str()) == FT_TRUE)
        separator = cmp_path_separator();
    if (left.empty() == FT_FALSE && left.c_str()[left.size() - 1] != separator)
    {
        const int32_t error_code = left.append(separator);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    file_path_convert_separator(right, '/', separator);
    return (left.append(right.c_str(), right.size()));
}

ft_string *file_path_join(const char *path_left, const char *path_right)
{
    ft_string *left;
    ft_string *right;

    left = file_path_normalize(path_left);
    if (left == ft_nullptr)
        return (ft_nullptr);
    right = file_path_normalize(path_right);
    if (right == ft_nullptr)
    {
        file_path_delete_string(left);
        return (ft_nullptr);
    }
    if (file_path_is_absolute(right->c_str()) == FT_TRUE)
    {
        file_path_convert_separator(*right, '\\', '/');
        file_path_delete_string(left);
        return (right);
    }
    const int32_t error_code = file_path_append_normalized_relative(*left, *right);
    file_path_delete_string(right);
    if (error_code != FT_ERR_SUCCESS)
    {
        file_path_delete_string(left);
        return (ft_nullptr);
    }
    return (left);
}
