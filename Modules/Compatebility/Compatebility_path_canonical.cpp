#include "compatebility_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if !defined(_WIN32) && !defined(_WIN64)
# include <cstdlib>
#endif

static char *cmp_path_copy_canonical(const char *source, ft_size_t length)
{
    char *output = static_cast<char *>(cma_malloc(length + 1));
    if (output == ft_nullptr)
        return (ft_nullptr);
    (void)ft_memcpy(output, source, length + 1);
    cmp_normalize_slashes(output);
    return (output);
}

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>

int32_t cmp_path_canonical(const char *path, char **output_path)
{
    HANDLE path_handle;
    DWORD written_size;
    DWORD path_error;
    char *canonical_path;
    char resolved_path[32768];

    if (output_path == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    *output_path = ft_nullptr;
    if (path == ft_nullptr || path[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    path_handle = CreateFileA(path, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (path_handle == INVALID_HANDLE_VALUE)
        return (cmp_file_error_to_errno(static_cast<int32_t>(GetLastError())));
    written_size = GetFinalPathNameByHandleA(path_handle, resolved_path,
        static_cast<DWORD>(sizeof(resolved_path)), FILE_NAME_NORMALIZED);
    path_error = GetLastError();
    CloseHandle(path_handle);
    if (written_size == 0)
        return (cmp_file_error_to_errno(static_cast<int32_t>(path_error)));
    if (written_size >= sizeof(resolved_path))
        return (FT_ERR_PATH_TOO_LONG);
    canonical_path = cmp_path_copy_canonical(resolved_path, written_size);
    if (canonical_path == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    *output_path = canonical_path;
    return (FT_ERR_SUCCESS);
}
#else
int32_t cmp_path_canonical(const char *path, char **output_path)
{
    char *system_path;
    char *canonical_path;

    if (output_path == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    *output_path = ft_nullptr;
    if (path == ft_nullptr || path[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    system_path = realpath(path, ft_nullptr);
    if (system_path == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    canonical_path = cmp_path_copy_canonical(system_path, ft_strlen_size_t(system_path));
    free(system_path);
    if (canonical_path == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    *output_path = canonical_path;
    return (FT_ERR_SUCCESS);
}
#endif
