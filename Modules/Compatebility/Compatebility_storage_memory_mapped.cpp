#include "compatebility_internal.hpp"

#include <limits>
#if !defined(_WIN32) && !defined(_WIN64)
# include <cerrno>
# include <fcntl.h>
# include <sys/mman.h>
# include <unistd.h>
#endif

int32_t cmp_storage_write_memory_mapped_descriptor(int32_t descriptor,
    const char *buffer, ft_size_t length)
{
    if (descriptor < 0 || (buffer == ft_nullptr && length != 0))
        return (FT_ERR_INVALID_ARGUMENT);
#if defined(_WIN32) || defined(_WIN64)
    if (length > static_cast<ft_size_t>(std::numeric_limits<int64_t>::max()))
        return (FT_ERR_OUT_OF_RANGE);
    const HANDLE file = cmp_retrieve_handle(descriptor);
    if (file == INVALID_HANDLE_VALUE)
        return (FT_ERR_INVALID_HANDLE);
    LARGE_INTEGER size;
    size.QuadPart = static_cast<int64_t>(length);
    if (SetFilePointerEx(file, size, ft_nullptr, FILE_BEGIN) == FALSE || SetEndOfFile(file) == FALSE)
        return (FT_ERR_IO);
    if (length == 0)
        return (FT_ERR_SUCCESS);
    const HANDLE mapping = CreateFileMappingA(file, ft_nullptr, PAGE_READWRITE, 0, 0, ft_nullptr);
    if (mapping == ft_nullptr)
        return (FT_ERR_IO);
    void *view = MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, length);
    int32_t error_code = FT_ERR_IO;
    if (view != ft_nullptr)
    {
        ft_memcpy(view, buffer, length);
        const ft_bool flushed = FlushViewOfFile(view, length) != FALSE;
        const ft_bool unmapped = UnmapViewOfFile(view) != FALSE;
        if (flushed == FT_TRUE && unmapped == FT_TRUE)
            error_code = FT_ERR_SUCCESS;
    }
    CloseHandle(mapping);
    return (error_code);
#else
    if (length > static_cast<ft_size_t>(std::numeric_limits<off_t>::max()))
        return (FT_ERR_OUT_OF_RANGE);
    if (ftruncate(descriptor, static_cast<off_t>(length)) != 0)
        return (FT_ERR_IO);
    if (length == 0)
        return (FT_ERR_SUCCESS);
    void *view = mmap(ft_nullptr, length, PROT_WRITE, MAP_SHARED, descriptor, 0);
    if (view == MAP_FAILED)
        return (FT_ERR_IO);
    ft_memcpy(view, buffer, length);
    const int32_t sync_result = msync(view, length, MS_SYNC);
    const int32_t unmap_result = munmap(view, length);
    if (sync_result != 0 || unmap_result != 0)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
#endif
}

int32_t cmp_storage_read_memory_mapped_file(const char *location,
    cmp_storage_read_mapped_callback read_mapping, void *context)
{
    if (location == ft_nullptr || read_mapping == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
#if defined(_WIN32) || defined(_WIN64)
    const HANDLE file = CreateFileA(location, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        ft_nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, ft_nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return (FT_ERR_NOT_FOUND);
        return (FT_ERR_INVALID_OPERATION);
    }
    LARGE_INTEGER size;
    if (GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0
        || static_cast<uint64_t>(size.QuadPart) > FT_SYSTEM_SIZE_MAX)
    {
        CloseHandle(file);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (size.QuadPart == 0)
    {
        CloseHandle(file);
        return (FT_ERR_EMPTY);
    }
    const HANDLE mapping = CreateFileMappingA(file, ft_nullptr, PAGE_READONLY, 0, 0, ft_nullptr);
    CloseHandle(file);
    if (mapping == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    const ft_size_t length = static_cast<ft_size_t>(size.QuadPart);
    const char *view = static_cast<const char *>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, length));
    CloseHandle(mapping);
    if (view == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    const int32_t result = read_mapping(view, length, context);
    UnmapViewOfFile(view);
    return (result);
#else
    const int32_t descriptor = open(location, O_RDONLY);
    if (descriptor < 0)
    {
        if (errno == ENOENT)
            return (FT_ERR_NOT_FOUND);
        return (FT_ERR_INVALID_OPERATION);
    }
    struct stat status;
    if (fstat(descriptor, &status) != 0 || status.st_size < 0
        || static_cast<uint64_t>(status.st_size) > FT_SYSTEM_SIZE_MAX)
    {
        close(descriptor);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (status.st_size == 0)
    {
        close(descriptor);
        return (FT_ERR_EMPTY);
    }
    const ft_size_t length = static_cast<ft_size_t>(status.st_size);
    void *view = mmap(ft_nullptr, length, PROT_READ, MAP_SHARED, descriptor, 0);
    close(descriptor);
    if (view == MAP_FAILED)
        return (FT_ERR_INVALID_OPERATION);
    const int32_t result = read_mapping(static_cast<const char *>(view), length, context);
    munmap(view, length);
    return (result);
#endif
}
