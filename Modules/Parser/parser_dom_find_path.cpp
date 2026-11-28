#include "dom.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"

int32_t ft_dom_find_path(const ft_dom_node *root, const ft_string &path, const ft_dom_node **out_node) noexcept
{
    if (!root || !out_node)
        return (FT_ERR_INVALID_ARGUMENT);
    *out_node = ft_nullptr;
    const char *segment = path.c_str();
    if (!segment)
        return (FT_ERR_INVALID_ARGUMENT);
    const char *end = segment + path.size();
    const ft_dom_node *current = root;
    while (segment < end)
    {
        const char *separator = static_cast<const char *>(ft_memchr(segment, '/', static_cast<ft_size_t>(end - segment)));
        const char *segment_end = end;
        if (separator)
            segment_end = separator;
        const ft_size_t length = static_cast<ft_size_t>(segment_end - segment);
        if (length == 0)
            return (FT_ERR_INVALID_ARGUMENT);
        current = current->find_child(segment, length);
        if (!current)
            return (FT_ERR_NOT_FOUND);
        if (!separator)
            break ;
        segment = separator + 1;
    }
    *out_node = current;
    return (FT_ERR_SUCCESS);
}
