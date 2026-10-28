#include "system_utils.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <cerrno>
#include <cstddef>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

#ifdef LIBFT_TEST_BUILD
static t_su_write_syscall_hook    g_su_write_syscall_hook = ft_nullptr;

void    su_set_write_syscall_hook(t_su_write_syscall_hook hook)
{
    if (hook != ft_nullptr)
    {
        g_su_write_syscall_hook = hook;
        return ;
    }
    g_su_write_syscall_hook = ft_nullptr;
    return ;
}

void    su_reset_write_syscall_hook(void)
{
    g_su_write_syscall_hook = ft_nullptr;
    return ;
}
#endif

int64_t su_read(int32_t file_descriptor, void *buffer, ft_size_t count)
{
    int32_t retry_attempts;
    int64_t bytes_read_value;

    retry_attempts = 0;
    while (1)
    {
        int32_t error_code;

        bytes_read_value = 0;
        error_code = cmp_read(file_descriptor, buffer, count, &bytes_read_value);
        if (error_code == FT_ERR_SUCCESS)
            return (bytes_read_value);
        const int32_t max_retries = 10;
        const int32_t retry_delay_ms = 500;
        if (error_code == cmp_map_system_error_to_ft(EINTR))
            continue ;
        else if (error_code == cmp_map_system_error_to_ft(EAGAIN)
            || error_code == cmp_map_system_error_to_ft(EWOULDBLOCK))
        {
                if (retry_attempts < static_cast<int32_t>(max_retries))
                {
                    retry_attempts++;
                    (void)cmp_thread_sleep(static_cast<uint32_t>(retry_delay_ms));
                    continue ;
                }
            return (-1);
        }
        return (-1);
    }
}

static int64_t su_write_internal(int32_t file_descriptor,
    const void *buffer, ft_size_t count, int32_t *error_code_out)
{
    ft_size_t total_written;
    int32_t retry_attempts;
    const char *byte_buffer;
    int64_t bytes_written_value;
#ifdef LIBFT_TEST_BUILD
    t_su_write_syscall_hook write_function;
#endif

    total_written = 0;
    retry_attempts = 0;
    byte_buffer = static_cast<const char*>(buffer);
#ifdef LIBFT_TEST_BUILD
    write_function = g_su_write_syscall_hook;
#endif
    if (error_code_out != ft_nullptr)
        *error_code_out = FT_ERR_SUCCESS;
    while (total_written < count)
    {
        int64_t write_result;
        int32_t error_code;

        error_code = FT_ERR_SUCCESS;
#ifdef LIBFT_TEST_BUILD
        if (su_test_take_write_interrupt() == FT_TRUE)
        {
            errno = EINTR;
            error_code = cmp_map_system_error_to_ft(errno);
            write_result = -1;
        }
        else if (write_function != ft_nullptr)
        {
            write_result = write_function(file_descriptor,
                byte_buffer + total_written, count - total_written);
            if (write_result < 0)
            {
                error_code = cmp_map_system_error_to_ft(errno);
                if (error_code == FT_ERR_SUCCESS)
                    error_code = FT_ERR_IO;
            }
        }
        else
#endif
        {
            bytes_written_value = 0;
            error_code = cmp_write(file_descriptor, byte_buffer + total_written,
                count - total_written, &bytes_written_value);
            if (error_code == FT_ERR_SUCCESS)
                write_result = bytes_written_value;
            else
                write_result = -1;
        }
        if (write_result > 0)
            total_written += static_cast<ft_size_t>(write_result);
        else
        {
            if (write_result == 0)
            {
                if (error_code_out != ft_nullptr)
                    *error_code_out = FT_ERR_IO;
                return (-1);
            }
            const int32_t max_retries = 10;
            const int32_t retry_delay_ms = 500;
            if (error_code == cmp_map_system_error_to_ft(EINTR))
                continue ;
            else if (error_code == cmp_map_system_error_to_ft(EAGAIN)
                || error_code == cmp_map_system_error_to_ft(EWOULDBLOCK))
            {
                if (retry_attempts < static_cast<int32_t>(max_retries))
                {
                    retry_attempts++;
                    (void)cmp_thread_sleep(static_cast<uint32_t>(retry_delay_ms));
                    continue ;
                }
                if (error_code_out != ft_nullptr)
                    *error_code_out = error_code;
                return (-1);
            }
            if (error_code_out != ft_nullptr)
                *error_code_out = error_code;
            return (-1);
        }
    }
    return (static_cast<int64_t>(total_written));
}

int64_t su_write(int32_t file_descriptor, const void *buffer, ft_size_t count)
{
    return (su_write_internal(file_descriptor, buffer, count, ft_nullptr));
}

int64_t su_write_with_error(int32_t file_descriptor, const void *buffer,
    ft_size_t count, int32_t *error_code_out)
{
    if (error_code_out == ft_nullptr)
        return (-1);
    *error_code_out = FT_ERR_SUCCESS;
    return (su_write_internal(file_descriptor, buffer, count, error_code_out));
}

off_t su_lseek(int32_t file_descriptor, off_t offset, int32_t whence)
{
    return (cmp_lseek(file_descriptor, offset, whence));
}

int32_t su_close(int32_t file_descriptor)
{
    int32_t close_result;

    close_result = cmp_close(file_descriptor);
    if (close_result != 0)
        return (close_result);
    return (0);
}
