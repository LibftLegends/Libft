#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_file.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static void cleanup_file(const char *path)
{
    if (path == ft_nullptr)
        return ;
    unlink(path);
    return ;
}

FT_TEST(test_ft_file_default_constructor_starts_closed)
{
    ft_file file;

    FT_ASSERT_EQ(-1, file.get_file_descriptor());
    return (1);
}

FT_TEST(test_ft_file_open_close_lifecycle_cycle)
{
    const char *path;
    ft_file file;

    path = "tmp_ft_file_lifecycle_cycle.txt";
    cleanup_file(path);
    FT_ASSERT_EQ(0, file.open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644));
    FT_ASSERT(file.get_file_descriptor() >= 0);
    FT_ASSERT_EQ(5, static_cast<int>(file.write("hello")));
    file.close();
    FT_ASSERT_EQ(-1, file.get_file_descriptor());
    cleanup_file(path);
    return (1);
}

FT_TEST(test_ft_file_reopen_after_close_reads_previous_data)
{
    const char *path;
    ft_file file;
    char buffer[6];

    path = "tmp_ft_file_lifecycle_reopen.txt";
    cleanup_file(path);
    FT_ASSERT_EQ(0, file.open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644));
    FT_ASSERT_EQ(5, static_cast<int>(file.write("world")));
    file.close();
    FT_ASSERT_EQ(0, file.open(path, O_RDONLY));
    std::memset(buffer, 0, sizeof(buffer));
    FT_ASSERT_EQ(5, static_cast<int>(file.read(buffer, 5)));
    FT_ASSERT_EQ(0, std::strcmp(buffer, "world"));
    file.close();
    cleanup_file(path);
    return (1);
}

FT_TEST(test_ft_file_close_when_already_closed_keeps_invalid_fd)
{
    ft_file file;

    file.close();
    FT_ASSERT_EQ(-1, file.get_file_descriptor());
    return (1);
}

FT_TEST(test_ft_file_destructor_closes_descriptor_from_legacy_open)
{
    const char *path = "tmp_ft_file_legacy_open_cleanup.txt";
    cleanup_file(path);
    int32_t file_descriptor = -1;
    {
        ft_file file;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, file.open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644));
        file_descriptor = file.get_file_descriptor();
        FT_ASSERT(file_descriptor >= 0);
    }
    const int32_t close_result = su_close(file_descriptor);
    cleanup_file(path);
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, close_result);
    return (1);
}

FT_TEST(test_ft_file_copy_to_with_buffer_requires_open_source)
{
    const char *destination_path;
    ft_file file;

    destination_path = "tmp_ft_file_lifecycle_copy_invalid.txt";
    cleanup_file(destination_path);
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, file.copy_to_with_buffer(destination_path, 64));
    cleanup_file(destination_path);
    return (1);
}

FT_TEST(test_ft_file_initialize_sets_initialised_state_and_invalid_descriptor)
{
    ft_file file;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.initialize());
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, file._initialised_state);
    FT_ASSERT_EQ(-1, file._file_descriptor);
    FT_ASSERT_EQ(ft_nullptr, file._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.destroy());
    return (1);
}

FT_TEST(test_ft_file_enable_thread_safety_installs_mutex)
{
    ft_file file;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.initialize());
    FT_ASSERT_EQ(ft_nullptr, file._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.enable_thread_safety());
    FT_ASSERT(file._mutex != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, file.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.destroy());
    return (1);
}

FT_TEST(test_ft_file_destroy_clears_mutex_and_descriptor)
{
    const char *path;
    ft_file file;

    path = "tmp_ft_file_destroy_state.txt";
    cleanup_file(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644));
    FT_ASSERT(file._file_descriptor >= 0);
    FT_ASSERT(file._mutex != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.destroy());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, file._initialised_state);
    FT_ASSERT_EQ(-1, file._file_descriptor);
    FT_ASSERT_EQ(ft_nullptr, file._mutex);
    cleanup_file(path);
    return (1);
}

FT_TEST(test_ft_file_reinitialize_after_thread_safe_destroy_resets_members)
{
    const char *path;
    ft_file file;

    path = "tmp_ft_file_reinitialize_state.txt";
    cleanup_file(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.initialize());
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, file._initialised_state);
    FT_ASSERT_EQ(-1, file._file_descriptor);
    FT_ASSERT_EQ(ft_nullptr, file._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.destroy());
    cleanup_file(path);
    return (1);
}

FT_TEST(test_ft_file_close_keeps_object_initialised_for_reuse)
{
    const char *path;
    ft_file file;

    path = "tmp_ft_file_close_keeps_state.txt";
    cleanup_file(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644));
    file.close();
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, file._initialised_state);
    FT_ASSERT_EQ(-1, file._file_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.open(path, O_WRONLY | O_TRUNC));
    FT_ASSERT(file._file_descriptor >= 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.destroy());
    cleanup_file(path);
    return (1);
}
