#include "compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include <cerrno>
#include <limits>
#include "../Basic/limits.hpp"

int32_t cmp_read(int32_t file_descriptor, void *buffer, ft_size_t count,
    int64_t *bytes_read_out)
{
    return (cmp_read(file_descriptor, buffer, count, bytes_read_out, ft_nullptr));
}

int32_t cmp_write(int32_t file_descriptor, const void *buffer, ft_size_t count,
    int64_t *bytes_written_out)
{
    return (cmp_write(file_descriptor, buffer, count, bytes_written_out, ft_nullptr));
}

static int32_t cmp_report_file_io_error(int32_t native_error, int32_t *system_error_out)
{
    if (system_error_out != ft_nullptr)
        *system_error_out = native_error;
    return (cmp_map_system_error_to_ft(native_error));
}

#if defined(_WIN32) || defined(_WIN64)
# include <atomic>
# include <windows.h>
# include <stdio.h>
# include <io.h>
# include <fcntl.h>

static constexpr int32_t CMP_TRACKED_FILE_LIMIT = 1024;
static std::atomic<HANDLE> g_file_handles[CMP_TRACKED_FILE_LIMIT]{};
static thread_local int32_t g_last_open_error = 0;
static thread_local int32_t g_last_open_error_code = FT_ERR_SUCCESS;

int32_t cmp_get_last_open_error(void)
{
    return (g_last_open_error);
}

int32_t cmp_get_last_open_error_code(void)
{
    return (g_last_open_error_code);
}

static void cmp_set_tracked_handle(int32_t file_descriptor, HANDLE handle)
{
    if (file_descriptor < 0 || file_descriptor >= CMP_TRACKED_FILE_LIMIT)
        return ;
    g_file_handles[file_descriptor].store(handle, std::memory_order_release);
    return ;
}

HANDLE cmp_retrieve_handle(int32_t file_descriptor)
{
    if (file_descriptor < 0 || file_descriptor >= CMP_TRACKED_FILE_LIMIT)
        return (INVALID_HANDLE_VALUE);
    const HANDLE stored_handle = g_file_handles[file_descriptor].load(std::memory_order_acquire);
    if (stored_handle == ft_nullptr)
        return (INVALID_HANDLE_VALUE);
    return (stored_handle);
}

static int32_t cmp_open_internal(const char *path_name, int32_t flags, int32_t mode)
{
    char *native_path;
    int32_t translate_error;
    DWORD desired_access;
    DWORD creation_disposition;
    DWORD flags_and_attributes;
    DWORD open_error;
    int32_t open_flags;
    HANDLE file_handle;
    int32_t file_descriptor;

    g_last_open_error = 0;
    g_last_open_error_code = FT_ERR_SUCCESS;
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
        g_last_open_error_code = translate_error;
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
    open_error = file_handle == INVALID_HANDLE_VALUE ? GetLastError() : 0U;
    cma_free(native_path);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        g_last_open_error = static_cast<int32_t>(open_error);
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
        g_last_open_error = errno;
        g_last_open_error_code = cmp_map_system_error_to_ft(errno);
        CloseHandle(file_handle);
        return (-1);
    }
    cmp_set_tracked_handle(file_descriptor, file_handle);
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
    int64_t *bytes_read_out, int32_t *system_error_out)
{
    if (bytes_read_out != ft_nullptr)
        *bytes_read_out = 0;
    if (system_error_out != ft_nullptr)
        *system_error_out = 0;
    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (count > static_cast<ft_size_t>(UINT32_MAX))
        return (FT_ERR_OUT_OF_RANGE);

    const int32_t read_result = _read(file_descriptor, buffer, static_cast<unsigned int>(count));
    if (read_result < 0)
        return (cmp_report_file_io_error(errno, system_error_out));
    if (bytes_read_out != ft_nullptr)
        *bytes_read_out = static_cast<int64_t>(read_result);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_write(int32_t file_descriptor, const void *buffer, ft_size_t count,
    int64_t *bytes_written_out, int32_t *system_error_out)
{
    if (bytes_written_out != ft_nullptr)
        *bytes_written_out = 0;
    if (system_error_out != ft_nullptr)
        *system_error_out = 0;
    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (count > static_cast<ft_size_t>(UINT32_MAX))
        return (FT_ERR_OUT_OF_RANGE);

    const int32_t write_result = _write(file_descriptor, buffer, static_cast<unsigned int>(count));
    if (write_result < 0)
        return (cmp_report_file_io_error(errno, system_error_out));
    if (bytes_written_out != ft_nullptr)
        *bytes_written_out = static_cast<int64_t>(write_result);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_close(int32_t file_descriptor)
{
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    // Clear before the CRT can recycle the descriptor for another open.
    cmp_set_tracked_handle(file_descriptor, ft_nullptr);
    if (_close(file_descriptor) < 0)
        return (cmp_map_system_error_to_ft(errno));
    return (FT_ERR_SUCCESS);
}

static void cmp_register_standard_descriptor(DWORD standard_id, int32_t target,
    int32_t flags)
{
    const HANDLE original = GetStdHandle(standard_id);
    if (original == ft_nullptr || original == INVALID_HANDLE_VALUE)
        return ;
    HANDLE duplicate;
    const HANDLE process = GetCurrentProcess();
    if (DuplicateHandle(process, original, process, &duplicate, 0U, FALSE,
            DUPLICATE_SAME_ACCESS) == 0)
        return ;
    const int32_t descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(duplicate),
        flags | _O_BINARY);
    if (descriptor < 0)
    {
        (void)CloseHandle(duplicate);
        return ;
    }
    if (descriptor != target)
    {
        const int32_t result = _dup2(descriptor, target);
        (void)_close(descriptor);
        if (result != 0)
            return ;
    }
    cmp_set_tracked_handle(target, reinterpret_cast<HANDLE>(_get_osfhandle(target)));
    return ;
}

void cmp_initialize_standard_file_descriptors()
{
    static const ft_bool initialized = []()
    {
        cmp_register_standard_descriptor(STD_INPUT_HANDLE, 0, _O_RDONLY);
        cmp_register_standard_descriptor(STD_OUTPUT_HANDLE, 1, _O_WRONLY);
        cmp_register_standard_descriptor(STD_ERROR_HANDLE, 2, _O_WRONLY);
        return (FT_TRUE);
    }();
    (void)initialized;
    return ;
}

#else
# include "../Basic/basic.hpp"
# include <fcntl.h>
# include <unistd.h>
# include <sys/stat.h>
# include <cerrno>

int32_t cmp_get_last_open_error(void)
{
    return (errno);
}

int32_t cmp_get_last_open_error_code(void)
{
    return (cmp_map_system_error_to_ft(errno));
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
    int64_t *bytes_read_out, int32_t *system_error_out)
{
    int64_t bytes_read;

    if (bytes_read_out != ft_nullptr)
        *bytes_read_out = 0;
    if (system_error_out != ft_nullptr)
        *system_error_out = 0;
    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    bytes_read = read(file_descriptor, buffer, count);
    if (bytes_read == -1)
        return (cmp_report_file_io_error(errno, system_error_out));
    if (bytes_read_out != ft_nullptr)
        *bytes_read_out = bytes_read;
    return (FT_ERR_SUCCESS);
}

int32_t cmp_write(int32_t file_descriptor, const void *buffer, ft_size_t count,
    int64_t *bytes_written_out, int32_t *system_error_out)
{
    int64_t bytes_written;

    if (bytes_written_out != ft_nullptr)
        *bytes_written_out = 0;
    if (system_error_out != ft_nullptr)
        *system_error_out = 0;
    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    bytes_written = write(file_descriptor, buffer, count);
    if (bytes_written == -1)
        return (cmp_report_file_io_error(errno, system_error_out));
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

int32_t cmp_file_stream_size(FILE *stream, ft_size_t *size_out) noexcept
{
    if (size_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *size_out = 0U;
    if (stream == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
#if defined(_WIN32) || defined(_WIN64)
    const int32_t descriptor = _fileno(stream);
    if (descriptor < 0)
        return (FT_ERR_IO);
    const HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(descriptor));
    LARGE_INTEGER length;
    if (GetFileSizeEx(handle, &length) == 0 || length.QuadPart < 0)
        return (FT_ERR_IO);
    *size_out = static_cast<ft_size_t>(length.QuadPart);
#else
    struct stat status;
    if (fstat(fileno(stream), &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0)
        return (FT_ERR_IO);
    *size_out = static_cast<ft_size_t>(status.st_size);
#endif
    return (FT_ERR_SUCCESS);
}

int32_t cmp_file_stream_seek_absolute(FILE *stream, ft_size_t position) noexcept
{
    if (stream == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
#if defined(_WIN32) || defined(_WIN64)
    if (position > static_cast<ft_size_t>(FT_LLONG_MAX))
        return (FT_ERR_OUT_OF_RANGE);
    if (_fseeki64(stream, static_cast<int64_t>(position), SEEK_SET) != 0)
        return (FT_ERR_IO);
#else
    if (position > static_cast<ft_size_t>(std::numeric_limits<off_t>::max()))
        return (FT_ERR_OUT_OF_RANGE);
    if (fseeko(stream, static_cast<off_t>(position), SEEK_SET) != 0)
        return (FT_ERR_IO);
#endif
    return (FT_ERR_SUCCESS);
}

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
