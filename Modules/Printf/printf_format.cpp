#include "printf_internal.hpp"
#include "printf_engine.hpp"
#include "../Basic/basic.hpp"
#include "../System_utils/system_utils.hpp"
#include <climits>

struct s_pf_fd_writer
{
    int32_t file_descriptor;
    ft_size_t pending_size;
    ft_bool failed;
    char buffer[512];
};

static int32_t pf_fd_write_span(s_pf_fd_writer &writer, const char *data, ft_size_t length)
{
    const int64_t written = su_write(writer.file_descriptor, data, length);
    if (written < 0 || static_cast<ft_size_t>(written) != length)
    {
        writer.failed = FT_TRUE;
        return (FT_ERR_IO);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t pf_fd_flush(void *context)
{
    auto &writer = *static_cast<s_pf_fd_writer *>(context);
    if (writer.failed != FT_FALSE)
        return (FT_ERR_IO);
    if (writer.pending_size == 0U)
        return (FT_ERR_SUCCESS);
    const ft_size_t length = writer.pending_size;
    writer.pending_size = 0U;
    return (pf_fd_write_span(writer, writer.buffer, length));
}

static int32_t pf_fd_writer(const char *data, ft_size_t length, void *context,
    ft_size_t *written_count)
{
    if (length == 0U)
        return (FT_ERR_SUCCESS);
    if (length > static_cast<ft_size_t>(INT_MAX) - *written_count)
        return (FT_ERR_OUT_OF_RANGE);
    auto &writer = *static_cast<s_pf_fd_writer *>(context);
    if (length > sizeof(writer.buffer) - writer.pending_size)
    {
        const int32_t error = pf_fd_flush(context);
        if (error != FT_ERR_SUCCESS)
            return (error);
    }
    if (length >= sizeof(writer.buffer))
    {
        const int32_t error = pf_fd_write_span(writer, data, length);
        if (error != FT_ERR_SUCCESS)
            return (error);
    }
    else
    {
        (void)ft_memcpy(writer.buffer + writer.pending_size, data, length);
        writer.pending_size += length;
    }
    *written_count += length;
    return (FT_ERR_SUCCESS);
}

int32_t pf_printf_fd_v(int32_t file_descriptor, const char *format, va_list arguments)
{
    if (file_descriptor < 0 || format == ft_nullptr)
        return (-1);
    s_pf_fd_writer writer;
    writer.file_descriptor = file_descriptor;
    writer.pending_size = 0U;
    writer.failed = FT_FALSE;
    ft_size_t written_count = 0U;
    const int32_t format_error = pf_engine_format_with_flush(format, arguments,
        pf_fd_writer, pf_fd_flush, &writer, &written_count);
    const int32_t write_error = pf_fd_flush(&writer);
    if (format_error != FT_ERR_SUCCESS || write_error != FT_ERR_SUCCESS)
        return (-1);
    return (static_cast<int32_t>(written_count));
}
