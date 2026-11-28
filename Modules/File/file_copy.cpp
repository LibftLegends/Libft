#include "file_internal.hpp"

#include "../CPP_class/class_file.hpp"
#include "../CPP_class/class_file_stream.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if defined(_WIN32) || defined(_WIN64)
# include <fcntl.h>
# define FILE_BINARY_FLAG O_BINARY
#else
# include <fcntl.h>
# define FILE_BINARY_FLAG 0
#endif

ft_size_t file_default_copy_buffer_size(void) noexcept
{
    return (ft_file_default_buffer_size());
}

int32_t file_copy_using_buffer(const char *source_path, const char *destination_path,
    char *buffer, ft_size_t buffer_size)
{
    int32_t source_flags;
    int32_t destination_flags;

    if (source_path == ft_nullptr || destination_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_file source_file;
    ft_file destination_file;
    source_flags = O_RDONLY | FILE_BINARY_FLAG;
    if (source_file.open(source_path, source_flags) != FT_ERR_SUCCESS)
        return (FT_ERR_IO);
    destination_flags = O_WRONLY | O_CREAT | O_TRUNC | FILE_BINARY_FLAG;
    if (destination_file.open(destination_path, destination_flags, 0644) != FT_ERR_SUCCESS)
        return (FT_ERR_IO);
    int32_t copy_result;
    if (buffer == ft_nullptr)
        copy_result = ft_file_stream_copy(source_file, destination_file, buffer_size);
    else
        copy_result = ft_file_stream_copy_into_buffer(source_file, destination_file, buffer, buffer_size);
    if (copy_result != FT_ERR_SUCCESS)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

int32_t file_copy_with_buffer(const char *source_path, const char *destination_path, ft_size_t buffer_size)
{
    return (file_copy_using_buffer(source_path, destination_path, ft_nullptr, buffer_size));
}

int32_t file_copy(const char *source_path, const char *destination_path)
{
    return (file_copy_with_buffer(source_path, destination_path, file_default_copy_buffer_size()));
}
