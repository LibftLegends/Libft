#include "system_utils.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

#ifdef LIBFT_TEST_BUILD
static ft_bool g_su_service_force_no_fork = FT_FALSE;
#endif
static t_su_service_signal_handler g_su_service_signal_handler = ft_nullptr;
static void *g_su_service_signal_context = ft_nullptr;

static void su_service_signal_dispatch(int32_t signal_number,
    void *user_context)
{
    (void)user_context;
    if (g_su_service_signal_handler == ft_nullptr)
        return ;
    g_su_service_signal_handler(signal_number,
        g_su_service_signal_context);
    return ;
}

#ifdef LIBFT_TEST_BUILD
void su_service_force_no_fork(ft_bool enable)
{
    g_su_service_force_no_fork = enable;
    return ;
}
#endif

static int32_t su_service_redirect_standard_streams(int32_t *error_code)
{
    const char  *null_path;
    FILE        *stream;

    null_path = cmp_service_null_device_path();
    stream = std::freopen(null_path, "r", stdin);
    if (stream == ft_nullptr)
    {
        int32_t local_error_code;

        if (errno != 0)
            local_error_code = cmp_map_system_error_to_ft(errno);
        else
            local_error_code = FT_ERR_IO;
        if (error_code != ft_nullptr)
            *error_code = local_error_code;
        return (-1);
    }
    stream = std::freopen(null_path, "w", stdout);
    if (stream == ft_nullptr)
    {
        int32_t local_error_code;

        if (errno != 0)
            local_error_code = cmp_map_system_error_to_ft(errno);
        else
            local_error_code = FT_ERR_IO;
        if (error_code != ft_nullptr)
            *error_code = local_error_code;
        return (-1);
    }
    stream = std::freopen(null_path, "w", stderr);
    if (stream == ft_nullptr)
    {
        int32_t local_error_code;

        if (errno != 0)
            local_error_code = cmp_map_system_error_to_ft(errno);
        else
            local_error_code = FT_ERR_IO;
        if (error_code != ft_nullptr)
            *error_code = local_error_code;
        return (-1);
    }
    if (error_code != ft_nullptr)
        *error_code = FT_ERR_SUCCESS;
    return (0);
}

static int32_t su_service_write_pid_file(const char *pid_file_path,
    int32_t *error_code)
{
    char    buffer[64];
    int32_t file_descriptor;
    ft_size_t  length;
    int64_t bytes_written;
    int32_t close_error;
    int32_t pid_error;

    if (pid_file_path == ft_nullptr)
    {
        if (error_code != ft_nullptr)
            *error_code = FT_ERR_SUCCESS;
        return (0);
    }
    std::memset(buffer, 0, sizeof(buffer));
    pid_error = cmp_service_format_pid_line(buffer, sizeof(buffer), &length);
    if (pid_error != FT_ERR_SUCCESS)
    {
        if (error_code != ft_nullptr)
            *error_code = pid_error;
        return (-1);
    }
    file_descriptor = su_open(pid_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_descriptor < 0)
    {
        if (error_code != ft_nullptr)
            *error_code = FT_ERR_FILE_OPEN_FAILED;
        return (-1);
    }
    bytes_written = su_write(file_descriptor, buffer, length);
    if (bytes_written < 0 || static_cast<ft_size_t>(bytes_written) != length)
    {
        (void)su_close(file_descriptor);
        if (error_code != ft_nullptr)
            *error_code = FT_ERR_IO;
        return (-1);
    }
    close_error = su_close(file_descriptor);
    if (close_error != 0)
    {
        if (error_code != ft_nullptr)
            *error_code = FT_ERR_IO;
        return (-1);
    }
    if (error_code != ft_nullptr)
        *error_code = FT_ERR_SUCCESS;
    return (0);
}

int32_t su_service_daemonize(const char *working_directory,
    const char *pid_file_path, ft_bool redirect_standard_streams)
{
    int32_t detach_result;

#ifdef LIBFT_TEST_BUILD
    detach_result = cmp_service_detach_process(g_su_service_force_no_fork);
#else
    detach_result = cmp_service_detach_process(FT_FALSE);
#endif
    if (detach_result != 0)
        return (detach_result);
    if (cmp_service_set_working_directory(working_directory) != 0)
        return (-1);
    if (redirect_standard_streams == FT_TRUE)
    {
        if (su_service_redirect_standard_streams(ft_nullptr) != 0)
            return (-1);
    }
    if (su_service_write_pid_file(pid_file_path, ft_nullptr) != 0)
        return (-1);
    return (0);
}

int32_t su_service_install_signal_handlers(t_su_service_signal_handler handler,
    void *user_context)
{
    if (handler == ft_nullptr)
        return (-1);
    su_service_clear_signal_handlers();
    g_su_service_signal_handler = handler;
    g_su_service_signal_context = user_context;
    if (cmp_service_install_signal_handlers(su_service_signal_dispatch,
            user_context) != 0)
    {
        g_su_service_signal_handler = ft_nullptr;
        g_su_service_signal_context = ft_nullptr;
        return (-1);
    }
    return (0);
}

void su_service_clear_signal_handlers(void)
{
    cmp_service_clear_signal_handlers();
    g_su_service_signal_handler = ft_nullptr;
    g_su_service_signal_context = ft_nullptr;
    return ;
}
