#include "../Compatebility/compatebility_internal.hpp"
#include "open_dir.hpp"

static int32_t file_check_native_directory(const char *path)
{
    int32_t exists = 0;
    int32_t error_code;
    const int32_t status = cmp_directory_exists(path, &exists, &error_code);
    if (status != FT_ERR_SUCCESS)
        return (error_code);
    if (exists == 0)
        return (FT_ERR_NOT_FOUND);
    return (FT_ERR_SUCCESS);
}

int32_t file_dir_exists(const char *rel_path)
{
    if (rel_path == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    char alternate_separator = '\\';
    if (cmp_path_separator() == '\\')
        alternate_separator = '/';
    if (ft_strchr(rel_path, alternate_separator) == ft_nullptr)
        return (file_check_native_directory(rel_path));
    ft_string path;
    const int32_t error_code = path.initialize(rel_path);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    char *data = path.data();
    if (data == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    cmp_normalize_slashes(data);
    return (file_check_native_directory(data));
}
