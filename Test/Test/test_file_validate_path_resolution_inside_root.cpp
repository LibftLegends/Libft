#include "../test_internal.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <string>
#include <cstdio>

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
#else
# include <fcntl.h>
# include <unistd.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
# ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#  define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2U
# endif
#endif

FT_TEST(test_file_validate_path_resolution_inside_root_allows_safe_paths)
{
#if defined(_WIN32) || defined(_WIN64)
    char temporary_path_buffer[MAX_PATH + 1];
    DWORD path_length;
    std::string root_path;
    std::string missing_path;

    path_length = GetTempPathA(static_cast<DWORD>(sizeof(temporary_path_buffer)),
            temporary_path_buffer);
    FT_ASSERT(path_length > 0U);
    if (path_length == 0U || path_length > MAX_PATH)
        return (1);
    temporary_path_buffer[path_length] = '\0';
    root_path = std::string(temporary_path_buffer) + "libft_resolution_root_"
        + std::to_string(GetCurrentProcessId());
    missing_path = root_path + "\\new_database.bin";
    (void)RemoveDirectoryA(root_path.c_str());
    FT_ASSERT(CreateDirectoryA(root_path.c_str(), ft_nullptr) != 0);
    if (GetFileAttributesA(root_path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return (1);
#else
    char root_template[] = "/tmp/libft_resolution_root_XXXXXX";
    char *root_path_buffer;
    std::string root_path;
    std::string missing_path;

    root_path_buffer = mkdtemp(root_template);
    FT_ASSERT(root_path_buffer != ft_nullptr);
    if (root_path_buffer == ft_nullptr)
        return (1);
    root_path = root_path_buffer;
    missing_path = root_path + "/new_database.bin";
#endif

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        file_validate_path_resolution_inside_root(root_path.c_str(),
            missing_path.c_str()));

#if defined(_WIN32) || defined(_WIN64)
    (void)RemoveDirectoryA(root_path.c_str());
#else
    (void)rmdir(root_path.c_str());
#endif
    return (1);
}

FT_TEST(test_file_validate_path_resolution_inside_root_rejects_symlink_escape)
{
    std::string root_path;
    std::string outside_path;
    std::string outside_file_path;
    std::string link_path;
    std::string candidate_path;
    ft_bool link_created;

#if defined(_WIN32) || defined(_WIN64)
    char temporary_path_buffer[MAX_PATH + 1];
    DWORD path_length;
    HANDLE file_handle;

    path_length = GetTempPathA(static_cast<DWORD>(sizeof(temporary_path_buffer)),
            temporary_path_buffer);
    FT_ASSERT(path_length > 0U);
    if (path_length == 0U || path_length > MAX_PATH)
        return (1);
    temporary_path_buffer[path_length] = '\0';
    root_path = std::string(temporary_path_buffer) + "libft_resolution_root_"
        + std::to_string(GetCurrentProcessId());
    outside_path = std::string(temporary_path_buffer) + "libft_resolution_outside_"
        + std::to_string(GetCurrentProcessId());
    outside_file_path = outside_path + "\\outside.txt";
    link_path = root_path + "\\escape";
    candidate_path = link_path + "\\outside.txt";
    (void)DeleteFileA(link_path.c_str());
    (void)RemoveDirectoryA(link_path.c_str());
    (void)DeleteFileA(outside_file_path.c_str());
    (void)RemoveDirectoryA(root_path.c_str());
    (void)RemoveDirectoryA(outside_path.c_str());
    if (CreateDirectoryA(root_path.c_str(), ft_nullptr) == 0
        || CreateDirectoryA(outside_path.c_str(), ft_nullptr) == 0)
    {
        (void)RemoveDirectoryA(root_path.c_str());
        (void)RemoveDirectoryA(outside_path.c_str());
        return (1);
    }
    file_handle = CreateFileA(outside_file_path.c_str(), GENERIC_WRITE, 0,
            ft_nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, ft_nullptr);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        (void)RemoveDirectoryA(root_path.c_str());
        (void)RemoveDirectoryA(outside_path.c_str());
        return (1);
    }
    (void)CloseHandle(file_handle);
    link_created = FT_FALSE;
    if (CreateSymbolicLinkA(link_path.c_str(), outside_path.c_str(),
            SYMBOLIC_LINK_FLAG_DIRECTORY
            | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0)
        link_created = FT_TRUE;
#else
    char root_template[] = "/tmp/libft_resolution_root_XXXXXX";
    char outside_template[] = "/tmp/libft_resolution_outside_XXXXXX";
    char *root_path_buffer;
    char *outside_path_buffer;
    int32_t file_descriptor;
    int64_t write_result;

    root_path_buffer = mkdtemp(root_template);
    outside_path_buffer = mkdtemp(outside_template);
    FT_ASSERT(root_path_buffer != ft_nullptr);
    FT_ASSERT(outside_path_buffer != ft_nullptr);
    if (root_path_buffer == ft_nullptr || outside_path_buffer == ft_nullptr)
    {
        if (root_path_buffer != ft_nullptr)
            (void)rmdir(root_path_buffer);
        if (outside_path_buffer != ft_nullptr)
            (void)rmdir(outside_path_buffer);
        return (1);
    }
    root_path = root_path_buffer;
    outside_path = outside_path_buffer;
    outside_file_path = outside_path + "/outside.txt";
    link_path = root_path + "/escape";
    candidate_path = link_path + "/outside.txt";
    file_descriptor = open(outside_file_path.c_str(), O_WRONLY | O_CREAT
            | O_TRUNC, 0600);
    FT_ASSERT(file_descriptor >= 0);
    if (file_descriptor < 0)
    {
        (void)rmdir(root_path.c_str());
        (void)rmdir(outside_path.c_str());
        return (1);
    }
    write_result = static_cast<int64_t>(write(file_descriptor, "x", 1U));
    (void)close(file_descriptor);
    FT_ASSERT_EQ(static_cast<int64_t>(1), write_result);
    link_created = FT_FALSE;
    if (symlink(outside_path.c_str(), link_path.c_str()) == 0)
        link_created = FT_TRUE;
#endif

    if (link_created == FT_FALSE)
    {
        std::fprintf(stderr,
            "[Filesystem test] symlink creation unavailable; escape test skipped\n");
#if defined(_WIN32) || defined(_WIN64)
        (void)DeleteFileA(outside_file_path.c_str());
        (void)RemoveDirectoryA(root_path.c_str());
        (void)RemoveDirectoryA(outside_path.c_str());
#else
        (void)unlink(outside_file_path.c_str());
        (void)rmdir(root_path.c_str());
        (void)rmdir(outside_path.c_str());
#endif
        return (1);
    }
    FT_ASSERT_EQ(FT_ERR_INVALID_PATH,
        file_validate_path_resolution_inside_root(root_path.c_str(),
            candidate_path.c_str()));

#if defined(_WIN32) || defined(_WIN64)
    (void)RemoveDirectoryA(link_path.c_str());
    (void)DeleteFileA(outside_file_path.c_str());
    (void)RemoveDirectoryA(root_path.c_str());
    (void)RemoveDirectoryA(outside_path.c_str());
#else
    (void)unlink(link_path.c_str());
    (void)unlink(outside_file_path.c_str());
    (void)rmdir(root_path.c_str());
    (void)rmdir(outside_path.c_str());
#endif
    return (1);
}
