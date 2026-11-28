#include "class_file_stream.hpp"

#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include <climits>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

ft_size_t ft_file_default_buffer_size(void) noexcept
{
    return (65536);
}

int32_t ft_file_stream_copy_into_buffer(ft_file &source_file, ft_file &destination_file,
    char *buffer, ft_size_t buffer_size) noexcept
{
    if (buffer == ft_nullptr || buffer_size == 0)
        return (-1);
    ft_size_t maximum_readable = buffer_size;
    if (maximum_readable > static_cast<ft_size_t>(INT_MAX))
        maximum_readable = static_cast<ft_size_t>(INT_MAX);
    const int32_t read_size = static_cast<int32_t>(maximum_readable);
    while (FT_TRUE)
    {
        const ssize_t bytes_read = source_file.read(buffer, read_size);
        if (bytes_read <= 0)
        {
            if (bytes_read < 0)
                return (-1);
            return (0);
        }
        ft_size_t write_offset = 0;
        while (write_offset < static_cast<ft_size_t>(bytes_read))
        {
            const ssize_t bytes_written = destination_file.write_buffer(buffer + write_offset,
                static_cast<ft_size_t>(bytes_read) - write_offset);
            if (bytes_written <= 0)
                return (-1);
            write_offset += static_cast<ft_size_t>(bytes_written);
        }
    }
}

int32_t ft_file_stream_copy(ft_file &source_file, ft_file &destination_file,
    ft_size_t buffer_size) noexcept
{
    if (buffer_size == 0)
        buffer_size = ft_file_default_buffer_size();
    char *buffer = static_cast<char *>(cma_malloc(buffer_size));
    if (buffer == ft_nullptr)
        return (-1);
    const int32_t result = ft_file_stream_copy_into_buffer(source_file, destination_file,
        buffer, buffer_size);
    cma_free(buffer);
    return (result);
}
