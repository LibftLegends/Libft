#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "compatebility_internal.hpp"
#include <cstdint>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <wincrypt.h>

#ifdef LIBFT_TEST_BUILD
static thread_local DWORD g_cmp_rng_windows_acquire_error = 0U;
static thread_local DWORD g_cmp_rng_windows_generate_error = 0U;
static thread_local DWORD g_cmp_rng_windows_release_error = 0U;

void cmp_force_rng_windows_acquire_failure(uint32_t last_error)
{
    g_cmp_rng_windows_acquire_error = static_cast<DWORD>(last_error);
    return ;
}

void cmp_force_rng_windows_generate_failure(uint32_t last_error)
{
    g_cmp_rng_windows_generate_error = static_cast<DWORD>(last_error);
    return ;
}

void cmp_force_rng_windows_release_failure(uint32_t last_error)
{
    g_cmp_rng_windows_release_error = static_cast<DWORD>(last_error);
    return ;
}

void cmp_clear_force_rng_windows_failures(void)
{
    g_cmp_rng_windows_acquire_error = 0U;
    g_cmp_rng_windows_generate_error = 0U;
    g_cmp_rng_windows_release_error = 0U;
    return ;
}
#endif

static int32_t cmp_rng_windows_error(DWORD last_error)
{
    if (last_error != 0)
    {
        return (cmp_map_system_error_to_ft(static_cast<int32_t>(last_error)));
    }
    return (FT_ERR_INVALID_ARGUMENT);
}

int32_t cmp_rng_secure_bytes(unsigned char *buffer, ft_size_t length)
{
    HCRYPTPROV crypt_provider = 0;
    int32_t error_code;

    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (length > static_cast<ft_size_t>(UINT32_MAX))
        return (FT_ERR_OUT_OF_RANGE);
#ifdef LIBFT_TEST_BUILD
    if (g_cmp_rng_windows_acquire_error != 0U)
    {
        DWORD forced_error = g_cmp_rng_windows_acquire_error;

        g_cmp_rng_windows_acquire_error = 0U;
        return (cmp_rng_windows_error(forced_error));
    }
#endif
    if (CryptAcquireContext(&crypt_provider, ft_nullptr, ft_nullptr,
            PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) == 0)
    {
        error_code = cmp_rng_windows_error(GetLastError());
        return (error_code);
    }
#ifdef LIBFT_TEST_BUILD
    if (g_cmp_rng_windows_generate_error != 0U)
    {
        DWORD forced_error = g_cmp_rng_windows_generate_error;

        g_cmp_rng_windows_generate_error = 0U;
        error_code = cmp_rng_windows_error(forced_error);
        CryptReleaseContext(crypt_provider, 0);
        return (error_code);
    }
#endif
    if (CryptGenRandom(crypt_provider, static_cast<DWORD>(length), buffer) == 0)
    {
        error_code = cmp_rng_windows_error(GetLastError());
        CryptReleaseContext(crypt_provider, 0);
        return (error_code);
    }
    if (CryptReleaseContext(crypt_provider, 0) == 0)
    {
        error_code = cmp_rng_windows_error(GetLastError());
        return (error_code);
    }
#ifdef LIBFT_TEST_BUILD
    if (g_cmp_rng_windows_release_error != 0U)
    {
        DWORD forced_error = g_cmp_rng_windows_release_error;

        g_cmp_rng_windows_release_error = 0U;
        return (cmp_rng_windows_error(forced_error));
    }
#endif
    return (FT_ERR_SUCCESS);
}

#else
# include <unistd.h>
# include <fcntl.h>
# include <cerrno>
# include <atomic>

#ifdef LIBFT_TEST_BUILD
void cmp_force_rng_open_failure(int32_t error_code);
void cmp_force_rng_read_failure(int32_t error_code);
void cmp_force_rng_read_eof(void);
void cmp_force_rng_close_failure(int32_t error_code);
void cmp_force_rng_short_reads(uint64_t maximum_read_size);
void cmp_clear_force_rng_failures(void);

static std::atomic<int32_t> g_force_rng_open_errno(0);
static std::atomic<int32_t> g_force_rng_read_errno(0);
static std::atomic<int32_t> g_force_rng_close_errno(0);
static std::atomic<int32_t> g_force_rng_read_zero(0);
static std::atomic<uint64_t> g_force_rng_max_read_size(0U);

void cmp_force_rng_open_failure(int32_t error_code)
{
    g_force_rng_open_errno.store(error_code);
    return ;
}

void cmp_force_rng_read_failure(int32_t error_code)
{
    g_force_rng_read_errno.store(error_code);
    return ;
}

void cmp_force_rng_close_failure(int32_t error_code)
{
    g_force_rng_close_errno.store(error_code);
    return ;
}

void cmp_force_rng_read_eof(void)
{
    g_force_rng_read_zero.store(1);
    return ;
}

void cmp_force_rng_short_reads(uint64_t maximum_read_size)
{
    g_force_rng_max_read_size.store(maximum_read_size);
    return ;
}

void cmp_clear_force_rng_failures(void)
{
    g_force_rng_open_errno.store(0);
    g_force_rng_read_errno.store(0);
    g_force_rng_close_errno.store(0);
    g_force_rng_read_zero.store(0);
    g_force_rng_max_read_size.store(0U);
    return ;
}
#endif

int32_t cmp_rng_secure_bytes(unsigned char *buffer, ft_size_t length)
{
    int64_t bytes_read;
    int32_t file_descriptor;
    int32_t error_code;
    ft_size_t offset;
    ft_size_t read_size;

    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
#ifdef LIBFT_TEST_BUILD
    int32_t forced_open_errno = g_force_rng_open_errno.exchange(0);
    if (forced_open_errno != 0)
    {
        errno = forced_open_errno;
        error_code = cmp_map_system_error_to_ft(errno);
        return (error_code);
    }
#endif
    file_descriptor = open("/dev/urandom", O_RDONLY);
    if (file_descriptor < 0)
    {
        error_code = cmp_map_system_error_to_ft(errno);
        return (error_code);
    }
    offset = 0;
    while (offset < length)
    {
#ifdef LIBFT_TEST_BUILD
        int32_t forced_read_errno = g_force_rng_read_errno.exchange(0);

        if (forced_read_errno != 0)
        {
            if (forced_read_errno == EINTR)
                continue ;
            errno = forced_read_errno;
            error_code = cmp_map_system_error_to_ft(errno);
            int32_t stored_errno = errno;

            close(file_descriptor);
            errno = stored_errno;
            return (error_code);
        }
        if (g_force_rng_read_zero.exchange(0) != 0)
        {
            bytes_read = 0;
        }
        else
#endif
        {
            read_size = length - offset;
            if (read_size > static_cast<ft_size_t>(INT32_MAX))
                read_size = static_cast<ft_size_t>(INT32_MAX);
#ifdef LIBFT_TEST_BUILD
            uint64_t maximum_read_size = g_force_rng_max_read_size.load();

            if (maximum_read_size != 0U
                && read_size > maximum_read_size)
                read_size = static_cast<ft_size_t>(maximum_read_size);
#endif
            bytes_read = read(file_descriptor, buffer + offset, read_size);
        }
        if (bytes_read < 0)
        {
            if (errno == EINTR)
                continue ;
            int32_t stored_errno = errno;

            error_code = cmp_map_system_error_to_ft(errno);
            close(file_descriptor);
            errno = stored_errno;
            return (error_code);
        }
        if (bytes_read == 0)
        {
            int32_t close_result = close(file_descriptor);

            if (close_result < 0)
            {
                error_code = cmp_map_system_error_to_ft(errno);
                return (error_code);
            }
            return (FT_ERR_IO);
        }
        offset += static_cast<ft_size_t>(bytes_read);
    }
    if (close(file_descriptor) < 0)
    {
        error_code = cmp_map_system_error_to_ft(errno);
        return (error_code);
    }
#ifdef LIBFT_TEST_BUILD
    int32_t forced_close_errno = g_force_rng_close_errno.exchange(0);
    if (forced_close_errno != 0)
    {
        errno = forced_close_errno;
        error_code = cmp_map_system_error_to_ft(errno);
        return (error_code);
    }
#endif
    return (FT_ERR_SUCCESS);
}
#endif
