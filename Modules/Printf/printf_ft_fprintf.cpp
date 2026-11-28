#include "printf.hpp"
#include "printf_internal.hpp"
#include "printf_engine.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include <cstdio>
#include <cstdarg>
#include <climits>
#include <cerrno>

static int32_t pf_stream_writer(const char *data, ft_size_t length, void *context,
    ft_size_t *written_count)
{
    if (length == 0U)
        return (FT_ERR_SUCCESS);
    if (length > static_cast<ft_size_t>(INT_MAX) - *written_count)
        return (FT_ERR_OUT_OF_RANGE);
    FILE *stream = static_cast<FILE *>(context);
    errno = 0;
    if (fwrite(data, 1U, FT_UINT64_TO_NATIVE_SIZE_CAST(length), stream) != length)
        return (FT_ERR_IO);
    *written_count += length;
    return (FT_ERR_SUCCESS);
}

int32_t ft_vfprintf(FILE *stream, const char *format, va_list arguments)
{
    if (stream == ft_nullptr || format == ft_nullptr)
        return (-1);
    try
    {
        ft_size_t written_count = 0U;
        if (pf_engine_format(format, arguments, pf_stream_writer, stream, &written_count) != FT_ERR_SUCCESS
            || pf_flush_stream(stream) != FT_ERR_SUCCESS)
            return (-1);
        return (static_cast<int32_t>(written_count));
    }
    catch (...)
    {
        return (-1);
    }
}

int32_t ft_fprintf(FILE *stream, const char *format, ...)
{
    if (stream == ft_nullptr || format == ft_nullptr)
        return (-1);
    va_list arguments;
    va_start(arguments, format);
    const int32_t result = ft_vfprintf(stream, format, arguments);
    va_end(arguments);
    return (result);
}
