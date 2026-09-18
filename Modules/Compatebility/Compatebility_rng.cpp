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
    if (CryptAcquireContext(&crypt_provider, ft_nullptr, ft_nullptr,
            PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) == 0)
    {
        error_code = cmp_rng_windows_error(GetLastError());
        return (error_code);
    }
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
    return (FT_ERR_SUCCESS);
}

#else
# include <unistd.h>
# include <fcntl.h>
# include <cerrno>
# include <atomic>

void cmp_force_rng_open_failure(int32_t error_code);
void cmp_force_rng_read_failure(int32_t error_code);
void cmp_force_rng_read_eof(void);
void cmp_force_rng_close_failure(int32_t error_code);
void cmp_clear_force_rng_failures(void);

static std::atomic<int32_t> g_force_rng_open_errno(0);
static std::atomic<int32_t> g_force_rng_read_errno(0);
static std::atomic<int32_t> g_force_rng_close_errno(0);
static std::atomic<int32_t> g_force_rng_read_zero(0);

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

void cmp_clear_force_rng_failures(void)
{
    g_force_rng_open_errno.store(0);
    g_force_rng_read_errno.store(0);
    g_force_rng_close_errno.store(0);
    g_force_rng_read_zero.store(0);
    return ;
}

int32_t cmp_rng_secure_bytes(unsigned char *buffer, ft_size_t length)
{
    int32_t forced_open_errno = g_force_rng_open_errno.exchange(0);
    int64_t bytes_read;
    int32_t file_descriptor;
    int32_t error_code;
    ft_size_t offset;

    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (forced_open_errno != 0)
    {
        errno = forced_open_errno;
        error_code = cmp_map_system_error_to_ft(errno);
        return (error_code);
    }
    file_descriptor = open("/dev/urandom", O_RDONLY);
    if (file_descriptor < 0)
    {
        error_code = cmp_map_system_error_to_ft(errno);
        return (error_code);
    }
    offset = 0;
    while (offset < length)
    {
        int32_t forced_read_errno = g_force_rng_read_errno.exchange(0);

        if (forced_read_errno != 0)
        {
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
        {
            bytes_read = read(file_descriptor, buffer + offset, length - offset);
        }
        if (bytes_read < 0)
        {
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
    int32_t forced_close_errno = g_force_rng_close_errno.exchange(0);
    if (forced_close_errno != 0)
    {
        errno = forced_close_errno;
        error_code = cmp_map_system_error_to_ft(errno);
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}
#endif
