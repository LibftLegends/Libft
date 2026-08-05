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

int32_t ft_file_sink(const char *message, void *user_data)
{
    s_file_sink *sink;
    ft_size_t length;
    ft_size_t total_written;
    ft_bool lock_acquired;
    int64_t write_result;

    sink = static_cast<s_file_sink *>(user_data);
    if (!sink)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_acquired = FT_FALSE;
    if (file_sink_lock(sink, &lock_acquired) != 0)
        return (FT_ERR_INTERNAL);
    length = ft_strlen(message);
    total_written = 0;
    while (total_written < length)
    {
        write_result = su_write(sink->file_descriptor, message + total_written,
                length - total_written);
        if (write_result <= 0)
        {
            if (lock_acquired)
                file_sink_unlock(sink, lock_acquired);
            return (FT_ERR_IO);
        }
        total_written += static_cast<ft_size_t>(write_result);
    }
    if (lock_acquired)
        file_sink_unlock(sink, lock_acquired);
    return (FT_ERR_SUCCESS);
}

static int32_t log_set_file_report(int32_t return_value)
{
    return (return_value);
}

int32_t ft_log_set_file(const char *path, ft_size_t max_size)
{
    s_file_sink *sink;
    int32_t          file_descriptor;
    int32_t          prepare_status;

    if (!path)
        return (log_set_file_report(-1));
    file_descriptor = cmp_open(path, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (file_descriptor == -1)
        return (log_set_file_report(-1));
    sink = new(std::nothrow) s_file_sink;
    if (!sink)
    {
        (void)cmp_close(file_descriptor);
        return (log_set_file_report(-1));
    }
    sink->file_descriptor = file_descriptor;
    sink->path = path;
    sink->max_size = max_size;
    sink->retention_count = 1;
    sink->max_age_seconds = 0;
    if (sink->path.get_error() != FT_ERR_SUCCESS)
    {
        (void)cmp_close(file_descriptor);
        delete sink;
        return (log_set_file_report(-1));
    }
    prepare_status = file_sink_prepare_thread_safety(sink);
    if (prepare_status != FT_ERR_SUCCESS)
    {
        (void)cmp_close(file_descriptor);
        delete sink;
        return (log_set_file_report(-1));
    }
    if (ft_log_add_sink(ft_file_sink, sink) != 0)
    {
        (void)cmp_close(file_descriptor);
        file_sink_teardown_thread_safety(sink);
        delete sink;
        return (log_set_file_report(-1));
    }
    return (log_set_file_report(0));
}
