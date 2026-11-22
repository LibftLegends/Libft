#include "file_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <cstdio>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

void file_observability_emit(const char *operation, const char *path,
    ft_observability_trace_phase phase, int32_t error_code,
    ft_size_t bytes_read, ft_size_t bytes_written)
{
    ft_observability_trace_event event;
    ft_bool success;

    event.module = FT_OBSERVABILITY_MODULE_FILE;
    event.phase = phase;
    event.operation = operation;
    event.resource = path;
    event.error_code = error_code;
    event.bytes_read = bytes_read;
    event.bytes_written = bytes_written;
    (void)observability_trace_emit(&event);
    if (phase == FT_OBSERVABILITY_TRACE_FINISH)
    {
        success = FT_FALSE;
        if (error_code == FT_ERR_SUCCESS)
            success = FT_TRUE;
        (void)observability_record_operation(FT_OBSERVABILITY_MODULE_FILE,
            success, bytes_read, bytes_written);
    }
    return ;
}

static int32_t file_write_buffer(FILE *file_stream, const char *data,
    ft_size_t size)
{
    ft_size_t written_total;
    size_t written_now;

    if (file_stream == ft_nullptr || (data == ft_nullptr && size > 0))
        return (FT_ERR_INVALID_POINTER);
    written_total = 0;
    while (written_total < size)
    {
        written_now = std::fwrite(data + written_total, 1,
                size - written_total, file_stream);
        if (written_now == 0)
            return (FT_ERR_IO);
        written_total = written_total + written_now;
    }
    return (FT_ERR_SUCCESS);
}

int32_t file_read_all(const char *path, ft_string &output)
{
    FILE *file_stream;
    int64_t file_size_long;
    ft_size_t file_size;
    char *content_pointer;
    size_t read_count;
    int32_t error_code;

    if (path == ft_nullptr)
    {
        file_observability_emit("read_all", path, FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_INVALID_ARGUMENT, 0, 0);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    file_observability_emit("read_all", path, FT_OBSERVABILITY_TRACE_START,
        FT_ERR_SUCCESS, 0, 0);
    file_stream = ft_fopen(path, "rb");
    if (file_stream == ft_nullptr)
    {
        file_observability_emit("read_all", path, FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_FILE_OPEN_FAILED, 0, 0);
        return (FT_ERR_FILE_OPEN_FAILED);
    }
    error_code = FT_ERR_SUCCESS;
    file_size_long = 0;
    file_size = 0;
    content_pointer = ft_nullptr;
    if (std::fseek(file_stream, 0, SEEK_END) != 0)
        error_code = FT_ERR_IO;
    if (error_code == FT_ERR_SUCCESS)
    {
        file_size_long = std::ftell(file_stream);
        if (file_size_long < 0)
            error_code = FT_ERR_IO;
    }
    if (error_code == FT_ERR_SUCCESS && std::fseek(file_stream, 0, SEEK_SET) != 0)
        error_code = FT_ERR_IO;
    if (error_code == FT_ERR_SUCCESS)
    {
        file_size = static_cast<ft_size_t>(file_size_long);
        content_pointer = static_cast<char *>(cma_malloc(file_size + 1));
        if (content_pointer == ft_nullptr)
            error_code = FT_ERR_NO_MEMORY;
    }
    if (error_code == FT_ERR_SUCCESS)
    {
        read_count = std::fread(content_pointer, 1, file_size, file_stream);
        if (read_count != file_size)
            error_code = FT_ERR_IO;
        else
            content_pointer[file_size] = '\0';
    }
    if (ft_fclose(file_stream) != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    if (error_code == FT_ERR_SUCCESS)
    {
        char *previous_buffer;

        error_code = ft_string_take_owned_buffer(output, content_pointer, file_size, &previous_buffer);
        if (error_code == FT_ERR_SUCCESS)
            content_pointer = previous_buffer;
    }
    if (error_code != FT_ERR_SUCCESS)
        file_size = 0;
    file_observability_emit("read_all", path, FT_OBSERVABILITY_TRACE_FINISH,
        error_code, file_size, 0);
    // The trace resource may point into the output's previous contents.
    cma_free(content_pointer);
    return (error_code);
}

int32_t file_write_all(const char *path, const char *data, ft_size_t size)
{
    FILE *file_stream;
    int32_t error_code;
    int32_t close_error;
    ft_size_t bytes_written;

    if (path == ft_nullptr || (data == ft_nullptr && size > 0))
    {
        file_observability_emit("write_all", path, FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_INVALID_POINTER, 0, 0);
        return (FT_ERR_INVALID_POINTER);
    }
    file_observability_emit("write_all", path, FT_OBSERVABILITY_TRACE_START,
        FT_ERR_SUCCESS, 0, 0);
    file_stream = ft_fopen(path, "wb");
    if (file_stream == ft_nullptr)
    {
        file_observability_emit("write_all", path, FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_FILE_OPEN_FAILED, 0, 0);
        return (FT_ERR_FILE_OPEN_FAILED);
    }
    error_code = file_write_buffer(file_stream, data, size);
    close_error = ft_fclose(file_stream);
    if (error_code == FT_ERR_SUCCESS)
        error_code = close_error;
    bytes_written = 0;
    if (error_code == FT_ERR_SUCCESS)
        bytes_written = size;
    file_observability_emit("write_all", path, FT_OBSERVABILITY_TRACE_FINISH,
        error_code, 0, bytes_written);
    return (error_code);
}

int32_t file_write_all_to_descriptor(int32_t file_descriptor,
    const char *data, ft_size_t size)
{
    ft_size_t total_written;
    int64_t bytes_written;
    int32_t error_code;

    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (data == ft_nullptr && size > 0)
        return (FT_ERR_INVALID_POINTER);
    total_written = 0;
    while (total_written < size)
    {
        bytes_written = 0;
        error_code = cmp_write(file_descriptor, data + total_written,
                size - total_written, &bytes_written);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (bytes_written <= 0)
            return (FT_ERR_IO);
        total_written += static_cast<ft_size_t>(bytes_written);
    }
    return (FT_ERR_SUCCESS);
}

int32_t file_write_all_atomic(const char *path, const char *data, ft_size_t size)
{
    if (path == ft_nullptr || (data == ft_nullptr && size > 0))
    {
        file_observability_emit("write_all_atomic", path,
            FT_OBSERVABILITY_TRACE_FINISH, FT_ERR_INVALID_POINTER, 0, 0);
        return (FT_ERR_INVALID_POINTER);
    }
    file_observability_emit("write_all_atomic", path,
        FT_OBSERVABILITY_TRACE_START, FT_ERR_SUCCESS, 0, 0);
    const int32_t error_code = file_replace_contents(path, data, size, FT_FALSE);
    ft_size_t bytes_written = 0;
    if (error_code == FT_ERR_SUCCESS)
        bytes_written = size;
    file_observability_emit("write_all_atomic", path,
        FT_OBSERVABILITY_TRACE_FINISH, error_code, 0, bytes_written);
    return (error_code);
}
