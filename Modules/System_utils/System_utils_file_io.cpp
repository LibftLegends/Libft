#include "system_utils.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <cerrno>
#include <atomic>
#include "../Basic/limits.hpp"

#ifdef LIBFT_TEST_BUILD
static std::atomic<t_su_write_syscall_hook> g_su_write_syscall_hook{ft_nullptr};

void    su_set_write_syscall_hook(t_su_write_syscall_hook hook)
{
    g_su_write_syscall_hook.store(hook, std::memory_order_relaxed);
    return ;
}

void    su_reset_write_syscall_hook(void)
{
    su_set_write_syscall_hook(ft_nullptr);
    return ;
}
#endif

static ft_bool su_retry_file_io(int32_t system_error, uint32_t &retry_attempts)
{
    constexpr uint32_t MAX_RETRIES = 10U;
    constexpr uint32_t RETRY_DELAY_MS = 500U;
    if (system_error == EINTR)
        return (FT_TRUE);
    if (system_error != EAGAIN && system_error != EWOULDBLOCK)
        return (FT_FALSE);
    if (retry_attempts >= MAX_RETRIES)
        return (FT_FALSE);
    retry_attempts += 1U;
    (void)cmp_thread_sleep(RETRY_DELAY_MS);
    return (FT_TRUE);
}

int64_t su_read(int32_t file_descriptor, void *buffer, ft_size_t count)
{
    uint32_t retry_attempts = 0U;
    while (1)
    {
        int64_t bytes_read_value;
        int32_t system_error;
        const int32_t error_code = cmp_read(file_descriptor, buffer, count,
            &bytes_read_value, &system_error);
        if (error_code == FT_ERR_SUCCESS)
            return (bytes_read_value);
        if (su_retry_file_io(system_error, retry_attempts) == FT_FALSE)
            return (-1);
    }
}

static int64_t su_write_internal(int32_t file_descriptor,
    const void *buffer, ft_size_t count, int32_t *error_code_out)
{
    if (error_code_out != ft_nullptr)
        *error_code_out = FT_ERR_SUCCESS;
    if (count == 0U)
        return (0);
    if (buffer == ft_nullptr)
    {
        if (error_code_out != ft_nullptr)
            *error_code_out = FT_ERR_INVALID_ARGUMENT;
        return (-1);
    }
    if (count > static_cast<ft_size_t>(INT64_MAX))
    {
        if (error_code_out != ft_nullptr)
            *error_code_out = FT_ERR_OUT_OF_RANGE;
        return (-1);
    }
    const char *byte_buffer = static_cast<const char *>(buffer);
    ft_size_t total_written = 0U;
    uint32_t retry_attempts = 0U;
#ifdef LIBFT_TEST_BUILD
    const t_su_write_syscall_hook write_function =
        g_su_write_syscall_hook.load(std::memory_order_relaxed);
#endif
    while (total_written < count)
    {
        const ft_size_t remaining = count - total_written;
        int64_t write_result = 0;
        int32_t system_error = 0;
        int32_t error_code = FT_ERR_SUCCESS;
#ifdef LIBFT_TEST_BUILD
        if (su_test_take_write_interrupt() == FT_TRUE)
        {
            errno = EINTR;
            system_error = EINTR;
            error_code = cmp_map_system_error_to_ft(system_error);
        }
        else if (write_function != ft_nullptr)
        {
            errno = 0;
            write_result = write_function(file_descriptor, byte_buffer + total_written, remaining);
            if (write_result < 0)
            {
                system_error = errno;
                error_code = cmp_map_system_error_to_ft(system_error);
                if (error_code == FT_ERR_SUCCESS)
                    error_code = FT_ERR_IO;
            }
        }
        else
#endif
        {
            error_code = cmp_write(file_descriptor, byte_buffer + total_written,
                remaining, &write_result, &system_error);
        }
        if (error_code == FT_ERR_SUCCESS && write_result > 0
            && static_cast<ft_size_t>(write_result) <= remaining)
        {
            total_written += static_cast<ft_size_t>(write_result);
            continue ;
        }
        if (su_retry_file_io(system_error, retry_attempts) == FT_TRUE)
            continue ;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        if (error_code_out != ft_nullptr)
            *error_code_out = error_code;
        return (-1);
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
    return (su_write_internal(file_descriptor, buffer, count, error_code_out));
}

off_t su_lseek(int32_t file_descriptor, off_t offset, int32_t whence)
{
    return (cmp_lseek(file_descriptor, offset, whence));
}

int32_t su_close(int32_t file_descriptor)
{
    return (cmp_close(file_descriptor));
}
