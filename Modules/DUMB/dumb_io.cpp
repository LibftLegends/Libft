#include "dumb_io.hpp"

#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include <errno.h>
#include <stdio.h>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

int32_t ft_read_file(const char *path, char **out_buffer, ft_size_t *out_size)
{
    FILE    *file;
    ft_size_t file_size;
    char    *buffer;
    ft_size_t  bytes_read;
    int32_t system_error;

    if (path == ft_nullptr || out_buffer == ft_nullptr || out_size == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }

    *out_buffer = ft_nullptr;
    *out_size = 0;

    file = fopen(path, "rb");
    if (file == ft_nullptr)
    {
        system_error = cmp_map_system_error_to_ft(errno);
        if (system_error == FT_ERR_SUCCESS)
            system_error = FT_ERR_IO;
        return (system_error);
    }

    if (cmp_file_stream_size(file, &file_size) != FT_ERR_SUCCESS)
    {
        if (fseek(file, 0, SEEK_END) != 0)
        {
            fclose(file);
            return (FT_ERR_IO);
        }
        const int64_t end_position = ftell(file);
        if (end_position < 0 || fseek(file, 0, SEEK_SET) != 0)
        {
            fclose(file);
            return (FT_ERR_IO);
        }
        file_size = static_cast<ft_size_t>(end_position);
    }

    if (file_size == 0)
    {
        fclose(file);
        return (FT_ERR_SUCCESS);
    }
    if (file_size > SIZE_MAX)
    {
        fclose(file);
        return (FT_ERR_OUT_OF_RANGE);
    }
    buffer = static_cast<char *>(cma_malloc(file_size));
    if (buffer == ft_nullptr)
    {
        fclose(file);
        return (FT_ERR_NO_MEMORY);
    }

    bytes_read = fread(buffer, 1, FT_UINT64_TO_NATIVE_SIZE_CAST(file_size), file);
    if (bytes_read != file_size)
    {
        cma_free(buffer);
        fclose(file);
        return (FT_ERR_IO);
    }

    fclose(file);

    *out_buffer = buffer;
    *out_size = file_size;

    return (FT_ERR_SUCCESS);
}

int32_t ft_write_file(const char *path, const char *buffer, ft_size_t size)
{
    FILE    *file;
    ft_size_t  bytes_written;
    int32_t system_error;

    if (path == ft_nullptr || buffer == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (size > SIZE_MAX)
        return (FT_ERR_OUT_OF_RANGE);

    file = fopen(path, "wb");
    if (file == ft_nullptr)
    {
        system_error = cmp_map_system_error_to_ft(errno);
        if (system_error == FT_ERR_SUCCESS)
            system_error = FT_ERR_IO;
        return (system_error);
    }

    if (size > 0)
    {
        bytes_written = fwrite(buffer, 1, FT_UINT64_TO_NATIVE_SIZE_CAST(size), file);
        if (bytes_written != size)
        {
            fclose(file);
            return (FT_ERR_IO);
        }
    }

    if (fclose(file) != 0)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}
