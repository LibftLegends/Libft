#include "compatebility_internal.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../File/file_utils.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if !defined(_WIN32) && !defined(_WIN64)
# include <fcntl.h>
#endif

static void cmp_set_error_code(int32_t *error_code_out, int32_t error_code)
{
    if (error_code_out != ft_nullptr)
        *error_code_out = error_code;
    return ;
}

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <stdio.h>
# include <errno.h>
# include <sys/stat.h>

void cmp_set_force_cross_device_move(int32_t force_cross_device_move);

static ft_bool global_force_cross_device_move = FT_FALSE;

int32_t cmp_file_error_to_errno(int32_t system_error) noexcept
{
    if (system_error == ERROR_FILE_NOT_FOUND || system_error == ERROR_PATH_NOT_FOUND)
        return (FT_ERR_IO);
    if (system_error == ERROR_ACCESS_DENIED)
        return (FT_ERR_PERMISSION_DENIED);
    if (system_error == ERROR_ALREADY_EXISTS || system_error == ERROR_FILE_EXISTS)
        return (FT_ERR_ALREADY_EXISTS);
    if (system_error == ERROR_INVALID_NAME || system_error == ERROR_INVALID_PARAMETER)
        return (FT_ERR_INVALID_PATH);
    if (system_error == ERROR_FILENAME_EXCED_RANGE)
        return (FT_ERR_PATH_TOO_LONG);
    if (system_error == ERROR_DISK_FULL || system_error == ERROR_HANDLE_DISK_FULL)
        return (FT_ERR_DISK_FULL);
    if (system_error == ENOENT)
        return (FT_ERR_IO);
    if (system_error == ENOTDIR)
        return (FT_ERR_IO);
    if (system_error == EISDIR)
        return (FT_ERR_INVALID_OPERATION);
    if (system_error == EACCES || system_error == EPERM)
        return (FT_ERR_PERMISSION_DENIED);
    if (system_error == EEXIST)
        return (FT_ERR_ALREADY_EXISTS);
    if (system_error == ENAMETOOLONG)
        return (FT_ERR_PATH_TOO_LONG);
    if (system_error == EINVAL)
        return (FT_ERR_INVALID_PATH);
    if (system_error == ENOSPC)
        return (FT_ERR_DISK_FULL);
    return (cmp_map_system_error_to_ft(system_error));
}

void cmp_set_force_cross_device_move(int32_t force_cross_device_move)
{
    if (force_cross_device_move != 0)
        global_force_cross_device_move = FT_TRUE;
    else
        global_force_cross_device_move = FT_FALSE;
    return ;
}

int32_t cmp_file_exists(const char *path, int32_t *exists_out,
    int32_t *error_code_out)
{
    char *native_path;
    int32_t error_code;
    DWORD file_attributes;
    DWORD last_error;

    if (exists_out != ft_nullptr)
        *exists_out = 0;
    if (path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    native_path = ft_nullptr;
    if (cmp_translate_path_to_native(path, &native_path) != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    file_attributes = GetFileAttributesA(native_path);
    cma_free(native_path);
    if (file_attributes != INVALID_FILE_ATTRIBUTES)
    {
        if ((file_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            if (exists_out != ft_nullptr)
                *exists_out = 1;
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            return (FT_ERR_SUCCESS);
        }
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    last_error = GetLastError();
    if (last_error != 0)
        error_code = cmp_file_error_to_errno(static_cast<int32_t>(last_error));
    else
        error_code = FT_ERR_INVALID_ARGUMENT;
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_delete(const char *path, int32_t *error_code_out)
{
    char *native_path;
    int32_t error_code;
    DWORD last_error;
    DWORD file_attributes;

    if (path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    native_path = ft_nullptr;
    if (cmp_translate_path_to_native(path, &native_path) != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    file_attributes = GetFileAttributesA(native_path);
    if (file_attributes != INVALID_FILE_ATTRIBUTES
        && (file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        cma_free(native_path);
        error_code = FT_ERR_INVALID_OPERATION;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (DeleteFileA(native_path) != 0)
    {
        cma_free(native_path);
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    cma_free(native_path);
    last_error = GetLastError();
    if (last_error != 0)
        error_code = cmp_file_error_to_errno(static_cast<int32_t>(last_error));
    else
        error_code = FT_ERR_INVALID_ARGUMENT;
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_move(const char *source_path, const char *destination_path, int32_t *error_code_out)
{
    char *native_source_path;
    char *native_destination_path;
    DWORD last_error;
    int32_t stored_error;
    int32_t delete_error;
    int32_t copy_error;

    if (source_path == ft_nullptr || destination_path == ft_nullptr)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    native_source_path = ft_nullptr;
    native_destination_path = ft_nullptr;
    if (cmp_translate_path_to_native(source_path, &native_source_path) != FT_ERR_SUCCESS
        || cmp_translate_path_to_native(destination_path, &native_destination_path) != FT_ERR_SUCCESS)
    {
        cma_free(native_source_path);
        cma_free(native_destination_path);
        cmp_set_error_code(error_code_out, FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    if (global_force_cross_device_move == FT_FALSE)
    {
        if (MoveFileExA(native_source_path, native_destination_path,
                MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
        {
            cma_free(native_source_path);
            cma_free(native_destination_path);
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            return (FT_ERR_SUCCESS);
        }
        last_error = GetLastError();
    }
    else
        last_error = ERROR_NOT_SAME_DEVICE;
    if (last_error == ERROR_NOT_SAME_DEVICE)
    {
        cma_free(native_source_path);
        cma_free(native_destination_path);
        if (cmp_file_copy(source_path, destination_path, &copy_error) == 0)
        {
            if (cmp_file_delete(source_path, &delete_error) == 0)
            {
                cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
                return (FT_ERR_SUCCESS);
            }
            stored_error = delete_error;
            cmp_file_delete(destination_path, &delete_error);
            if (stored_error == FT_ERR_SUCCESS)
                stored_error = FT_ERR_INTERNAL;
            cmp_set_error_code(error_code_out, stored_error);
            return (stored_error);
        }
        if (copy_error != FT_ERR_SUCCESS)
        {
            cmp_set_error_code(error_code_out, copy_error);
            return (copy_error);
        }
        cmp_set_error_code(error_code_out, FT_ERR_INTERNAL);
        return (FT_ERR_INTERNAL);
    }
    else if (last_error != 0)
        stored_error = cmp_file_error_to_errno(static_cast<int32_t>(last_error));
    else
        stored_error = FT_ERR_INVALID_ARGUMENT;
    cma_free(native_source_path);
    cma_free(native_destination_path);
    if (stored_error == FT_ERR_SUCCESS)
        stored_error = FT_ERR_INTERNAL;
    cmp_set_error_code(error_code_out, stored_error);
    return (stored_error);
}

int32_t cmp_file_copy(const char *source_path, const char *destination_path, int32_t *error_code_out)
{
    char *native_source_path;
    char *native_destination_path;
    int32_t error_code;
    DWORD last_error;
    DWORD destination_attributes;

    if (source_path == ft_nullptr || destination_path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    native_source_path = ft_nullptr;
    native_destination_path = ft_nullptr;
    if (cmp_translate_path_to_native(source_path, &native_source_path) != FT_ERR_SUCCESS
        || cmp_translate_path_to_native(destination_path, &native_destination_path) != FT_ERR_SUCCESS)
    {
        cma_free(native_source_path);
        cma_free(native_destination_path);
        error_code = FT_ERR_NO_MEMORY;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    destination_attributes = GetFileAttributesA(native_destination_path);
    if (destination_attributes != INVALID_FILE_ATTRIBUTES
        && (destination_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        cma_free(native_source_path);
        cma_free(native_destination_path);
        error_code = FT_ERR_INVALID_OPERATION;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (CopyFileA(native_source_path, native_destination_path, 0))
    {
        cma_free(native_source_path);
        cma_free(native_destination_path);
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    cma_free(native_source_path);
    cma_free(native_destination_path);
    last_error = GetLastError();
    if (last_error != 0)
        error_code = cmp_file_error_to_errno(static_cast<int32_t>(last_error));
    else
        error_code = FT_ERR_INVALID_ARGUMENT;
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_create_directory(const char *path, mode_t mode, int32_t *error_code_out)
{
    char *native_path;
    int32_t error_code;
    (void)mode;
    DWORD last_error;

    if (path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    native_path = ft_nullptr;
    if (cmp_translate_path_to_native(path, &native_path) != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (CreateDirectoryA(native_path, ft_nullptr))
    {
        cma_free(native_path);
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    cma_free(native_path);
    last_error = GetLastError();
    if (last_error != 0)
        error_code = cmp_file_error_to_errno(static_cast<int32_t>(last_error));
    else
        error_code = FT_ERR_INVALID_ARGUMENT;
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_get_type(const char *path, file_type *type_out,
    int32_t *error_code_out)
{
    int32_t error_code;
    DWORD file_attributes;
    DWORD last_error;

    if (type_out != ft_nullptr)
        *type_out = FILE_TYPE_UNKNOWN;
    if (path == ft_nullptr || type_out == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    file_attributes = GetFileAttributesA(path);
    if (file_attributes == INVALID_FILE_ATTRIBUTES)
    {
        last_error = GetLastError();
        if (last_error == ERROR_FILE_NOT_FOUND || last_error == ERROR_PATH_NOT_FOUND)
        {
            *type_out = FILE_TYPE_MISSING;
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            return (FT_ERR_SUCCESS);
        }
        error_code = cmp_file_error_to_errno(static_cast<int32_t>(last_error));
        if (error_code == FT_ERR_NOT_FOUND)
        {
            *type_out = FILE_TYPE_MISSING;
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            return (FT_ERR_SUCCESS);
        }
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if ((file_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        *type_out = FILE_TYPE_SYMLINK;
    else if ((file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        *type_out = FILE_TYPE_DIRECTORY;
    else
        *type_out = FILE_TYPE_REGULAR;
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_file_get_size(const char *path, ft_size_t *size_out,
    int32_t *error_code_out)
{
    int32_t error_code;
    struct _stat file_info;

    if (size_out != ft_nullptr)
        *size_out = 0;
    if (path == ft_nullptr || size_out == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (_stat(path, &file_info) == 0)
    {
        *size_out = static_cast<ft_size_t>(file_info.st_size);
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

int32_t cmp_file_get_modification_time(const char *path, t_time *modification_time_out,
    int32_t *error_code_out)
{
    int32_t error_code;
    struct _stat file_info;

    if (modification_time_out != ft_nullptr)
        *modification_time_out = 0;
    if (path == ft_nullptr || modification_time_out == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (_stat(path, &file_info) == 0)
    {
        *modification_time_out = static_cast<t_time>(file_info.st_mtime);
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

int32_t cmp_file_get_permissions(const char *path, mode_t *mode_out, int32_t *error_code_out)
{
    int32_t error_code;
    struct _stat file_info;

    if (path == ft_nullptr || mode_out == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (_stat(path, &file_info) == 0)
    {
        *mode_out = static_cast<mode_t>(file_info.st_mode);
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

int32_t cmp_file_set_permissions(const char *path, int32_t owner_permissions,
    int32_t group_permissions, int32_t other_permissions, int32_t *error_code_out)
{
    int32_t error_code;
    int32_t windows_mode;

    if (path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (owner_permissions < 0 || owner_permissions > 7
        || group_permissions < 0 || group_permissions > 7
        || other_permissions < 0 || other_permissions > 7)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    windows_mode = 0;
    if ((owner_permissions & 4) != 0 || (group_permissions & 4) != 0
        || (other_permissions & 4) != 0)
        windows_mode |= _S_IREAD;
    if ((owner_permissions & 2) != 0 || (group_permissions & 2) != 0
        || (other_permissions & 2) != 0)
        windows_mode |= _S_IWRITE;
    if (_chmod(path, windows_mode) == 0)
    {
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

#else
# include <filesystem>
# include <cstdio>
# include <cerrno>
# include <system_error>
# include <new>
# include <sys/stat.h>

void cmp_set_force_cross_device_move(int32_t force_cross_device_move);

static ft_bool global_force_cross_device_move = FT_FALSE;

int32_t cmp_file_error_to_errno(int32_t system_error) noexcept
{
    if (system_error == ENOENT)
        return (FT_ERR_IO);
    if (system_error == ENOTDIR)
        return (FT_ERR_IO);
    if (system_error == EISDIR)
        return (FT_ERR_INVALID_OPERATION);
    if (system_error == EACCES || system_error == EPERM)
        return (FT_ERR_PERMISSION_DENIED);
    if (system_error == EEXIST)
        return (FT_ERR_ALREADY_EXISTS);
    if (system_error == ENAMETOOLONG)
        return (FT_ERR_PATH_TOO_LONG);
    if (system_error == EINVAL)
        return (FT_ERR_INVALID_PATH);
    if (system_error == ENOSPC)
        return (FT_ERR_DISK_FULL);
    return (cmp_map_system_error_to_ft(system_error));
}

void cmp_set_force_cross_device_move(int32_t force_cross_device_move)
{
    if (force_cross_device_move != 0)
        global_force_cross_device_move = FT_TRUE;
    else
        global_force_cross_device_move = FT_FALSE;
    return ;
}

int32_t cmp_file_exists(const char *path, int32_t *exists_out,
    int32_t *error_code_out)
{
    int32_t error_code;
    struct stat stat_buffer;

    if (exists_out != ft_nullptr)
        *exists_out = 0;
    if (path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (stat(path, &stat_buffer) == 0)
    {
        if (S_ISREG(stat_buffer.st_mode))
        {
            if (exists_out != ft_nullptr)
                *exists_out = 1;
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            return (FT_ERR_SUCCESS);
        }
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    error_code = cmp_file_error_to_errno(errno);
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_delete(const char *path, int32_t *error_code_out)
{
    int32_t error_code;
    struct stat path_status;

    if (path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (stat(path, &path_status) == 0 && S_ISDIR(path_status.st_mode))
    {
        error_code = FT_ERR_INVALID_OPERATION;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (unlink(path) == 0)
    {
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    error_code = cmp_file_error_to_errno(errno);
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_move(const char *source_path, const char *destination_path, int32_t *error_code_out)
{
    std::error_code copy_error_code;
    std::error_code directory_error_code;
    int32_t delete_errno;
    ft_bool destination_is_directory;

    try
    {
        if (source_path == ft_nullptr || destination_path == ft_nullptr)
        {
            cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_INVALID_ARGUMENT);
        }
        if (global_force_cross_device_move != FT_FALSE)
            errno = EXDEV;
        if (global_force_cross_device_move == FT_FALSE)
        {
            if (rename(source_path, destination_path) == 0)
            {
                cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
                return (FT_ERR_SUCCESS);
            }
            if (errno != EXDEV)
            {
                int32_t error_code = cmp_file_error_to_errno(errno);
                cmp_set_error_code(error_code_out, error_code);
                return (error_code);
            }
        }
        destination_is_directory = std::filesystem::is_directory(destination_path,
            directory_error_code);
        if (directory_error_code.value() != 0
            && directory_error_code.value()
                != static_cast<int32_t>(std::errc::not_a_directory))
        {
            cmp_set_error_code(error_code_out,
                cmp_file_error_to_errno(directory_error_code.value()));
            return (cmp_file_error_to_errno(directory_error_code.value()));
        }
        if (destination_is_directory != FT_FALSE)
        {
            cmp_set_error_code(error_code_out, FT_ERR_INVALID_OPERATION);
            return (FT_ERR_INVALID_OPERATION);
        }
        std::filesystem::copy_file(source_path, destination_path,
            std::filesystem::copy_options::overwrite_existing, copy_error_code);
        if (copy_error_code.value() == 0)
        {
            if (unlink(source_path) == 0)
            {
                cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
                return (FT_ERR_SUCCESS);
            }
            delete_errno = errno;
            std::error_code remove_error_code;

            std::filesystem::remove(destination_path, remove_error_code);
            cmp_set_error_code(error_code_out, cmp_file_error_to_errno(delete_errno));
            return (cmp_file_error_to_errno(delete_errno));
        }
        if (copy_error_code.value() != 0)
        {
            int32_t copy_value;

            copy_value = copy_error_code.value();
            cmp_set_error_code(error_code_out, cmp_file_error_to_errno(copy_value));
        }
    }
    catch (const std::bad_alloc &)
    {
        cmp_set_error_code(error_code_out, FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    catch (...)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INTERNAL);
        return (FT_ERR_INTERNAL);
    }
    return (FT_ERR_INTERNAL);
}

int32_t cmp_file_copy(const char *source_path, const char *destination_path, int32_t *error_code_out)
{
    std::error_code directory_error_code;
    std::error_code copy_error_code;
    int32_t error_code;
    ft_bool destination_is_directory;

    if (source_path == ft_nullptr || destination_path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    try
    {
        destination_is_directory = std::filesystem::is_directory(destination_path,
            directory_error_code);
        if (directory_error_code.value() == 0
            && destination_is_directory != FT_FALSE)
        {
            cmp_set_error_code(error_code_out, FT_ERR_INVALID_OPERATION);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (directory_error_code.value() != 0
            && directory_error_code.value()
                != static_cast<int32_t>(std::errc::not_a_directory)
            && directory_error_code.value()
                != static_cast<int32_t>(std::errc::no_such_file_or_directory))
        {
            error_code = cmp_file_error_to_errno(directory_error_code.value());
            cmp_set_error_code(error_code_out, error_code);
            return (error_code);
        }
        std::filesystem::copy_file(source_path, destination_path,
            std::filesystem::copy_options::overwrite_existing, copy_error_code);
        if (copy_error_code.value() == 0)
        {
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            return (FT_ERR_SUCCESS);
        }
        if (copy_error_code.value() != 0)
        {
            int32_t copy_value;

            copy_value = copy_error_code.value();
            error_code = cmp_file_error_to_errno(copy_value);
            cmp_set_error_code(error_code_out, error_code);
            return (error_code);
        }
    }
    catch (const std::bad_alloc &)
    {
        cmp_set_error_code(error_code_out, FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    catch (...)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INTERNAL);
        return (FT_ERR_INTERNAL);
    }
    return (FT_ERR_INTERNAL);
}

int32_t cmp_file_create_directory(const char *path, mode_t mode, int32_t *error_code_out)
{
    int32_t error_code;

    if (path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (mkdir(path, mode) == 0)
    {
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    error_code = cmp_file_error_to_errno(errno);
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_get_type(const char *path, file_type *type_out,
    int32_t *error_code_out)
{
    int32_t error_code;
    struct stat stat_buffer;

    if (type_out != ft_nullptr)
        *type_out = FILE_TYPE_UNKNOWN;
    if (path == ft_nullptr || type_out == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (lstat(path, &stat_buffer) != 0)
    {
        if (errno == ENOENT || errno == ENOTDIR)
        {
            *type_out = FILE_TYPE_MISSING;
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            return (FT_ERR_SUCCESS);
        }
        error_code = cmp_file_error_to_errno(errno);
        if (error_code == FT_ERR_NOT_FOUND)
        {
            *type_out = FILE_TYPE_MISSING;
            cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
            return (FT_ERR_SUCCESS);
        }
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (S_ISLNK(stat_buffer.st_mode))
        *type_out = FILE_TYPE_SYMLINK;
    else if (S_ISDIR(stat_buffer.st_mode))
        *type_out = FILE_TYPE_DIRECTORY;
    else if (S_ISREG(stat_buffer.st_mode))
        *type_out = FILE_TYPE_REGULAR;
    else
        *type_out = FILE_TYPE_UNKNOWN;
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_file_get_size(const char *path, ft_size_t *size_out,
    int32_t *error_code_out)
{
    int32_t error_code;
    struct stat stat_buffer;

    if (size_out != ft_nullptr)
        *size_out = 0;
    if (path == ft_nullptr || size_out == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (stat(path, &stat_buffer) == 0)
    {
        *size_out = static_cast<ft_size_t>(stat_buffer.st_size);
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    error_code = cmp_file_error_to_errno(errno);
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_get_modification_time(const char *path, t_time *modification_time_out,
    int32_t *error_code_out)
{
    int32_t error_code;
    struct stat stat_buffer;

    if (modification_time_out != ft_nullptr)
        *modification_time_out = 0;
    if (path == ft_nullptr || modification_time_out == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (stat(path, &stat_buffer) == 0)
    {
        *modification_time_out = stat_buffer.st_mtime;
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    error_code = cmp_file_error_to_errno(errno);
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_get_permissions(const char *path, mode_t *mode_out, int32_t *error_code_out)
{
    int32_t error_code;
    struct stat file_info;

    if (path == ft_nullptr || mode_out == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (stat(path, &file_info) == 0)
    {
        *mode_out = file_info.st_mode;
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    error_code = cmp_file_error_to_errno(errno);
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_set_permissions(const char *path, int32_t owner_permissions,
    int32_t group_permissions, int32_t other_permissions, int32_t *error_code_out)
{
    int32_t error_code;
    mode_t mode_value;

    if (path == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (owner_permissions < 0 || owner_permissions > 7
        || group_permissions < 0 || group_permissions > 7
        || other_permissions < 0 || other_permissions > 7)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    mode_value = 0;
    if ((owner_permissions & 4) != 0)
        mode_value |= S_IRUSR;
    if ((owner_permissions & 2) != 0)
        mode_value |= S_IWUSR;
    if ((owner_permissions & 1) != 0)
        mode_value |= S_IXUSR;
    if ((group_permissions & 4) != 0)
        mode_value |= S_IRGRP;
    if ((group_permissions & 2) != 0)
        mode_value |= S_IWGRP;
    if ((group_permissions & 1) != 0)
        mode_value |= S_IXGRP;
    if ((other_permissions & 4) != 0)
        mode_value |= S_IROTH;
    if ((other_permissions & 2) != 0)
        mode_value |= S_IWOTH;
    if ((other_permissions & 1) != 0)
        mode_value |= S_IXOTH;
    if (chmod(path, mode_value) == 0)
    {
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    error_code = cmp_file_error_to_errno(errno);
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

#endif

#if defined(_WIN32) || defined(_WIN64)
int32_t cmp_file_sync(int32_t file_descriptor, int32_t *error_code_out)
{
    HANDLE file_handle;
    int32_t error_code;

    file_handle = cmp_retrieve_handle(file_descriptor);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_HANDLE);
        return (FT_ERR_INVALID_HANDLE);
    }
    if (FlushFileBuffers(file_handle) == 0)
    {
        error_code = cmp_file_error_to_errno(static_cast<int32_t>(GetLastError()));
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_file_sync_directory(const char *path, int32_t *error_code_out)
{
    (void)path;
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}
#else
int32_t cmp_file_sync(int32_t file_descriptor, int32_t *error_code_out)
{
    int32_t error_code;

    if (fsync(file_descriptor) == 0)
    {
        cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    error_code = cmp_file_error_to_errno(errno);
    cmp_set_error_code(error_code_out, error_code);
    return (error_code);
}

int32_t cmp_file_sync_directory(const char *path, int32_t *error_code_out)
{
    int32_t directory_descriptor;
    int32_t error_code;

    if (path == ft_nullptr)
    {
        cmp_set_error_code(error_code_out, FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    directory_descriptor = cmp_open(path, O_DIRECTORY | O_RDONLY, 0);
    if (directory_descriptor < 0)
    {
        error_code = cmp_file_error_to_errno(errno);
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    if (fsync(directory_descriptor) != 0)
    {
        error_code = cmp_file_error_to_errno(errno);
        (void)cmp_close(directory_descriptor);
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    error_code = cmp_close(directory_descriptor);
    if (error_code != FT_ERR_SUCCESS)
    {
        cmp_set_error_code(error_code_out, error_code);
        return (error_code);
    }
    cmp_set_error_code(error_code_out, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}
#endif
