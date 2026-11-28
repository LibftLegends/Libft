#include "printf.hpp"
#include "printf_engine.hpp"
#include "printf_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include <atomic>
#include <cerrno>
#include <climits>
#include <memory>

static int64_t pf_default_ftell(FILE *stream)
{
    return (ftell(stream));
}

static std::atomic<t_pf_tmpfile_function> g_pf_tmpfile_function{tmpfile};
static std::atomic<t_pf_fflush_function> g_pf_fflush_function{fflush};
static std::atomic<t_pf_ftell_function> g_pf_ftell_function{pf_default_ftell};

void pf_set_tmpfile_function(t_pf_tmpfile_function function)
{
    if (function == ft_nullptr)
        function = tmpfile;
    g_pf_tmpfile_function.store(function, std::memory_order_relaxed);
    return ;
}

void pf_reset_tmpfile_function(void)
{
    pf_set_tmpfile_function(tmpfile);
    return ;
}

void pf_set_fflush_function(t_pf_fflush_function function)
{
    if (function == ft_nullptr)
        function = fflush;
    g_pf_fflush_function.store(function, std::memory_order_relaxed);
    return ;
}

void pf_reset_fflush_function(void)
{
    pf_set_fflush_function(fflush);
    return ;
}

int32_t pf_flush_stream(FILE *stream)
{
    if (stream == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    errno = 0;
    if (g_pf_fflush_function.load(std::memory_order_relaxed)(stream) != 0)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

void pf_set_ftell_function(t_pf_ftell_function function)
{
    if (function == ft_nullptr)
        function = pf_default_ftell;
    g_pf_ftell_function.store(function, std::memory_order_relaxed);
    return ;
}

void pf_reset_ftell_function(void)
{
    pf_set_ftell_function(pf_default_ftell);
    return ;
}

struct s_pf_buffer_writer
{
    char *buffer;
    ft_size_t capacity;
    ft_size_t copied;
};

static int32_t pf_write_buffer(const char *data, ft_size_t length, void *context,
    ft_size_t *written_count)
{
    if (length > static_cast<ft_size_t>(INT_MAX) - *written_count)
        return (FT_ERR_OUT_OF_RANGE);
    auto *writer = static_cast<s_pf_buffer_writer *>(context);
    ft_size_t copied = writer->capacity - writer->copied;
    if (copied > length)
        copied = length;
    if (copied != 0U)
    {
        (void)ft_memcpy(writer->buffer + writer->copied, data, copied);
        writer->copied += copied;
    }
    *written_count += length;
    return (FT_ERR_SUCCESS);
}

static int32_t pf_format_buffer(char *buffer, ft_size_t size, const char *format,
    va_list arguments)
{
    s_pf_buffer_writer writer{buffer, size, 0U};
    if (writer.capacity != 0U)
        writer.capacity -= 1U;
    ft_size_t written_count = 0U;
    va_list copy;
    va_copy(copy, arguments);
    const int32_t error = pf_engine_format(format, copy, pf_write_buffer, &writer, &written_count);
    va_end(copy);
    if (error != FT_ERR_SUCCESS)
        return (-1);
    if (size != 0U)
        buffer[writer.copied] = '\0';
    return (static_cast<int32_t>(written_count));
}

static int32_t pf_format_buffer_with_hooks(char *buffer, ft_size_t size,
    const char *format, va_list arguments, t_pf_tmpfile_function open_stream,
    t_pf_ftell_function tell_stream)
{
    std::unique_ptr<FILE, decltype(&fclose)> file(open_stream(), fclose);
    FILE *stream = file.get();
    if (stream == ft_nullptr)
        return (-1);
    const int32_t written = ft_vfprintf(stream, format, arguments);
    int32_t result = -1;
    if (written >= 0 && pf_flush_stream(stream) == FT_ERR_SUCCESS)
    {
        const int64_t position = tell_stream(stream);
        if (position >= 0)
        {
            rewind(stream);
            if (size != 0U)
            {
                ft_size_t length = static_cast<ft_size_t>(position);
                if (length >= size)
                    length = size - 1U;
                const ft_size_t copied = fread(buffer, 1U, FT_UINT64_TO_NATIVE_SIZE_CAST(length), stream);
                buffer[copied] = '\0';
            }
            result = written;
        }
    }
    return (result);
}

int32_t pf_vsnprintf(char *buffer, ft_size_t size, const char *format, va_list arguments)
{
    if (format == ft_nullptr || (buffer == ft_nullptr && size != 0U))
        return (-1);
    const t_pf_tmpfile_function open_stream = g_pf_tmpfile_function.load(std::memory_order_relaxed);
    const t_pf_ftell_function tell_stream = g_pf_ftell_function.load(std::memory_order_relaxed);
    int32_t written;
    if (open_stream != tmpfile || tell_stream != pf_default_ftell
        || g_pf_fflush_function.load(std::memory_order_relaxed) != fflush)
        written = pf_format_buffer_with_hooks(buffer, size, format, arguments, open_stream, tell_stream);
    else
        written = pf_format_buffer(buffer, size, format, arguments);
    if (written < 0 && size != 0U)
        buffer[0] = '\0';
    return (written);
}
