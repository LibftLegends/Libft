#include "compatebility_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../File/open_dir.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static void cmp_set_error_code(int32_t *error_code_out, int32_t error_code)
{
    if (error_code_out != ft_nullptr)
        *error_code_out = error_code;
    return ;
}

#if defined(_WIN32) || defined(_WIN64)
# include "../Basic/class_nullptr.hpp"
# include <windows.h>
# include <errno.h>
# ifndef DT_DIR
#  define DT_DIR 4
# endif
# ifndef DT_REG
#  define DT_REG 8
# endif

file_dir *cmp_dir_open(const char *directory_path, int32_t *error_code_out)
{
    WIN32_FIND_DATAA find_data;
    if (directory_path == ft_nullptr)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    ft_size_t directory_path_length = ft_strlen(directory_path);
    ft_size_t allocation_size = directory_path_length + 3;
    char *search_path = reinterpret_cast<char*>(cma_malloc(allocation_size));
    if (!search_path)
    {
        cmp_set_error_code(error_code_out, FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    ft_strlcpy(search_path, directory_path, allocation_size);
    ft_size_t search_path_length = ft_strlen(search_path);
    if (search_path_length > 0
        && (search_path[search_path_length - 1] == '\\'
            || search_path[search_path_length - 1] == '/'))
    {
        search_path[search_path_length] = '*';
        search_path[search_path_length + 1] = '\0';
    }
    else
    {
        search_path[search_path_length] = '\\';
        search_path[search_path_length + 1] = '*';
        search_path[search_path_length + 2] = '\0';
    }
    HANDLE handle = FindFirstFileA(search_path, &find_data);
    if (handle == INVALID_HANDLE_VALUE)
    {
        DWORD last_error = GetLastError();
        cma_free(search_path);
        if (last_error != 0)
            cmp_set_error_code(error_code_out,
                cmp_map_system_error_to_ft(static_cast<int32_t>(last_error)));
        else
            cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    cma_free(search_path);
    file_dir *directory_stream = reinterpret_cast<file_dir*>(cma_malloc(sizeof(file_dir)));
    if (!directory_stream)
    {
        FindClose(handle);
        cmp_set_error_code(error_code_out, FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    ft_memset(directory_stream, 0, sizeof(file_dir));
    directory_stream->file_descriptor = reinterpret_cast<intptr_t>(handle);
    directory_stream->w_find_data = find_data;
    directory_stream->first_read = FT_TRUE;
    int32_t mutex_error = directory_stream->mutex.initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        cma_free(directory_stream);
        FindClose(handle);
        cmp_set_error_code(error_code_out, mutex_error);
        return (ft_nullptr);
    }
    directory_stream->mutex_initialised = FT_TRUE;
    directory_stream->closed = FT_FALSE;
    ft_bzero(&directory_stream->entry, sizeof(directory_stream->entry));
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (directory_stream);
}

file_dirent *cmp_dir_read(file_dir *directory_stream, int32_t *error_code_out)
{
    if (directory_stream == ft_nullptr)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    if (!directory_stream->mutex_initialised)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    int32_t lock_result = directory_stream->mutex.lock();
    if (lock_result != FT_ERR_SUCCESS)
    {
        cmp_set_error_code(error_code_out, lock_result);
        return (ft_nullptr);
    }
    if (directory_stream->closed)
    {
        int32_t unlock_result = directory_stream->mutex.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
            cmp_set_error_code(error_code_out, unlock_result);
        else
            cmp_set_error_code(error_code_out, FT_ERR_INVALID_STATE);
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    HANDLE handle = reinterpret_cast<HANDLE>(directory_stream->file_descriptor);
    if (directory_stream->first_read)
    {
        directory_stream->first_read = FT_FALSE;
    }
    else if (!FindNextFileA(handle, &directory_stream->w_find_data))
    {
        DWORD last_error;

        last_error = GetLastError();
        if (last_error == ERROR_NO_MORE_FILES)
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        else if (last_error != 0)
            cmp_set_error_code(error_code_out,
                cmp_map_system_error_to_ft(static_cast<int32_t>(last_error)));
        else
            cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        int32_t unlock_result = directory_stream->mutex.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
            cmp_set_error_code(error_code_out, unlock_result);
        return (ft_nullptr);
    }
    ft_bzero(&directory_stream->entry, sizeof(directory_stream->entry));
    ft_strncpy(directory_stream->entry.d_name, directory_stream->w_find_data.cFileName,
        sizeof(directory_stream->entry.d_name) - 1);
    directory_stream->entry.d_ino = 0;
    if (directory_stream->w_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        directory_stream->entry.d_type = DT_DIR;
    else
        directory_stream->entry.d_type = DT_REG;
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    int32_t unlock_result = directory_stream->mutex.unlock();
    if (unlock_result != FT_ERR_SUCCESS)
        cmp_set_error_code(error_code_out, unlock_result);
    return (&directory_stream->entry);
}

int32_t cmp_dir_close(file_dir *directory_stream, int32_t *error_code_out)
{
    int32_t error_code;

    if (directory_stream == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (!directory_stream->mutex_initialised)
    {
        error_code = FT_ERR_INVALID_STATE;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    int32_t lock_result = directory_stream->mutex.lock();
    if (lock_result != FT_ERR_SUCCESS)
    {
        cmp_set_error_code(error_code_out, lock_result);
        return (lock_result);
    }
    if (directory_stream->closed)
    {
        int32_t unlock_result = directory_stream->mutex.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
            cmp_set_error_code(error_code_out, unlock_result);
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    directory_stream->closed = FT_TRUE;
    FindClose(reinterpret_cast<HANDLE>(directory_stream->file_descriptor));
    int32_t unlock_result = directory_stream->mutex.unlock();
    if (unlock_result != FT_ERR_SUCCESS)
        cmp_set_error_code(error_code_out, unlock_result);
    directory_stream->mutex.destroy();
    directory_stream->mutex_initialised = FT_FALSE;
    cma_free(directory_stream);
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_directory_exists(const char *path, int32_t *exists_out,
    int32_t *error_code_out)
{
    int32_t error_code;

    if (exists_out != ft_nullptr)
        *exists_out = 0;
    if (path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    DWORD attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES)
    {
        if (attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (exists_out != ft_nullptr)
                *exists_out = 1;
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            return (FT_ERR_SUCCESS);
        }
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    DWORD last_error = GetLastError();
    if (last_error != 0)
        error_code = cmp_map_system_error_to_ft(static_cast<int32_t>(last_error));
    else
        error_code = FT_ERR_INVALID_ARGUMENT;
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

#else
# include "../Basic/class_nullptr.hpp"
# include <dirent.h>
# include <errno.h>
# include <sys/syscall.h>
# include <cstdio>
# include <stdint.h>
#ifdef __linux__
struct linux_dirent64
{
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};
#endif

file_dir *cmp_dir_open(const char *directory_path, int32_t *error_code_out)
{
    if (directory_path == ft_nullptr)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
#ifdef __linux__
    int32_t file_descriptor = cmp_open(directory_path, O_DIRECTORY | O_RDONLY, 0);
    if (file_descriptor < 0)
    {
        if (errno != 0)
            cmp_set_error_code(error_code_out, cmp_file_error_to_errno(errno));
        else
            cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    file_dir *directory_stream = reinterpret_cast<file_dir*>(cma_malloc(sizeof(file_dir)));
    if (!directory_stream)
    {
        cmp_close(file_descriptor);
        cmp_set_error_code(error_code_out, FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    ft_memset(directory_stream, 0, sizeof(file_dir));
    directory_stream->file_descriptor = static_cast<intptr_t>(file_descriptor);
    directory_stream->buffer_size = 4096;
    directory_stream->buffer = reinterpret_cast<char*>(cma_malloc(directory_stream->buffer_size));
    if (!directory_stream->buffer)
    {
        cma_free(directory_stream);
        cmp_close(file_descriptor);
        cmp_set_error_code(error_code_out, FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    directory_stream->buffer_used = 0;
    directory_stream->buffer_offset = 0;
    int32_t mutex_error = directory_stream->mutex.initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        cma_free(directory_stream->buffer);
        cma_free(directory_stream);
        cmp_close(file_descriptor);
        cmp_set_error_code(error_code_out, mutex_error);
        return (ft_nullptr);
    }
    directory_stream->mutex_initialised = FT_TRUE;
    directory_stream->closed = FT_FALSE;
    ft_bzero(&directory_stream->entry, sizeof(directory_stream->entry));
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (directory_stream);
#else
    DIR *dir = opendir(directory_path);
    if (!dir)
    {
        if (errno != 0)
            cmp_set_error_code(error_code_out, cmp_file_error_to_errno(errno));
        else
            cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    file_dir *directory_stream = reinterpret_cast<file_dir*>(cma_malloc(sizeof(file_dir)));
    if (!directory_stream)
    {
        closedir(dir);
        cmp_set_error_code(error_code_out, FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    ft_memset(directory_stream, 0, sizeof(file_dir));
    directory_stream->file_descriptor = reinterpret_cast<intptr_t>(dir);
    int32_t mutex_error = directory_stream->mutex.initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        cma_free(directory_stream);
        closedir(dir);
        cmp_set_error_code(error_code_out, mutex_error);
        return (ft_nullptr);
    }
    directory_stream->mutex_initialised = FT_TRUE;
    directory_stream->closed = FT_FALSE;
    ft_bzero(&directory_stream->entry, sizeof(directory_stream->entry));
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (directory_stream);
#endif
}

file_dirent *cmp_dir_read(file_dir *directory_stream, int32_t *error_code_out)
{
    if (directory_stream == ft_nullptr)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
#ifdef __linux__
    if (!directory_stream->mutex_initialised)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    int32_t lock_result = directory_stream->mutex.lock();
    if (lock_result != FT_ERR_SUCCESS)
    {
        cmp_set_error_code(error_code_out, lock_result);
        return (ft_nullptr);
    }
    if (directory_stream->closed)
    {
        int32_t unlock_result = directory_stream->mutex.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
            cmp_set_error_code(error_code_out, unlock_result);
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    if (directory_stream->buffer_offset
            >= static_cast<ft_size_t>(directory_stream->buffer_used))
    {
        int64_t bytes;

        directory_stream->buffer_offset = 0;
        bytes = syscall(SYS_getdents64, static_cast<int32_t>(directory_stream->file_descriptor),
            reinterpret_cast<linux_dirent64*>(directory_stream->buffer),
            directory_stream->buffer_size);
        if (bytes <= 0)
        {
            if (bytes == 0)
                cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            else if (errno != 0)
        cmp_set_error_code(error_code_out, cmp_map_system_error_to_ft(errno));
        else
            cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        int32_t unlock_result = directory_stream->mutex.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
            cmp_set_error_code(error_code_out, unlock_result);
        return (ft_nullptr);
    }
        directory_stream->buffer_used = bytes;
    }
    linux_dirent64 *raw = reinterpret_cast<linux_dirent64*>(directory_stream->buffer + directory_stream->buffer_offset);
    if (raw->d_reclen == 0)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        int32_t unlock_result = directory_stream->mutex.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
            cmp_set_error_code(error_code_out, unlock_result);
        return (ft_nullptr);
    }
    ft_bzero(&directory_stream->entry, sizeof(directory_stream->entry));
    directory_stream->entry.d_ino = raw->d_ino;
    directory_stream->entry.d_type = raw->d_type;
    ft_strncpy(directory_stream->entry.d_name, raw->d_name, sizeof(directory_stream->entry.d_name) - 1);
    directory_stream->buffer_offset += raw->d_reclen;
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    int32_t unlock_result = directory_stream->mutex.unlock();
    if (unlock_result != FT_ERR_SUCCESS)
        cmp_set_error_code(error_code_out, unlock_result);
    return (&directory_stream->entry);
#else
    if (!directory_stream->mutex_initialised)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    int32_t lock_result = directory_stream->mutex.lock();
    if (lock_result != FT_ERR_SUCCESS)
    {
        cmp_set_error_code(error_code_out, lock_result);
        return (ft_nullptr);
    }
    if (directory_stream->closed)
    {
        int32_t unlock_result = directory_stream->mutex.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
            cmp_set_error_code(error_code_out, unlock_result);
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    DIR *dir = reinterpret_cast<DIR*>(directory_stream->file_descriptor);
    struct dirent *entry = readdir(dir);
    if (!entry)
    {
        if (errno != 0)
            cmp_set_error_code(error_code_out, cmp_map_system_error_to_ft(errno));
        else
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        int32_t unlock_result = directory_stream->mutex.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
            cmp_set_error_code(error_code_out, unlock_result);
        return (ft_nullptr);
    }
    ft_bzero(&directory_stream->entry, sizeof(directory_stream->entry));
    directory_stream->entry.d_ino = entry->d_ino;
    directory_stream->entry.d_type = entry->d_type;
    ft_strncpy(directory_stream->entry.d_name, entry->d_name, sizeof(directory_stream->entry.d_name) - 1);
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    int32_t unlock_result = directory_stream->mutex.unlock();
    if (unlock_result != FT_ERR_SUCCESS)
        cmp_set_error_code(error_code_out, unlock_result);
    return (&directory_stream->entry);
#endif
}

int32_t cmp_dir_close(file_dir *directory_stream, int32_t *error_code_out)
{
    int32_t error_code;

#ifdef __linux__
    if (directory_stream == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (!directory_stream->mutex_initialised)
    {
        error_code = FT_ERR_INVALID_STATE;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    int32_t lock_result = directory_stream->mutex.lock();
    if (lock_result != FT_ERR_SUCCESS)
    {
        cmp_set_error_code(error_code_out, lock_result);
        return (lock_result);
    }
    if (directory_stream->closed)
    {
        int32_t unlock_result = directory_stream->mutex.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
            cmp_set_error_code(error_code_out, unlock_result);
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    directory_stream->closed = FT_TRUE;
    cmp_close(static_cast<int32_t>(directory_stream->file_descriptor));
    cma_free(directory_stream->buffer);
    int32_t unlock_result = directory_stream->mutex.unlock();
    if (unlock_result != FT_ERR_SUCCESS)
        cmp_set_error_code(error_code_out, unlock_result);
    directory_stream->mutex.destroy();
    directory_stream->mutex_initialised = FT_FALSE;
    cma_free(directory_stream);
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
#else
    if (directory_stream == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (!directory_stream->mutex_initialised)
    {
        error_code = FT_ERR_INVALID_STATE;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    int32_t lock_result = directory_stream->mutex.lock();
    if (lock_result != FT_ERR_SUCCESS)
    {
        cmp_set_error_code(error_code_out, lock_result);
        return (lock_result);
    }
    if (directory_stream->closed)
    {
        int32_t unlock_result = directory_stream->mutex.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
            cmp_set_error_code(error_code_out, unlock_result);
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    directory_stream->closed = FT_TRUE;
    DIR *dir = reinterpret_cast<DIR*>(directory_stream->file_descriptor);
    closedir(dir);
    int32_t unlock_result = directory_stream->mutex.unlock();
    if (unlock_result != FT_ERR_SUCCESS)
        cmp_set_error_code(error_code_out, unlock_result);
    directory_stream->mutex.destroy();
    directory_stream->mutex_initialised = FT_FALSE;
    cma_free(directory_stream);
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
#endif
}

int32_t cmp_directory_exists(const char *path, int32_t *exists_out,
    int32_t *error_code_out)
{
    int32_t error_code;

    if (exists_out != ft_nullptr)
        *exists_out = 0;
    if (path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    struct stat stat_buffer;
    if (stat(path, &stat_buffer) == 0)
    {
        if (S_ISDIR(stat_buffer.st_mode))
        {
            if (exists_out != ft_nullptr)
                *exists_out = 1;
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            return (FT_ERR_SUCCESS);
        }
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (errno != 0)
        error_code = cmp_file_error_to_errno(errno);
    else
        error_code = FT_ERR_INVALID_ARGUMENT;
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

#endif
