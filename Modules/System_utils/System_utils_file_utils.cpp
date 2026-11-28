#include "system_utils.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Errno/errno.hpp"
#include "../File/open_dir.hpp"
#include <fcntl.h>
#include <filesystem>
#include <utility>
#include "../Basic/limits.hpp"
#if defined(_WIN32) || defined(_WIN64)
# include <io.h>
#endif

static ft_bool su_should_skip_directory_entry(const char *entry_name)
{
    if (entry_name == ft_nullptr)
        return (FT_TRUE);
    if (entry_name[0] == '.' && entry_name[1] == '\0')
        return (FT_TRUE);
    if (entry_name[0] == '.' && entry_name[1] == '.' && entry_name[2] == '\0')
        return (FT_TRUE);
    return (FT_FALSE);
}

static int32_t su_initialize_child_path(const char *parent, char separator, ft_string &path)
{
    const int32_t string_error = path.initialize(parent);
    if (string_error != FT_ERR_SUCCESS)
        return (string_error);
    const ft_size_t length = path.size();
    if (length != 0U && path.c_str()[length - 1U] != separator)
        return (path.append(separator));
    return (FT_ERR_SUCCESS);
}

static int32_t su_set_child_path(ft_string &path, ft_size_t prefix_length, const char *name)
{
    const int32_t string_error = path.resize_length(prefix_length);
    if (string_error != FT_ERR_SUCCESS)
        return (string_error);
    return (path.append(name));
}

static int32_t su_copy_file_descriptors(int32_t source, int32_t destination)
{
    char buffer[32U * 1024U];
    while (1)
    {
        const int64_t bytes_read = su_read(source, buffer, sizeof(buffer));
        if (bytes_read == 0)
            return (0);
        if (bytes_read < 0)
            return (-1);
        if (su_write(destination, buffer, static_cast<ft_size_t>(bytes_read)) != bytes_read)
            return (-1);
    }
}

#if defined(_WIN32) || defined(_WIN64)
static ft_bool su_distinct_disk_files(HANDLE source, HANDLE destination)
{
    FILE_ID_INFO source_id;
    FILE_ID_INFO destination_id;
    if (GetFileInformationByHandleEx(source, FileIdInfo, &source_id, sizeof(source_id)) != 0
        && GetFileInformationByHandleEx(destination, FileIdInfo, &destination_id,
            sizeof(destination_id)) != 0)
    {
        return (source_id.VolumeSerialNumber != destination_id.VolumeSerialNumber
            || ft_memcmp(source_id.FileId.Identifier, destination_id.FileId.Identifier,
                sizeof(source_id.FileId.Identifier)) != 0);
    }
    BY_HANDLE_FILE_INFORMATION source_info;
    BY_HANDLE_FILE_INFORMATION destination_info;
    if (GetFileInformationByHandle(source, &source_info) == 0
        || GetFileInformationByHandle(destination, &destination_info) == 0)
        return (FT_FALSE);
    return (source_info.dwVolumeSerialNumber != destination_info.dwVolumeSerialNumber
        || source_info.nFileIndexHigh != destination_info.nFileIndexHigh
        || source_info.nFileIndexLow != destination_info.nFileIndexLow);
}
#endif

static int32_t su_prepare_copy_destination(int32_t source, int32_t destination)
{
#if defined(_WIN32) || defined(_WIN64)
    const HANDLE output = reinterpret_cast<HANDLE>(_get_osfhandle(destination));
    const DWORD output_type = GetFileType(output);
    if (output_type == FILE_TYPE_PIPE || output_type == FILE_TYPE_CHAR)
        return (0);
    if (output_type != FILE_TYPE_DISK)
        return (-1);
    const HANDLE input = reinterpret_cast<HANDLE>(_get_osfhandle(source));
    if (GetFileType(input) == FILE_TYPE_DISK && su_distinct_disk_files(input, output) == FT_FALSE)
        return (-1);
    if (SetEndOfFile(output) == 0)
        return (-1);
#else
    struct stat output;
    if (fstat(destination, &output) != 0)
        return (-1);
    if (!S_ISREG(output.st_mode))
        return (0);
    struct stat input;
    if (fstat(source, &input) != 0
        || (input.st_dev == output.st_dev && input.st_ino == output.st_ino))
        return (-1);
    if (ftruncate(destination, 0) != 0)
        return (-1);
#endif
    return (0);
}

int32_t su_copy_file(const char *source_path, const char *destination_path)
{
    if (source_path == ft_nullptr || destination_path == ft_nullptr)
        return (-1);
    const int32_t source = su_open(source_path, O_RDONLY);
    if (source < 0)
        return (-1);
    const int32_t destination = su_open(destination_path, O_WRONLY | O_CREAT, 0644);
    if (destination < 0)
    {
        (void)su_close(source);
        return (-1);
    }
    int32_t result = su_prepare_copy_destination(source, destination);
    if (result == 0)
        result = su_copy_file_descriptors(source, destination);
    if (su_close(source) != 0)
        result = -1;
    if (su_close(destination) != 0)
        result = -1;
    return (result);
}

static int32_t su_ensure_directory_exists(const char *path)
{
    int32_t directory_status;
    int32_t directory_exists;
    int32_t error_code;

    directory_exists = 0;
    directory_status = cmp_directory_exists(path, &directory_exists, &error_code);
    if (directory_status == FT_ERR_SUCCESS && directory_exists == 1)
        return (0);
    if (cmp_file_create_directory(path, 0755, &error_code) != 0)
        return (-1);
    return (0);
}

static int32_t su_copy_directory_contents(const char *source_path, const char *destination_path)
{
    file_dirent  *directory_entry;
    int32_t          error_code;
    ft_string source_child;
    ft_string destination_child;

    const char separator = cmp_path_separator();
    if (su_initialize_child_path(source_path, separator, source_child) != FT_ERR_SUCCESS
        || su_initialize_child_path(destination_path, separator, destination_child) != FT_ERR_SUCCESS)
        return (-1);
    const ft_size_t source_prefix = source_child.size();
    const ft_size_t destination_prefix = destination_child.size();
    file_dir *directory_stream = cmp_dir_open(source_path, &error_code);
    if (directory_stream == ft_nullptr)
        return (-1);
    while (1)
    {
        directory_entry = cmp_dir_read(directory_stream, &error_code);
        if (directory_entry == ft_nullptr)
            break ;
        if (su_should_skip_directory_entry(directory_entry->d_name))
            continue ;
        if (su_set_child_path(source_child, source_prefix, directory_entry->d_name) != FT_ERR_SUCCESS
            || su_set_child_path(destination_child, destination_prefix,
                directory_entry->d_name) != FT_ERR_SUCCESS)
            break ;
        const file_type child_type = cmp_dir_entry_type(*directory_entry);
        int32_t child_is_directory = 0;
        if (child_type == FILE_TYPE_DIRECTORY)
            child_is_directory = 1;
        else if (child_type != FILE_TYPE_REGULAR
            && cmp_directory_exists(source_child.c_str(), &child_is_directory, &error_code) != FT_ERR_SUCCESS)
            break ;
        if (child_is_directory == 1)
        {
            if (su_ensure_directory_exists(destination_child.c_str()) != 0)
                break ;
            if (su_copy_directory_contents(source_child.c_str(), destination_child.c_str()) != 0)
                break ;
        }
        else
        {
            if (su_copy_file(source_child.c_str(), destination_child.c_str()) != 0)
                break ;
        }
    }
    if (directory_entry == ft_nullptr && error_code == FT_ERR_SUCCESS)
    {
        if (cmp_dir_close(directory_stream, &error_code) != 0)
            return (-1);
        return (0);
    }
    (void)cmp_dir_close(directory_stream, &error_code);
    return (-1);
}

static ft_bool su_valid_copy_tree_destination(const char *source, const char *destination)
{
    namespace fs = std::filesystem;
    try
    {
        std::error_code error;
        const fs::path source_path(source);
        const fs::path absolute_destination = fs::absolute(destination, error);
        if (error)
            return (FT_FALSE);
        fs::path ancestor = fs::weakly_canonical(absolute_destination, error);
        if (error)
            return (FT_FALSE);
        while (!ancestor.empty())
        {
            if (fs::equivalent(source_path, ancestor, error))
                return (FT_FALSE);
            if (error && error != std::errc::no_such_file_or_directory)
                return (FT_FALSE);
            fs::path parent = ancestor.parent_path();
            if (parent == ancestor)
                break ;
            ancestor = std::move(parent);
        }
        return (FT_TRUE);
    }
    catch (...)
    {
        return (FT_FALSE);
    }
}

int32_t su_copy_directory_recursive(const char *source_path, const char *destination_path)
{
    if (source_path == ft_nullptr || destination_path == ft_nullptr)
        return (-1);
    int32_t source_is_directory = 0;
    int32_t error_code;
    const int32_t source_status = cmp_directory_exists(source_path, &source_is_directory, &error_code);
    if (source_status != FT_ERR_SUCCESS || source_is_directory != 1)
        return (-1);
    if (su_valid_copy_tree_destination(source_path, destination_path) == FT_FALSE)
        return (-1);
    if (su_ensure_directory_exists(destination_path) != 0)
        return (-1);
    if (su_copy_directory_contents(source_path, destination_path) != 0)
        return (-1);
    return (0);
}

int32_t su_inspect_permissions(const char *path, mode_t *permissions_out)
{
    mode_t  permissions;
    int32_t     error_code;

    if (path == ft_nullptr || permissions_out == ft_nullptr)
        return (-1);
    if (cmp_file_get_permissions(path, &permissions, &error_code) != 0)
        return (-1);
    *permissions_out = permissions;
    return (0);
}

int32_t su_chmod(const char *path, int32_t owner_permissions, int32_t group_permissions,
    int32_t other_permissions)
{
    int32_t error_code;

    if (path == ft_nullptr)
        return (-1);
    if (cmp_file_set_permissions(path, owner_permissions, group_permissions,
            other_permissions, &error_code) != FT_ERR_SUCCESS)
        return (-1);
    return (0);
}
