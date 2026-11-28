#include "filesystem_internal.hpp"
#include "../File/file_internal.hpp"

static ft_string *filesystem_check_joined_containment(const char *root_path, ft_string *path)
{
    if (path != ft_nullptr && filesystem_is_inside_root(root_path, path->c_str()) == FT_FALSE)
    {
        delete path;
        return (ft_nullptr);
    }
    return (path);
}

ft_string *filesystem_safe_join_path(const char *root_path,
    const char *relative_path)
{
    if (root_path == ft_nullptr)
        return (ft_nullptr);
    ft_string *relative = filesystem_normalize_safe_relative_path(relative_path);
    if (relative == ft_nullptr)
        return (ft_nullptr);
    if (file_path_is_absolute(relative->c_str()) == FT_TRUE)
    {
        delete relative;
        return (filesystem_check_joined_containment(root_path,
            filesystem_join_path(root_path, relative_path)));
    }
    ft_string *joined = filesystem_normalize_path(root_path);
    if (joined == ft_nullptr)
    {
        delete relative;
        return (ft_nullptr);
    }
    const ft_bool needs_containment_check = joined->empty() == FT_TRUE
        || (*joined == ".") == FT_TRUE || (*relative == ".") == FT_TRUE;
    const int32_t error_code = file_path_append_normalized_relative(*joined, *relative);
    delete relative;
    if (error_code != FT_ERR_SUCCESS)
    {
        delete joined;
        return (ft_nullptr);
    }
    // Nonempty normalized paths without leading parents only extend the root.
    if (needs_containment_check == FT_TRUE)
        return (filesystem_check_joined_containment(root_path, joined));
    return (joined);
}
