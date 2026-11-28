#include "filesystem.hpp"
#include "../File/file_internal.hpp"

int32_t filesystem_atomic_write(const char *path, const void *data, ft_size_t size)
{
    return (file_replace_contents(path, static_cast<const char *>(data), size, FT_FALSE));
}
