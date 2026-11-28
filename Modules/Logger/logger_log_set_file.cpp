#include "logger_internal.hpp"
#include <cerrno>
#include <fcntl.h>
#include <new>
#include "../Compatebility/compatebility_internal.hpp"
#include "../Basic/basic.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno.hpp"

int32_t logger_write_file_unlocked(s_file_sink &sink, const char *message)
{
    if (message == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_size_t length = ft_strlen_size_t(message);
    if (length == 0U)
        return (FT_ERR_SUCCESS);
    const int64_t written = su_write(sink.file_descriptor, message, length);
    if (written < 0 || static_cast<ft_size_t>(written) != length)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

int32_t ft_file_sink(const char *message, void *user_data)
{
    auto *sink = static_cast<s_file_sink *>(user_data);
    if (sink == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bool acquired = FT_FALSE;
    if (file_sink_lock(sink, &acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    const int32_t error = logger_write_file_unlocked(*sink, message);
    file_sink_unlock(sink, acquired);
    return (error);
}

int32_t logger_configure_file(const char *path, ft_size_t max_size)
{
    s_file_sink *sink;
    int32_t          file_descriptor;
    int32_t          prepare_status;

    if (path == ft_nullptr || path[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    file_descriptor = cmp_open(path, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (file_descriptor == -1)
        return (FT_ERR_IO);
    sink = new(std::nothrow) s_file_sink;
    if (!sink)
    {
        (void)cmp_close(file_descriptor);
        return (FT_ERR_NO_MEMORY);
    }
    sink->file_descriptor = file_descriptor;
    int32_t error = sink->path.initialize(path);
    sink->max_size = max_size;
    sink->retention_count = 1;
    sink->max_age_seconds = 0;
    if (error != FT_ERR_SUCCESS)
    {
        (void)cmp_close(file_descriptor);
        delete sink;
        return (error);
    }
    prepare_status = file_sink_prepare_thread_safety(sink);
    if (prepare_status != FT_ERR_SUCCESS)
    {
        (void)cmp_close(file_descriptor);
        delete sink;
        return (prepare_status);
    }
    error = ft_log_add_sink(ft_file_sink, sink);
    if (error != FT_ERR_SUCCESS)
    {
        (void)cmp_close(file_descriptor);
        file_sink_teardown_thread_safety(sink);
        delete sink;
        return (error);
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_log_set_file(const char *path, ft_size_t max_size)
{
    return (-static_cast<int32_t>(logger_configure_file(path, max_size) != FT_ERR_SUCCESS));
}
