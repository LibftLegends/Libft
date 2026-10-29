#include "compatebility_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../System_utils/system_utils.hpp"
#include <cstdlib>
#include <ctime>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <inttypes.h>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

const char *cmp_service_null_device_path(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return ("NUL");
#else
    return ("/dev/null");
#endif
}

int32_t cmp_service_format_pid_line(char *buffer, ft_size_t buffer_size,
    ft_size_t *length_out)
{
    if (!buffer || buffer_size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
#if defined(_WIN32) || defined(_WIN64)
    uint64_t pid_value = static_cast<uint64_t>(_getpid());
    int32_t formatted = std::snprintf(buffer, buffer_size, "%" PRIu64 "\n",
        pid_value);
#else
    pid_t pid_value = getpid();
    int32_t formatted = std::snprintf(buffer, buffer_size, "%" PRId64 "\n",
        static_cast<int64_t>(pid_value));
#endif
    if (formatted < 0 || static_cast<ft_size_t>(formatted) >= buffer_size)
        return (FT_ERR_IO);
    if (length_out != ft_nullptr)
        *length_out = static_cast<ft_size_t>(formatted);
    return (FT_ERR_SUCCESS);
}

#if defined(LIBFT_TEST_BUILD)
void cmp_set_force_unsetenv_result(int32_t result, int32_t errno_value);
void cmp_clear_force_unsetenv_result(void);
void cmp_set_force_unsetenv_windows_errors(int32_t last_error, int32_t socket_error);
void cmp_set_force_putenv_result(int32_t result, int32_t errno_value);
void cmp_clear_force_putenv_result(void);
void cmp_set_force_putenv_windows_error(int32_t last_error);
void cmp_set_force_cpu_count_success(uint32_t cpu_count);
void cmp_set_force_cpu_count_failure(int32_t errno_value);
void cmp_clear_force_cpu_count_result(void);
void cmp_set_force_total_memory_success(uint64_t memory_size);
void cmp_set_force_total_memory_failure(int32_t errno_value);
#if defined(_WIN32) || defined(_WIN64)
void cmp_set_force_total_memory_windows_failure(uint64_t last_error);
#endif
void cmp_clear_force_total_memory_result(void);
#endif

#if defined(_WIN32) || defined(_WIN64)
# include <winsock2.h>
# include <windows.h>
# include <sysinfoapi.h>
# include <process.h>
#else
# include <unistd.h>
# if defined(__APPLE__) && defined(__MACH__)
#  include <sys/types.h>
#  include <sys/sysctl.h>
# endif
#endif

#if defined(LIBFT_TEST_BUILD)
static int32_t global_force_unsetenv_enabled = 0;
static int32_t global_force_unsetenv_result = 0;
static int32_t global_force_unsetenv_errno_value = 0;
static int32_t global_force_putenv_enabled = 0;
static int32_t global_force_putenv_result = 0;
static int32_t global_force_putenv_errno_value = 0;
#if defined(_WIN32) || defined(_WIN64)
static int32_t global_force_unsetenv_last_error = 0;
static int32_t global_force_unsetenv_socket_error = 0;
static int32_t global_force_putenv_last_error = 0;
#endif
static int32_t global_force_cpu_count_enabled = 0;
static int32_t global_force_cpu_count_should_fail = 0;
static uint32_t global_force_cpu_count_value = 0;
static int32_t global_force_cpu_count_errno_value = 0;
static int32_t global_force_total_memory_enabled = 0;
static int32_t global_force_total_memory_should_fail = 0;
static uint64_t global_force_total_memory_value = 0;
static int32_t global_force_total_memory_errno_value = 0;
#if defined(_WIN32) || defined(_WIN64)
static uint64_t global_force_total_memory_last_error = 0;
#endif
#endif

#if defined(_WIN32) || defined(_WIN64)
static int32_t cmp_translate_windows_error(int32_t error_code)
{
    if (error_code == WSAEINTR)
        return (FT_ERR_INVALID_STATE);
    if (error_code == WSAEWOULDBLOCK)
        return (FT_ERR_INVALID_STATE);
    if (error_code == WSAEINPROGRESS)
        return (FT_ERR_INVALID_STATE);
    if (error_code == WSAEALREADY)
        return (FT_ERR_INVALID_STATE);
    if (error_code == WSAENOTSOCK)
        return (FT_ERR_INVALID_HANDLE);
    if (error_code == WSAEDESTADDRREQ)
        return (FT_ERR_INVALID_ARGUMENT);
    if (error_code == WSAEMSGSIZE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (error_code == WSAEPROTOTYPE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (error_code == WSAENOPROTOOPT)
        return (FT_ERR_INVALID_ARGUMENT);
    if (error_code == WSAEPROTONOSUPPORT)
        return (FT_ERR_UNSUPPORTED_TYPE);
    if (error_code == WSAESOCKTNOSUPPORT)
        return (FT_ERR_UNSUPPORTED_TYPE);
    if (error_code == WSAEOPNOTSUPP)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == WSAEPFNOSUPPORT)
        return (FT_ERR_UNSUPPORTED_TYPE);
    if (error_code == WSAEAFNOSUPPORT)
        return (FT_ERR_UNSUPPORTED_TYPE);
    if (error_code == WSAEADDRINUSE)
        return (FT_ERR_ALREADY_EXISTS);
    if (error_code == WSAEADDRNOTAVAIL)
        return (FT_ERR_NOT_FOUND);
    if (error_code == WSAENETDOWN)
        return (FT_ERR_IO);
    if (error_code == WSAENETUNREACH)
        return (FT_ERR_IO);
    if (error_code == WSAENETRESET)
        return (FT_ERR_IO);
    if (error_code == WSAECONNABORTED)
        return (FT_ERR_IO);
    if (error_code == WSAECONNRESET)
        return (FT_ERR_IO);
    if (error_code == WSAENOBUFS)
        return (FT_ERR_NO_MEMORY);
    if (error_code == WSAEISCONN)
        return (FT_ERR_INVALID_STATE);
    if (error_code == WSAENOTCONN)
        return (FT_ERR_INVALID_STATE);
    if (error_code == WSAESHUTDOWN)
        return (FT_ERR_INVALID_STATE);
    if (error_code == WSAETOOMANYREFS)
        return (FT_ERR_FULL);
    if (error_code == WSAETIMEDOUT)
        return (FT_ERR_IO);
    if (error_code == WSAECONNREFUSED)
        return (FT_ERR_SOCKET_CONNECT_FAILED);
    if (error_code == WSAEHOSTDOWN)
        return (FT_ERR_IO);
    if (error_code == WSAEHOSTUNREACH)
        return (FT_ERR_IO);
    if (error_code == WSAEPROCLIM)
        return (FT_ERR_FULL);
    if (error_code == WSAEUSERS)
        return (FT_ERR_FULL);
    if (error_code == WSAEDQUOT)
        return (FT_ERR_FULL);
    if (error_code == WSAESTALE)
        return (FT_ERR_IO);
    if (error_code == WSAEREMOTE)
        return (FT_ERR_IO);
    if (error_code == WSAEDISCON)
        return (FT_ERR_IO);
    if (error_code == WSASYSNOTREADY)
        return (FT_ERR_INVALID_STATE);
    if (error_code == WSAVERNOTSUPPORTED)
        return (FT_ERR_UNSUPPORTED_TYPE);
    if (error_code == WSANOTINITIALISED)
        return (FT_ERR_INVALID_STATE);
    if (error_code == WSAHOST_NOT_FOUND)
        return (FT_ERR_SOCKET_RESOLVE_NO_NAME);
    if (error_code == WSATRY_AGAIN)
        return (FT_ERR_SOCKET_RESOLVE_AGAIN);
    if (error_code == WSANO_RECOVERY)
        return (FT_ERR_SOCKET_RESOLVE_FAIL);
    if (error_code == WSANO_DATA)
        return (FT_ERR_SOCKET_RESOLVE_NO_NAME);
    if (error_code == ERROR_FILE_NOT_FOUND)
        return (FT_ERR_NOT_FOUND);
    if (error_code == ERROR_PATH_NOT_FOUND)
        return (FT_ERR_NOT_FOUND);
#if defined(ERROR_FILENAME_EXCED_RANGE)
    if (error_code == ERROR_FILENAME_EXCED_RANGE)
        return (FT_ERR_OUT_OF_RANGE);
#endif
    if (error_code == EACCES)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == EPERM)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == EBADF)
        return (FT_ERR_INVALID_HANDLE);
    if (error_code == ERROR_ACCESS_DENIED)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == ERROR_SHARING_VIOLATION)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == ERROR_LOCK_VIOLATION)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == ERROR_ALREADY_EXISTS)
        return (FT_ERR_ALREADY_EXISTS);
    if (error_code == ERROR_FILE_EXISTS)
        return (FT_ERR_ALREADY_EXISTS);
    if (error_code == ERROR_INVALID_HANDLE)
        return (FT_ERR_INVALID_HANDLE);
    if (error_code == ERROR_NOT_ENOUGH_MEMORY)
        return (FT_ERR_NO_MEMORY);
    if (error_code == ERROR_OUTOFMEMORY)
        return (FT_ERR_NO_MEMORY);
    if (error_code == ERROR_DISK_FULL)
        return (FT_ERR_FULL);
#if defined(ERROR_TOO_MANY_OPEN_FILES)
    if (error_code == ERROR_TOO_MANY_OPEN_FILES)
        return (FT_ERR_FULL);
#endif
#if defined(ERROR_NO_SYSTEM_RESOURCES)
    if (error_code == ERROR_NO_SYSTEM_RESOURCES)
        return (FT_ERR_FULL);
#endif
    if (error_code == ERROR_HANDLE_EOF)
        return (FT_ERR_END_OF_FILE);
    if (error_code == ERROR_INVALID_PARAMETER)
        return (FT_ERR_INVALID_ARGUMENT);
    if (error_code == ERROR_INVALID_NAME)
        return (FT_ERR_INVALID_PATH);
#if defined(ERROR_BAD_PATHNAME)
    if (error_code == ERROR_BAD_PATHNAME)
        return (FT_ERR_INVALID_PATH);
#endif
    if (error_code == EINVAL)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == ERROR_BROKEN_PIPE)
        return (FT_ERR_IO);
    return (static_cast<int32_t>(error_code) + ERRNO_OFFSET);
}
#else
static int32_t cmp_translate_posix_error(int32_t error_code)
{
    if (error_code == EINTR)
        return (FT_ERR_INVALID_STATE);
    if (error_code == EAGAIN)
        return (FT_ERR_INVALID_STATE);
    if (error_code == EWOULDBLOCK)
        return (FT_ERR_INVALID_STATE);
    if (error_code == EINPROGRESS)
        return (FT_ERR_INVALID_STATE);
    if (error_code == EALREADY)
        return (FT_ERR_INVALID_STATE);
    if (error_code == ENOENT)
        return (FT_ERR_NOT_FOUND);
    if (error_code == ENOTDIR)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == EISDIR)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == EACCES)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == EPERM)
        return (FT_ERR_INVALID_OPERATION);
    if (error_code == EINVAL)
        return (FT_ERR_INVALID_OPERATION);
#if defined(EFAULT)
    if (error_code == EFAULT)
        return (FT_ERR_INVALID_POINTER);
#endif
#if defined(E2BIG)
    if (error_code == E2BIG)
        return (FT_ERR_INVALID_ARGUMENT);
#endif
    if (error_code == EDOM)
        return (FT_ERR_INVALID_ARGUMENT);
    if (error_code == ESRCH)
        return (FT_ERR_NOT_FOUND);
    if (error_code == EEXIST)
        return (FT_ERR_ALREADY_EXISTS);
    if (error_code == EBUSY)
        return (FT_ERR_INVALID_STATE);
    if (error_code == ECHILD)
        return (FT_ERR_NOT_FOUND);
#if defined(EDEADLK)
    if (error_code == EDEADLK)
        return (FT_ERR_INVALID_STATE);
#endif
    if (error_code == EADDRINUSE)
        return (FT_ERR_ALREADY_EXISTS);
    if (error_code == EADDRNOTAVAIL)
        return (FT_ERR_NOT_FOUND);
    if (error_code == EBADF)
        return (FT_ERR_INVALID_HANDLE);
#if defined(ENODEV)
    if (error_code == ENODEV)
        return (FT_ERR_NOT_FOUND);
#endif
#if defined(ENXIO)
    if (error_code == ENXIO)
        return (FT_ERR_NOT_FOUND);
#endif
    if (error_code == ENOENT)
        return (FT_ERR_NOT_FOUND);
#if defined(ENFILE)
    if (error_code == ENFILE)
        return (FT_ERR_FULL);
#endif
    if (error_code == EMFILE)
        return (FT_ERR_FULL);
#if defined(EFBIG)
    if (error_code == EFBIG)
        return (FT_ERR_FULL);
#endif
#if defined(ENOLCK)
    if (error_code == ENOLCK)
        return (FT_ERR_INVALID_STATE);
#endif
    if (error_code == ENOTSOCK)
        return (FT_ERR_INVALID_HANDLE);
#if defined(ENOTTY)
    if (error_code == ENOTTY)
        return (FT_ERR_INVALID_OPERATION);
#endif
#if defined(ENOTBLK)
    if (error_code == ENOTBLK)
        return (FT_ERR_INVALID_OPERATION);
#endif
#if defined(ESPIPE)
    if (error_code == ESPIPE)
        return (FT_ERR_INVALID_OPERATION);
#endif
    if (error_code == EDESTADDRREQ)
        return (FT_ERR_INVALID_ARGUMENT);
    if (error_code == EMSGSIZE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (error_code == EPROTOTYPE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (error_code == ENOPROTOOPT)
        return (FT_ERR_INVALID_ARGUMENT);
    if (error_code == EPROTONOSUPPORT)
        return (FT_ERR_UNSUPPORTED_TYPE);
    if (error_code == ESOCKTNOSUPPORT)
        return (FT_ERR_UNSUPPORTED_TYPE);
    if (error_code == EOPNOTSUPP)
        return (FT_ERR_INVALID_OPERATION);
#if defined(ENOTSUP)
    if (error_code == ENOTSUP)
        return (FT_ERR_INVALID_OPERATION);
#endif
    if (error_code == EPFNOSUPPORT)
        return (FT_ERR_UNSUPPORTED_TYPE);
    if (error_code == EAFNOSUPPORT)
        return (FT_ERR_UNSUPPORTED_TYPE);
#if defined(ENAMETOOLONG)
    if (error_code == ENAMETOOLONG)
        return (FT_ERR_OUT_OF_RANGE);
#endif
#if defined(ELOOP)
    if (error_code == ELOOP)
        return (FT_ERR_INVALID_OPERATION);
#endif
    if (error_code == ENOBUFS)
        return (FT_ERR_NO_MEMORY);
    if (error_code == ENOMEM)
        return (FT_ERR_NO_MEMORY);
    if (error_code == EIO)
        return (FT_ERR_IO);
#if defined(ENODATA)
    if (error_code == ENODATA)
        return (FT_ERR_IO);
#endif
#if defined(ETIME)
    if (error_code == ETIME)
        return (FT_ERR_IO);
#endif
#if defined(EBADMSG)
    if (error_code == EBADMSG)
        return (FT_ERR_IO);
#endif
#if defined(EMULTIHOP)
    if (error_code == EMULTIHOP)
        return (FT_ERR_IO);
#endif
#if defined(EPROTO)
    if (error_code == EPROTO)
        return (FT_ERR_IO);
#endif
#if defined(EREMOTEIO)
    if (error_code == EREMOTEIO)
        return (FT_ERR_IO);
#endif
#if defined(ENONET)
    if (error_code == ENONET)
        return (FT_ERR_IO);
#endif
    if (error_code == ENOSPC)
        return (FT_ERR_FULL);
#if defined(EOVERFLOW)
    if (error_code == EOVERFLOW)
        return (FT_ERR_OUT_OF_RANGE);
#endif
    if (error_code == ERANGE)
        return (FT_ERR_OUT_OF_RANGE);
#if defined(EILSEQ)
    if (error_code == EILSEQ)
        return (FT_ERR_INVALID_ARGUMENT);
#endif
#if defined(ECANCELED)
    if (error_code == ECANCELED)
        return (FT_ERR_TERMINATED);
#endif
#if defined(ENOSYS)
    if (error_code == ENOSYS)
        return (FT_ERR_UNSUPPORTED_TYPE);
#endif
#if defined(ENOLINK)
    if (error_code == ENOLINK)
        return (FT_ERR_INVALID_OPERATION);
#endif
#if defined(ENOSTR)
    if (error_code == ENOSTR)
        return (FT_ERR_INVALID_OPERATION);
#endif
#if defined(ENOSR)
    if (error_code == ENOSR)
        return (FT_ERR_INVALID_OPERATION);
#endif
#if defined(ENOTEMPTY)
    if (error_code == ENOTEMPTY)
        return (FT_ERR_INVALID_OPERATION);
#endif
#if defined(EROFS)
    if (error_code == EROFS)
        return (FT_ERR_INVALID_OPERATION);
#endif
#if defined(EXDEV)
    if (error_code == EXDEV)
        return (FT_ERR_INVALID_OPERATION);
#endif
#if defined(ETXTBSY)
    if (error_code == ETXTBSY)
        return (FT_ERR_INVALID_OPERATION);
#endif
#if defined(EUCLEAN)
    if (error_code == EUCLEAN)
        return (FT_ERR_INVALID_OPERATION);
#endif
#if defined(ENOTUNIQ)
    if (error_code == ENOTUNIQ)
        return (FT_ERR_ALREADY_EXISTS);
#endif
#if defined(EOWNERDEAD)
    if (error_code == EOWNERDEAD)
        return (FT_ERR_INVALID_STATE);
#endif
#if defined(ENOTRECOVERABLE)
    if (error_code == ENOTRECOVERABLE)
        return (FT_ERR_INVALID_STATE);
#endif
#if defined(ERFKILL)
    if (error_code == ERFKILL)
        return (FT_ERR_INVALID_STATE);
#endif
#if defined(EHWPOISON)
    if (error_code == EHWPOISON)
        return (FT_ERR_INVALID_STATE);
#endif
#if defined(EIDRM)
    if (error_code == EIDRM)
        return (FT_ERR_NOT_FOUND);
#endif
    if (error_code == ENETDOWN)
        return (FT_ERR_IO);
    if (error_code == ENETUNREACH)
        return (FT_ERR_IO);
    if (error_code == ENETRESET)
        return (FT_ERR_IO);
    if (error_code == ECONNABORTED)
        return (FT_ERR_IO);
    if (error_code == ECONNRESET)
        return (FT_ERR_IO);
    if (error_code == ETIMEDOUT)
        return (FT_ERR_IO);
    if (error_code == ECONNREFUSED)
        return (FT_ERR_SOCKET_CONNECT_FAILED);
    if (error_code == EHOSTDOWN)
        return (FT_ERR_IO);
    if (error_code == EHOSTUNREACH)
        return (FT_ERR_IO);
#if defined(EPROCLIM)
    if (error_code == EPROCLIM)
        return (FT_ERR_FULL);
#endif
#if defined(EUSERS)
    if (error_code == EUSERS)
        return (FT_ERR_FULL);
#endif
#if defined(EDQUOT)
    if (error_code == EDQUOT)
        return (FT_ERR_FULL);
#endif
#if defined(ESTALE)
    if (error_code == ESTALE)
        return (FT_ERR_IO);
#endif
#if defined(EREMOTE)
    if (error_code == EREMOTE)
        return (FT_ERR_IO);
#endif
    if (error_code == EISCONN)
        return (FT_ERR_INVALID_STATE);
    if (error_code == ENOTCONN)
        return (FT_ERR_INVALID_STATE);
    if (error_code == ESHUTDOWN)
        return (FT_ERR_INVALID_STATE);
    if (error_code == ETOOMANYREFS)
        return (FT_ERR_FULL);
    if (error_code == EPIPE)
        return (FT_ERR_IO);
    return (error_code + ERRNO_OFFSET);
}
#endif

int32_t cmp_map_system_error_to_ft(int32_t error_code)
{
    if (error_code == 0)
        return (FT_ERR_SUCCESS);
#if defined(_WIN32) || defined(_WIN64)
    return (cmp_translate_windows_error(error_code));
#else
    return (cmp_translate_posix_error(error_code));
#endif
}

int32_t cmp_decode_errno_offset_error(int32_t error_code)
{
    int32_t normalized_error;

    if (error_code < ERRNO_OFFSET)
    {
        return (error_code);
    }
    normalized_error = cmp_map_system_error_to_ft(error_code - ERRNO_OFFSET);
    return (normalized_error);
}

#if defined(LIBFT_TEST_BUILD)
void cmp_set_force_unsetenv_result(int32_t result, int32_t errno_value)
{
    global_force_unsetenv_enabled = 1;
    global_force_unsetenv_result = result;
    global_force_unsetenv_errno_value = errno_value;
#if defined(_WIN32) || defined(_WIN64)
    global_force_unsetenv_last_error = 0;
    global_force_unsetenv_socket_error = 0;
#endif
    return ;
}

void cmp_clear_force_unsetenv_result(void)
{
    global_force_unsetenv_enabled = 0;
    return ;
}

void cmp_set_force_putenv_result(int32_t result, int32_t errno_value)
{
    global_force_putenv_enabled = 1;
    global_force_putenv_result = result;
    global_force_putenv_errno_value = errno_value;
#if defined(_WIN32) || defined(_WIN64)
    global_force_putenv_last_error = 0;
#endif
    return ;
}

void cmp_clear_force_putenv_result(void)
{
    global_force_putenv_enabled = 0;
    return ;
}

void cmp_set_force_unsetenv_windows_errors(int32_t last_error, int32_t socket_error)
{
#if defined(_WIN32) || defined(_WIN64)
    global_force_unsetenv_last_error = last_error;
    global_force_unsetenv_socket_error = socket_error;
#else
    (void)last_error;
    (void)socket_error;
#endif
    return ;
}

void cmp_set_force_putenv_windows_error(int32_t last_error)
{
#if defined(_WIN32) || defined(_WIN64)
    global_force_putenv_last_error = last_error;
#else
    (void)last_error;
#endif
    return ;
}

void cmp_set_force_cpu_count_success(uint32_t cpu_count)
{
    global_force_cpu_count_enabled = 1;
    global_force_cpu_count_should_fail = 0;
    global_force_cpu_count_value = cpu_count;
    global_force_cpu_count_errno_value = 0;
    return ;
}

void cmp_set_force_cpu_count_failure(int32_t errno_value)
{
    global_force_cpu_count_enabled = 1;
    global_force_cpu_count_should_fail = 1;
    global_force_cpu_count_errno_value = errno_value;
    global_force_cpu_count_value = 0;
    return ;
}

void cmp_clear_force_cpu_count_result(void)
{
    global_force_cpu_count_enabled = 0;
    global_force_cpu_count_should_fail = 0;
    global_force_cpu_count_value = 0;
    global_force_cpu_count_errno_value = 0;
    return ;
}

void cmp_set_force_total_memory_success(uint64_t memory_size)
{
    global_force_total_memory_enabled = 1;
    global_force_total_memory_should_fail = 0;
    global_force_total_memory_value = memory_size;
    global_force_total_memory_errno_value = 0;
#if defined(_WIN32) || defined(_WIN64)
    global_force_total_memory_last_error = 0;
#endif
    return ;
}

void cmp_set_force_total_memory_failure(int32_t errno_value)
{
    global_force_total_memory_enabled = 1;
    global_force_total_memory_should_fail = 1;
    global_force_total_memory_errno_value = errno_value;
    global_force_total_memory_value = 0;
#if defined(_WIN32) || defined(_WIN64)
    global_force_total_memory_last_error = 0;
#endif
    return ;
}

#if defined(_WIN32) || defined(_WIN64)
void cmp_set_force_total_memory_windows_failure(uint64_t last_error)
{
    global_force_total_memory_enabled = 1;
    global_force_total_memory_should_fail = 1;
    global_force_total_memory_last_error = last_error;
    global_force_total_memory_errno_value = 0;
    global_force_total_memory_value = 0;
    return ;
}
#endif

void cmp_clear_force_total_memory_result(void)
{
    global_force_total_memory_enabled = 0;
    global_force_total_memory_should_fail = 0;
    global_force_total_memory_value = 0;
    global_force_total_memory_errno_value = 0;
#if defined(_WIN32) || defined(_WIN64)
    global_force_total_memory_last_error = 0;
#endif
    return ;
}
#endif

int32_t cmp_secure_memzero(void *buffer, ft_size_t length)
{
    if (buffer == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    if (length == 0)
    {
        return (FT_ERR_SUCCESS);
    }
#if defined(_WIN32) || defined(_WIN64)
    SecureZeroMemory(buffer, length);
    return (FT_ERR_SUCCESS);
#else
    volatile unsigned char *volatile byte_pointer;

    byte_pointer = reinterpret_cast<volatile unsigned char *>(buffer);
    while (length > 0)
    {
        *byte_pointer = 0;
        byte_pointer = byte_pointer + 1;
        length = length - 1;
    }
    return (FT_ERR_SUCCESS);
#endif
}

static int32_t cmp_environment_map_last_error(void)
{
#if defined(_WIN32) || defined(_WIN64)
    int32_t system_error;

    system_error = static_cast<int32_t>(GetLastError());
    if (system_error != 0)
        return (cmp_map_system_error_to_ft(system_error));
    system_error = WSAGetLastError();
    if (system_error != 0)
        return (cmp_map_system_error_to_ft(system_error));
    if (errno != 0)
        return (cmp_map_system_error_to_ft(errno));
    return (FT_ERR_INVALID_ARGUMENT);
#else
    if (errno != 0)
        return (cmp_map_system_error_to_ft(errno));
    return (FT_ERR_INVALID_ARGUMENT);
#endif
}

int32_t cmp_setenv(const char *name, const char *value, int32_t overwrite)
{
    if (name == ft_nullptr || value == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (name[0] == '\0' || ft_strchr(name, '=') != ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
#if defined(_WIN32) || defined(_WIN64)
    if (!overwrite && getenv(name) != ft_nullptr)
    {
        return (FT_ERR_SUCCESS);
    }
    errno = 0;
    SetLastError(0);
    WSASetLastError(0);
    int32_t result = _putenv_s(name, value);
    if (result != 0)
    {
        if (errno == 0)
            errno = result;
        return (cmp_environment_map_last_error());
    }
    return (FT_ERR_SUCCESS);
#else
    errno = 0;
    int32_t result = setenv(name, value, overwrite);
    if (result != 0)
    {
        return (cmp_environment_map_last_error());
    }
    return (FT_ERR_SUCCESS);
#endif
}

int32_t cmp_unsetenv(const char *name)
{
    if (name == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
#if defined(_WIN32) || defined(_WIN64)
#if defined(LIBFT_TEST_BUILD)
    if (global_force_unsetenv_enabled != 0)
    {
        errno = global_force_unsetenv_errno_value;
        SetLastError(global_force_unsetenv_last_error);
        WSASetLastError(global_force_unsetenv_socket_error);
        return (global_force_unsetenv_result);
    }
#endif
    errno = 0;
    SetLastError(0);
    WSASetLastError(0);
    int32_t result = _putenv_s(name, "");
    if (result != 0)
    {
        if (errno == 0)
            errno = result;
        return (-1);
    }
    return (FT_ERR_SUCCESS);
#else
#if defined(LIBFT_TEST_BUILD)
    if (global_force_unsetenv_enabled != 0)
    {
        errno = global_force_unsetenv_errno_value;
        return (global_force_unsetenv_result);
    }
#endif
    errno = 0;
    int32_t result = unsetenv(name);
    if (result != 0)
    {
        return (result);
    }
    return (FT_ERR_SUCCESS);
#endif
}

int32_t cmp_putenv(char *string)
{
    int32_t result;

    if (string == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
#if defined(LIBFT_TEST_BUILD)
    if (global_force_putenv_enabled != 0)
    {
        errno = global_force_putenv_errno_value;
#if defined(_WIN32) || defined(_WIN64)
        SetLastError(global_force_putenv_last_error);
        WSASetLastError(0);
#endif
        result = global_force_putenv_result;
        if (result != 0)
            return (cmp_environment_map_last_error());
        return (FT_ERR_SUCCESS);
    }
#endif
#if defined(_WIN32) || defined(_WIN64)
    SetLastError(0);
    WSASetLastError(0);
    result = _putenv(string);
    if (result != 0)
    {
        return (cmp_environment_map_last_error());
    }
    return (FT_ERR_SUCCESS);
#else
    result = putenv(string);
    if (result != 0)
    {
        return (cmp_environment_map_last_error());
    }
    return (FT_ERR_SUCCESS);
#endif
}

char **cmp_get_environ_entries(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return (_environ);
#else
    extern char **environ;
    return (environ);
#endif
}

const char *cmp_system_strerror(int32_t error_code)
{
#if defined(_WIN32) || defined(_WIN64)
    static char message_buffer[512];
    DWORD system_error;
    DWORD format_result;

    if (error_code <= ERRNO_OFFSET)
        return (NULL);
    system_error = static_cast<DWORD>(error_code - ERRNO_OFFSET);
    format_result = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        system_error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        message_buffer,
        static_cast<DWORD>(sizeof(message_buffer)),
        NULL);
    if (format_result != 0)
    {
        while (format_result > 0)
        {
            char current_character;

            current_character = message_buffer[format_result - 1];
            if (current_character != '\r' && current_character != '\n'
                && current_character != '\t' && current_character != ' '
                && current_character != '\0')
                break ;
            message_buffer[format_result - 1] = '\0';
            format_result--;
        }
        if (format_result != 0)
            return (message_buffer);
    }
    return (strerror(static_cast<int32_t>(system_error)));
#else
    if (error_code > ERRNO_OFFSET)
    {
        int32_t standard_errno;

        standard_errno = error_code - ERRNO_OFFSET;
        return (strerror(standard_errno));
    }
    return (NULL);
#endif
}

[[maybe_unused]] static char *cmp_join_two_c_strings(const char *string_left, const char *string_right)
{
    ft_size_t left_length;
    ft_size_t right_length;
    ft_size_t total_length;
    ft_size_t index;
    char *output;

    if (string_left == ft_nullptr || string_right == ft_nullptr)
        return (ft_nullptr);
    left_length = ft_strlen_size_t(string_left);
    right_length = ft_strlen_size_t(string_right);
    total_length = left_length + right_length;
    output = static_cast<char *>(cma_malloc(total_length + 1));
    if (output == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < left_length)
    {
        output[index] = string_left[index];
        index++;
    }
    index = 0;
    while (index < right_length)
    {
        output[left_length + index] = string_right[index];
        index++;
    }
    output[total_length] = '\0';
    return (output);
}

char *cmp_get_home_directory(void)
{
#if defined(_WIN32) || defined(_WIN64)
    char *home;
    char *home_drive;
    char *home_path;
    char *combined_home;

    home = su_getenv("USERPROFILE");
    if (home != ft_nullptr)
    {
        return (home);
    }
    home_drive = su_getenv("HOMEDRIVE");
    home_path = su_getenv("HOMEPATH");
    if (home_drive == ft_nullptr || home_path == ft_nullptr)
    {
        return (ft_nullptr);
    }
    combined_home = cmp_join_two_c_strings(home_drive, home_path);
    if (combined_home == ft_nullptr)
    {
        return (ft_nullptr);
    }
    return (combined_home);
#else
    char *home;

    home = su_getenv("HOME");
    if (home == ft_nullptr)
    {
        return (ft_nullptr);
    }
    return (home);
#endif
}

uint32_t cmp_get_cpu_count(void)
{
#if defined(LIBFT_TEST_BUILD)
    if (global_force_cpu_count_enabled != 0)
    {
        if (global_force_cpu_count_should_fail != 0)
            return (0);
        return (global_force_cpu_count_value);
    }
#endif
#if defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO system_info;

    GetSystemInfo(&system_info);
    return (system_info.dwNumberOfProcessors);
#elif defined(__APPLE__) && defined(__MACH__)
    int32_t cpu_count;
    size_t size;

    size = sizeof(cpu_count);
    errno = 0;
    if (sysctlbyname("hw.ncpu", &cpu_count, &size, ft_nullptr, 0) != 0)
        return (0);
    return (static_cast<uint32_t>(cpu_count));
#else
    int64_t cpu_count;

    errno = 0;
    cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 0)
        return (0);
    return (static_cast<uint32_t>(cpu_count));
#endif
}

int32_t cmp_get_total_memory(uint64_t *total_memory)
{
    int32_t error_code;

    if (total_memory == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
#if defined(LIBFT_TEST_BUILD)
    if (global_force_total_memory_enabled != 0)
    {
#if defined(_WIN32) || defined(_WIN64)
        if (global_force_total_memory_should_fail != 0)
#else
        if (global_force_total_memory_should_fail != 0)
#endif
        {
#if defined(_WIN32) || defined(_WIN64)
            if (global_force_total_memory_last_error != 0)
                error_code = cmp_map_system_error_to_ft(static_cast<int32_t>(global_force_total_memory_last_error));
            else if (global_force_total_memory_errno_value != 0)
                error_code = cmp_map_system_error_to_ft(global_force_total_memory_errno_value);
            else
                error_code = FT_ERR_TERMINATED;
#else
            if (global_force_total_memory_errno_value != 0)
                error_code = cmp_map_system_error_to_ft(global_force_total_memory_errno_value);
            else
                error_code = FT_ERR_TERMINATED;
#endif
            *total_memory = 0;
            return (error_code);
        }
        *total_memory = global_force_total_memory_value;
        return (FT_ERR_SUCCESS);
    }
#endif
#if defined(_WIN32) || defined(_WIN64)
    MEMORYSTATUSEX memory_status;

    memory_status.dwLength = sizeof(memory_status);
    if (GlobalMemoryStatusEx(&memory_status) == 0)
    {
        DWORD last_error;

        last_error = GetLastError();
        if (last_error != 0)
            error_code = cmp_map_system_error_to_ft(static_cast<int32_t>(last_error));
        else
            error_code = FT_ERR_TERMINATED;
        *total_memory = 0;
        return (error_code);
    }
    *total_memory = memory_status.ullTotalPhys;
    return (FT_ERR_SUCCESS);
#elif defined(__APPLE__) && defined(__MACH__)
    uint64_t memory_size;
    size_t size;

    size = sizeof(memory_size);
    errno = 0;
    if (sysctlbyname("hw.memsize", &memory_size, &size, ft_nullptr, 0) != 0)
    {
        if (errno != 0)
            error_code = cmp_map_system_error_to_ft(errno);
        else
            error_code = FT_ERR_TERMINATED;
        *total_memory = 0;
        return (error_code);
    }
    *total_memory = memory_size;
    return (FT_ERR_SUCCESS);
#else
    int64_t pages;
    int64_t page_size;

    errno = 0;
    pages = sysconf(_SC_PHYS_PAGES);
    if (pages < 0)
    {
        if (errno != 0)
            error_code = cmp_map_system_error_to_ft(errno);
        else
            error_code = FT_ERR_TERMINATED;
        *total_memory = 0;
        return (error_code);
    }
    errno = 0;
    page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size < 0)
    {
        if (errno != 0)
            error_code = cmp_map_system_error_to_ft(errno);
        else
            error_code = FT_ERR_TERMINATED;
        *total_memory = 0;
        return (error_code);
    }
    *total_memory = static_cast<uint64_t>(pages) *
        static_cast<uint64_t>(page_size);
    return (FT_ERR_SUCCESS);
#endif
}

std::time_t cmp_timegm(std::tm *time_pointer)
{
    std::time_t conversion_result;

    if (time_pointer == ft_nullptr)
    {
        return (static_cast<std::time_t>(-1));
    }
    errno = 0;
#if defined(_WIN32) || defined(_WIN64)
    conversion_result = _mkgmtime(time_pointer);
#else
    conversion_result = ::timegm(time_pointer);
#endif
    return (conversion_result);
}
