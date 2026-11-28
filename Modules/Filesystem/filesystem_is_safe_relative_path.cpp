#include "filesystem_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_bool filesystem_path_is_parent_reference(const char *path) noexcept
{
    if (path == ft_nullptr)
        return (FT_FALSE);
    if (path[0] == '.' && path[1] == '.' && path[2] == '\0')
        return (FT_TRUE);
    if (path[0] == '.' && path[1] == '.'
        && (path[2] == '/' || path[2] == '\\'))
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool filesystem_can_be_relative_path(const char *path) noexcept
{
    return (path != ft_nullptr && path[0] != '\0' && filesystem_is_relative(path) == FT_TRUE);
}

ft_string *filesystem_normalize_safe_relative_path(const char *path)
{
    if (filesystem_can_be_relative_path(path) == FT_FALSE)
        return (ft_nullptr);
    ft_string *normalized_path = filesystem_normalize_path(path);
    if (normalized_path != ft_nullptr && filesystem_path_is_parent_reference(normalized_path->c_str()) == FT_TRUE)
    {
        delete normalized_path;
        return (ft_nullptr);
    }
    return (normalized_path);
}

ft_bool filesystem_is_safe_relative_path(const char *path) noexcept
{
    if (filesystem_can_be_relative_path(path) == FT_FALSE)
        return (FT_FALSE);
    // A parent reference can only survive normalization if the input contains "..".
    if (ft_strstr(path, "..") == ft_nullptr)
        return (FT_TRUE);
    ft_string *normalized_path = filesystem_normalize_safe_relative_path(path);
    if (normalized_path == ft_nullptr)
        return (FT_FALSE);
    delete normalized_path;
    return (FT_TRUE);
}
