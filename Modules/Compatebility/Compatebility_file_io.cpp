#include "compatebility_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include <cerrno>

#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"
#if defined(_WIN32) || defined(_WIN64)
# include "../Basic/class_nullptr.hpp"
# include "../PThread/mutex.hpp"
# include "../PThread/pthread_internal.hpp"
# include "../Basic/basic.hpp"
# include <windows.h>
# include <stdio.h>
# include <io.h>
# include <fcntl.h>

static HANDLE g_file_handles[1024];
static pt_mutex g_file_mutex;
static ft_bool g_file_mutex_ready = FT_FALSE;
static thread_local int32_t g_last_open_error = 0;

int32_t cmp_get_last_open_error(void)
{
    return (g_last_open_error);
}

static int32_t cmp_ensure_file_mutex_initialized(void)
{
    if (g_file_mutex_ready == FT_TRUE)
        return (FT_ERR_SUCCESS);
    if (g_file_mutex.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    g_file_mutex_ready = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

static void cmp_clear_handle(int32_t file_descriptor)
{
    if (file_descriptor < 0 || file_descriptor >= 1024)
        return ;
    g_file_handles[file_descriptor] = ft_nullptr;
    return ;
}

static int32_t cmp_lock_file_mutex(void)
{
    int32_t init_error;
    int32_t lock_result;

    init_error = cmp_ensure_file_mutex_initialized();
    if (init_error != FT_ERR_SUCCESS)
        return (init_error);
    lock_result = g_file_mutex.lock();
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    return (FT_ERR_SUCCESS);
}

static int32_t cmp_unlock_file_mutex(void)
{
    g_file_mutex.unlock();
    return (FT_ERR_SUCCESS);
}

HANDLE cmp_retrieve_handle(int32_t file_descriptor)
{
    if (file_descriptor < 0 || file_descriptor >= 1024)
        return (INVALID_HANDLE_VALUE);
    HANDLE stored_handle = g_file_handles[file_descriptor];
    if (stored_handle == ft_nullptr)
        return (INVALID_HANDLE_VALUE);
    return (stored_handle);
}

static int32_t cmp_open_internal(const char *path_name, int32_t flags, int32_t mode)
{
    char *native_path;
    int32_t lock_error;
    int32_t translate_error;
    DWORD desired_access;
    DWORD creation_disposition;
    DWORD flags_and_attributes;
    int32_t open_flags;
    HANDLE file_handle;
    int32_t file_descriptor;

    g_last_open_error = 0;
    if (path_name == ft_nullptr)
    {
        g_last_open_error = ERROR_INVALID_PARAMETER;
        return (-1);
    }
    native_path = ft_nullptr;
    translate_error = cmp_translate_path_to_native(path_name, &native_path);
    if (translate_error != FT_ERR_SUCCESS)
    {
        g_last_open_error = ERROR_INVALID_NAME;
        return (-1);
    }
    lock_error = cmp_lock_file_mutex();
    if (lock_error != FT_ERR_SUCCESS)
    {
        cma_free(native_path);
        return (-1);
    }
    desired_access = 0;
    creation_disposition = 0;
    flags_and_attributes = FILE_ATTRIBUTE_NORMAL;
    (void)mode;
    if (flags & O_DIRECTORY)
    {
        desired_access = FILE_LIST_DIRECTORY;
        creation_disposition = OPEN_EXISTING;
        flags_and_attributes = FILE_FLAG_BACKUP_SEMANTICS;
    }
    else
    {
        if ((flags & O_RDWR) == O_RDWR)
            desired_access = GENERIC_READ | GENERIC_WRITE;
        else if (flags & O_WRONLY)
            desired_access = GENERIC_WRITE;
        else
            desired_access = GENERIC_READ;
        if ((flags & O_CREAT) && (flags & O_EXCL))
            creation_disposition = CREATE_NEW;
        else if ((flags & O_CREAT) && (flags & O_TRUNC))
            creation_disposition = CREATE_ALWAYS;
        else if (flags & O_CREAT)
            creation_disposition = OPEN_ALWAYS;
        else if (flags & O_TRUNC)
            creation_disposition = TRUNCATE_EXISTING;
        else
            creation_disposition = OPEN_EXISTING;
        if (flags & O_APPEND)
            desired_access |= FILE_APPEND_DATA;
    }
    file_handle = CreateFileA(native_path, desired_access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, ft_nullptr, creation_disposition,
        flags_and_attributes, ft_nullptr);
    cma_free(native_path);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        g_last_open_error = static_cast<int32_t>(GetLastError());
        cmp_unlock_file_mutex();
        return (-1);
    }
    open_flags = _O_BINARY;
    if ((flags & O_RDWR) == O_RDWR)
        open_flags |= _O_RDWR;
    else if (flags & O_WRONLY)
        open_flags |= _O_WRONLY;
    else
        open_flags |= _O_RDONLY;
    if (flags & O_APPEND)
        open_flags |= _O_APPEND;
    file_descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(file_handle), open_flags);
    if (file_descriptor < 0)
    {
        g_last_open_error = static_cast<int32_t>(GetLastError());
        CloseHandle(file_handle);
        cmp_unlock_file_mutex();
        return (-1);
    }
    if (file_descriptor >= 0 && file_descriptor < 1024)
        g_file_handles[file_descriptor] = file_handle;
    cmp_unlock_file_mutex();
    return (file_descriptor);
}

int32_t cmp_open(const char *path_name)
{
    return (cmp_open_internal(path_name, O_RDONLY, 0));
}

int32_t cmp_open(const char *path_name, int32_t flags)
{
    return (cmp_open_internal(path_name, flags, 0));
}

int32_t cmp_open(const char *path_name, int32_t flags, int32_t mode)
{
    return (cmp_open_internal(path_name, flags, mode));
}

int32_t cmp_read(int32_t file_descriptor, void *buffer, ft_size_t count,
    int64_t *bytes_read_out)
{
    int32_t lock_error;
    int32_t read_result;

    if (bytes_read_out != ft_nullptr)
        *bytes_read_out = 0;
    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (count > static_cast<ft_size_t>(UINT32_MAX))
        return (FT_ERR_OUT_OF_RANGE);

    lock_error = cmp_lock_file_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    read_result = _read(file_descriptor, buffer, static_cast<unsigned int>(count));
    cmp_unlock_file_mutex();
    if (read_result < 0)
        return (cmp_map_system_error_to_ft(errno));
    if (bytes_read_out != ft_nullptr)
        *bytes_read_out = static_cast<int64_t>(read_result);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_write(int32_t file_descriptor, const void *buffer, ft_size_t count,
    int64_t *bytes_written_out)
{
    int32_t lock_error;
    int32_t write_result;

    if (bytes_written_out != ft_nullptr)
        *bytes_written_out = 0;
    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (count > static_cast<ft_size_t>(UINT32_MAX))
        return (FT_ERR_OUT_OF_RANGE);

    lock_error = cmp_lock_file_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    write_result = _write(file_descriptor, buffer, static_cast<unsigned int>(count));
    cmp_unlock_file_mutex();
    if (write_result < 0)
        return (cmp_map_system_error_to_ft(errno));
    if (bytes_written_out != ft_nullptr)
        *bytes_written_out = static_cast<int64_t>(write_result);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_close(int32_t file_descriptor)
{
    int32_t lock_error;
    int32_t close_result;

    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_error = cmp_lock_file_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    close_result = _close(file_descriptor);
    if (close_result < 0)
    {
        cmp_unlock_file_mutex();
        return (cmp_map_system_error_to_ft(errno));
    }
    cmp_clear_handle(file_descriptor);
    cmp_unlock_file_mutex();
    return (FT_ERR_SUCCESS);
}

void cmp_initialize_standard_file_descriptors()
{
    static int32_t initialised = 0;
    if (initialised == 1)
        return ;
    if (cmp_ensure_file_mutex_initialized() != FT_ERR_SUCCESS)
        return ;
    HANDLE standard_input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE standard_output = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE standard_error = GetStdHandle(STD_ERROR_HANDLE);
    if (standard_input != INVALID_HANDLE_VALUE)
    {
        int32_t file_descriptor_input = _open_osfhandle(
            reinterpret_cast<intptr_t>(standard_input), _O_RDONLY);
        if (file_descriptor_input != -1)
        {
            _dup2(file_descriptor_input, 0);
            cmp_clear_handle(0);
        }
    }
    if (standard_output != INVALID_HANDLE_VALUE)
    {
        int32_t file_descriptor_output = _open_osfhandle(
            reinterpret_cast<intptr_t>(standard_output), _O_WRONLY);
        if (file_descriptor_output != -1)
        {
            _dup2(file_descriptor_output, 1);
            cmp_clear_handle(1);
        }
    }
    if (standard_error != INVALID_HANDLE_VALUE)
    {
        int32_t file_descriptor_error = _open_osfhandle(
            reinterpret_cast<intptr_t>(standard_error), _O_WRONLY);
        if (file_descriptor_error != -1)
        {
            _dup2(file_descriptor_error, 2);
            cmp_clear_handle(2);
        }
    }
    int32_t lock_error;

    lock_error = cmp_lock_file_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    g_file_handles[0] = standard_input;
    g_file_handles[1] = standard_output;
    g_file_handles[2] = standard_error;
    cmp_unlock_file_mutex();
    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
    _setmode(2, _O_BINARY);
    initialised = 1;
    return ;
}

#else
# include "../Basic/class_nullptr.hpp"
# include "../Basic/basic.hpp"
# include <fcntl.h>
# include <unistd.h>
# include <sys/stat.h>
# include <cerrno>

int32_t cmp_get_last_open_error(void)
{
    return (errno);
}

int32_t cmp_open(const char *path_name)
{
    if (path_name == ft_nullptr)
    {
        errno = EINVAL;
        return (-1);
    }
    int32_t file_descriptor = open(path_name, O_RDONLY);
    if (file_descriptor == -1)
    {
        return (-1);
    }
    return (file_descriptor);
}

int32_t cmp_open(const char *path_name, int32_t flags)
{
    if (path_name == ft_nullptr)
    {
        errno = EINVAL;
        return (-1);
    }
    int32_t file_descriptor = open(path_name, flags);
    if (file_descriptor == -1)
    {
        return (-1);
    }
    return (file_descriptor);
}

int32_t cmp_open(const char *path_name, int32_t flags, mode_t mode)
{
    if (path_name == ft_nullptr)
    {
        errno = EINVAL;
        return (-1);
    }
    int32_t file_descriptor = open(path_name, flags, mode);
    if (file_descriptor == -1)
    {
        return (-1);
    }
    return (file_descriptor);
}

int32_t cmp_read(int32_t file_descriptor, void *buffer, ft_size_t count,
    int64_t *bytes_read_out)
{
    int64_t bytes_read;

    if (bytes_read_out != ft_nullptr)
        *bytes_read_out = 0;
    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    bytes_read = read(file_descriptor, buffer, count);
    if (bytes_read == -1)
        return (cmp_map_system_error_to_ft(errno));
    if (bytes_read_out != ft_nullptr)
        *bytes_read_out = bytes_read;
    return (FT_ERR_SUCCESS);
}

int32_t cmp_write(int32_t file_descriptor, const void *buffer, ft_size_t count,
    int64_t *bytes_written_out)
{
    int64_t bytes_written;

    if (bytes_written_out != ft_nullptr)
        *bytes_written_out = 0;
    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    bytes_written = write(file_descriptor, buffer, count);
    if (bytes_written == -1)
        return (cmp_map_system_error_to_ft(errno));
    if (bytes_written_out != ft_nullptr)
        *bytes_written_out = bytes_written;
    return (FT_ERR_SUCCESS);
}

int32_t cmp_close(int32_t file_descriptor)
{
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (close(file_descriptor) == -1)
        return (cmp_map_system_error_to_ft(errno));
    return (FT_ERR_SUCCESS);
}

void cmp_initialize_standard_file_descriptors()
{
    return ;
}

#endif

off_t cmp_lseek(int32_t file_descriptor, off_t offset, int32_t whence)
{
#if defined(_WIN32) || defined(_WIN64)
    off_t result;

    result = static_cast<off_t>(_lseeki64(file_descriptor,
        static_cast<__int64>(offset), whence));
    if (result < 0)
        return (static_cast<off_t>(-1));
    return (result);
#else
    off_t result;

    result = lseek(file_descriptor, offset, whence);
    return (result);
#endif
}
