#include "filesystem.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_string *filesystem_string_from_owned_c_string(char *value)
{
    ft_string *result;

    if (value == ft_nullptr)
        return (ft_nullptr);
    result = new (std::nothrow) ft_string();
    if (result == ft_nullptr)
    {
        cma_free(value);
        return (ft_nullptr);
    }
    if (ft_string_take_owned_buffer(*result, value, ft_strlen_size_t(value)) != FT_ERR_SUCCESS)
    {
        cma_free(value);
        delete result;
        return (ft_nullptr);
    }
    return (result);
}

ft_string *filesystem_canonical_path(const char *path)
{
    char *owned_path;
    int32_t error_code;

    if (path == ft_nullptr || path[0] == '\0')
        return (ft_nullptr);
    error_code = cmp_path_canonical(path, &owned_path);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (owned_path == ft_nullptr)
        return (ft_nullptr);
    return (filesystem_string_from_owned_c_string(owned_path));
}
