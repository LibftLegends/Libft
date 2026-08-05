#include "compatebility_internal.hpp"

#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if defined(_WIN32) || defined(_WIN64)
# include "../Basic/class_nullptr.hpp"
# include <windows.h>
#else
# include <cerrno>
# include <fcntl.h>
# include <sys/mman.h>
# include <unistd.h>
#endif

int32_t cmp_storage_write_memory_mapped_file(const char *location,
    const char *buffer, ft_size_t length)
{
    if (location == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (buffer == ft_nullptr && length > 0)
        return (FT_ERR_INVALID_ARGUMENT);
#if defined(_WIN32) || defined(_WIN64)
    HANDLE file_handle;
    LARGE_INTEGER size_value;
    HANDLE mapping_handle;
    void *mapping_view;

    file_handle = CreateFileA(location, GENERIC_READ | GENERIC_WRITE, 0,
        ft_nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, ft_nullptr);
    if (file_handle == INVALID_HANDLE_VALUE)
        return (FT_ERR_INVALID_OPERATION);
    size_value.QuadPart = static_cast<LONGLONG>(length);
    if (SetFilePointerEx(file_handle, size_value, ft_nullptr, FILE_BEGIN) == FALSE
        || SetEndOfFile(file_handle) == FALSE)
    {
        CloseHandle(file_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (length == 0)
    {
        CloseHandle(file_handle);
        return (FT_ERR_SUCCESS);
    }
    mapping_handle = CreateFileMappingA(file_handle, ft_nullptr, PAGE_READWRITE, 0,
        0, ft_nullptr);
    if (mapping_handle == ft_nullptr)
    {
        CloseHandle(file_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    mapping_view = MapViewOfFile(mapping_handle, FILE_MAP_WRITE, 0, 0, 0);
    if (mapping_view == ft_nullptr)
    {
        CloseHandle(mapping_handle);
        CloseHandle(file_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    ft_memcpy(mapping_view, buffer, length);
    UnmapViewOfFile(mapping_view);
    CloseHandle(mapping_handle);
    CloseHandle(file_handle);
    return (FT_ERR_SUCCESS);
#else
    int32_t file_descriptor;
    void *mapping_pointer;

    file_descriptor = open(location, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_OPERATION);
    if (ftruncate(file_descriptor, static_cast<off_t>(length)) != 0)
    {
        close(file_descriptor);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (length == 0)
    {
        close(file_descriptor);
        return (FT_ERR_SUCCESS);
    }
    mapping_pointer = mmap(ft_nullptr, length, PROT_READ | PROT_WRITE,
        MAP_SHARED, file_descriptor, 0);
    if (mapping_pointer == MAP_FAILED)
    {
        close(file_descriptor);
        return (FT_ERR_INVALID_OPERATION);
    }
    ft_memcpy(mapping_pointer, buffer, length);
    msync(mapping_pointer, length, MS_SYNC);
    munmap(mapping_pointer, length);
    close(file_descriptor);
    return (FT_ERR_SUCCESS);
#endif
}

int32_t cmp_storage_read_memory_mapped_file(const char *location,
    char **buffer_out, ft_size_t *length_out)
{
    if (buffer_out == ft_nullptr || length_out == ft_nullptr
        || location == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *buffer_out = ft_nullptr;
    *length_out = 0;
#if defined(_WIN32) || defined(_WIN64)
    HANDLE file_handle;
    LARGE_INTEGER size_value;
    HANDLE mapping_handle;
    const char *content_pointer;
    char *buffer_pointer;
    DWORD last_error;

    file_handle = CreateFileA(location, GENERIC_READ, FILE_SHARE_READ, ft_nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, ft_nullptr);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        last_error = GetLastError();
        if (last_error == ERROR_FILE_NOT_FOUND)
            return (FT_ERR_NOT_FOUND);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (GetFileSizeEx(file_handle, &size_value) == FALSE)
    {
        CloseHandle(file_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (size_value.QuadPart == 0)
    {
        CloseHandle(file_handle);
        return (FT_ERR_EMPTY);
    }
    mapping_handle = CreateFileMappingA(file_handle, ft_nullptr, PAGE_READONLY, 0,
        0, ft_nullptr);
    if (mapping_handle == ft_nullptr)
    {
        CloseHandle(file_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    content_pointer = static_cast<const char *>(MapViewOfFile(mapping_handle,
        FILE_MAP_READ, 0, 0, static_cast<SIZE_T>(size_value.QuadPart)));
    if (content_pointer == ft_nullptr)
    {
        CloseHandle(mapping_handle);
        CloseHandle(file_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    buffer_pointer = static_cast<char *>(
        cma_malloc(static_cast<ft_size_t>(size_value.QuadPart) + 1));
    if (buffer_pointer == ft_nullptr)
    {
        UnmapViewOfFile(content_pointer);
        CloseHandle(mapping_handle);
        CloseHandle(file_handle);
        return (FT_ERR_NO_MEMORY);
    }
    ft_memcpy(buffer_pointer, content_pointer,
        static_cast<ft_size_t>(size_value.QuadPart));
    buffer_pointer[size_value.QuadPart] = '\0';
    UnmapViewOfFile(content_pointer);
    CloseHandle(mapping_handle);
    CloseHandle(file_handle);
    *buffer_out = buffer_pointer;
    *length_out = static_cast<ft_size_t>(size_value.QuadPart);
    return (FT_ERR_SUCCESS);
#else
    int32_t file_descriptor;
    struct stat file_stat;
    void *mapping_pointer;
    char *buffer_pointer;
    ft_size_t content_size;

    file_descriptor = open(location, O_RDONLY);
    if (file_descriptor < 0)
    {
        if (errno == ENOENT)
            return (FT_ERR_NOT_FOUND);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (fstat(file_descriptor, &file_stat) != 0)
    {
        close(file_descriptor);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (file_stat.st_size == 0)
    {
        close(file_descriptor);
        return (FT_ERR_EMPTY);
    }
    content_size = static_cast<ft_size_t>(file_stat.st_size);
    mapping_pointer = mmap(ft_nullptr, content_size, PROT_READ, MAP_SHARED,
        file_descriptor, 0);
    if (mapping_pointer == MAP_FAILED)
    {
        close(file_descriptor);
        return (FT_ERR_INVALID_OPERATION);
    }
    buffer_pointer = static_cast<char *>(cma_malloc(content_size + 1));
    if (buffer_pointer == ft_nullptr)
    {
        munmap(mapping_pointer, content_size);
        close(file_descriptor);
        return (FT_ERR_NO_MEMORY);
    }
    ft_memcpy(buffer_pointer, mapping_pointer, content_size);
    buffer_pointer[content_size] = '\0';
    munmap(mapping_pointer, content_size);
    close(file_descriptor);
    *buffer_out = buffer_pointer;
    *length_out = content_size;
    return (FT_ERR_SUCCESS);
#endif
}
