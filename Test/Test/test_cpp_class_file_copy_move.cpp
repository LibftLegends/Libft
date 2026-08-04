#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_file.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <unistd.h>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <fcntl.h>
#ifndef LIBFT_TEST_BUILD
#endif

static int file_copy_move_expect_sigabrt(void (*operation)(void))
{
    return (test_expect_sigabrt_signal(operation));
}

static ft_bool g_ft_file_get_error_returned = FT_FALSE;
static int32_t g_ft_file_get_error_result = FT_ERR_SUCCESS;
static ft_bool g_ft_file_get_error_str_returned = FT_FALSE;
static const char *g_ft_file_get_error_str_result = ft_nullptr;

static void file_copy_move_make_template(char *path_buffer)
{
    const char *template_value;
    ft_size_t index;

    template_value = "/tmp/libft_file_copy_move_XXXXXX";
    index = 0;
    while (template_value[index] != '\0')
    {
        path_buffer[index] = template_value[index];
        index++;
    }
    path_buffer[index] = '\0';
    return ;
}

static void ft_file_get_error_uninitialised_operation(void)
{
    ft_file file;

    g_ft_file_get_error_result = file.get_error();
    g_ft_file_get_error_returned = FT_TRUE;
    return ;
}

static void ft_file_get_error_str_uninitialised_operation(void)
{
    ft_file file;

    g_ft_file_get_error_str_result = file.get_error_str();
    g_ft_file_get_error_str_returned = FT_TRUE;
    return ;
}

FT_TEST(test_cpp_class_file_copy_constructor_preserves_open_handle)
{
    char path_buffer[64];
    char read_buffer[3];
    int32_t template_descriptor;
    ft_file source_file;

    file_copy_move_make_template(path_buffer);
    template_descriptor = test_create_temp_file_from_template(path_buffer,
            sizeof(path_buffer), path_buffer);
    FT_ASSERT(template_descriptor >= 0);
    close(template_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.open(path_buffer, O_RDWR | O_TRUNC));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.enable_thread_safety());
    FT_ASSERT_EQ(2, static_cast<int>(source_file.write_buffer("xy", 2)));

    ft_file copied_file;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_file.move(source_file));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, copied_file._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, copied_file.is_thread_safe());
    FT_ASSERT(copied_file.get_file_descriptor() >= 0);
    FT_ASSERT(source_file.get_file_descriptor() != copied_file.get_file_descriptor());
    FT_ASSERT_EQ(0, copied_file.seek(0, SEEK_SET));
    read_buffer[0] = '\0';
    read_buffer[1] = '\0';
    read_buffer[2] = '\0';
    FT_ASSERT_EQ(2, static_cast<int>(copied_file.read(read_buffer, 2)));
    FT_ASSERT_EQ(0, ft_strcmp(read_buffer, "xy"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_file.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.destroy());
    unlink(path_buffer);
    return (1);
}

FT_TEST(test_cpp_class_file_error_queries_follow_lifecycle_contract)
{
    ft_file file;

    g_ft_file_get_error_returned = FT_FALSE;
    g_ft_file_get_error_result = FT_ERR_SUCCESS;
    g_ft_file_get_error_str_returned = FT_FALSE;
    g_ft_file_get_error_str_result = ft_nullptr;
    FT_ASSERT_EQ(1, file_copy_move_expect_sigabrt(
        ft_file_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_ft_file_get_error_returned);
    FT_ASSERT_EQ(1, file_copy_move_expect_sigabrt(
        ft_file_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_ft_file_get_error_str_returned);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file.get_error());
    FT_ASSERT(ft_strcmp(file.get_error_str(), ft_strerror(FT_ERR_SUCCESS)) == 0);
    return (1);
}

FT_TEST(test_cpp_class_file_move_constructor_preserves_open_handle)
{
    char path_buffer[64];
    char read_buffer[4];
    int32_t template_descriptor;
    ft_file source_file;

    file_copy_move_make_template(path_buffer);
    template_descriptor = test_create_temp_file_from_template(path_buffer,
            sizeof(path_buffer), path_buffer);
    FT_ASSERT(template_descriptor >= 0);
    close(template_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.open(path_buffer, O_RDWR | O_TRUNC));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.enable_thread_safety());
    FT_ASSERT_EQ(3, static_cast<int>(source_file.write_buffer("abc", 3)));

    ft_file moved_file;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_file.move(source_file));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_file._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, moved_file._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, moved_file.is_thread_safe());
    FT_ASSERT(moved_file.get_file_descriptor() >= 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_file.get_error());
    FT_ASSERT_EQ(0, moved_file.seek(0, SEEK_SET));
    read_buffer[0] = '\0';
    read_buffer[1] = '\0';
    read_buffer[2] = '\0';
    read_buffer[3] = '\0';
    FT_ASSERT_EQ(3, static_cast<int>(moved_file.read(read_buffer, 3)));
    FT_ASSERT_EQ(0, ft_strcmp(read_buffer, "abc"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_file.destroy());
    unlink(path_buffer);
    return (1);
}

FT_TEST(test_cpp_class_file_move_into_initialized_destination_preserves_source_thread_safety)
{
    char path_buffer[64];
    char read_buffer[4];
    int32_t template_descriptor;
    ft_file source_file;
    ft_file destination_file;

    file_copy_move_make_template(path_buffer);
    template_descriptor = test_create_temp_file_from_template(path_buffer,
            sizeof(path_buffer), path_buffer);
    FT_ASSERT(template_descriptor >= 0);
    close(template_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.open(path_buffer, O_RDWR | O_TRUNC));
    FT_ASSERT_EQ(3, static_cast<int>(source_file.write_buffer("xyz", 3)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.get_error());
    FT_ASSERT_EQ(FT_FALSE, source_file.is_thread_safe());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.move(source_file));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.get_error());
    FT_ASSERT_EQ(FT_FALSE, destination_file.is_thread_safe());
    FT_ASSERT_EQ(0, destination_file.seek(0, SEEK_SET));
    read_buffer[0] = '\0';
    read_buffer[1] = '\0';
    read_buffer[2] = '\0';
    read_buffer[3] = '\0';
    FT_ASSERT_EQ(3, static_cast<int>(destination_file.read(read_buffer, 3)));
    FT_ASSERT_EQ(0, ft_strcmp(read_buffer, "xyz"));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.destroy());
    unlink(path_buffer);
    return (1);
}

FT_TEST(test_cpp_class_file_copy_constructor_from_destroyed_source_produces_destroyed_destination)
{
    ft_file source_file;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.destroy());

    ft_file copied_file;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_file.move(source_file));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, copied_file._initialised_state);
    FT_ASSERT_EQ(-1, copied_file._file_descriptor);
    FT_ASSERT_EQ(ft_nullptr, copied_file._mutex);
    return (1);
}

FT_TEST(test_cpp_class_file_move_constructor_from_destroyed_source_produces_destroyed_destination)
{
    ft_file source_file;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.destroy());

    ft_file moved_file;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_file.move(source_file));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, moved_file._initialised_state);
    FT_ASSERT_EQ(-1, moved_file._file_descriptor);
    FT_ASSERT_EQ(ft_nullptr, moved_file._mutex);
    return (1);
}

FT_TEST(test_cpp_class_file_move_into_uninitialised_destination_preserves_thread_safety)
{
    char path_buffer[64];
    char read_buffer[4];
    int32_t template_descriptor;
    ft_file source_file;
    ft_file destination_file;

    file_copy_move_make_template(path_buffer);
    template_descriptor = test_create_temp_file_from_template(path_buffer,
            sizeof(path_buffer), path_buffer);
    FT_ASSERT(template_descriptor >= 0);
    close(template_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.open(path_buffer, O_RDWR | O_TRUNC));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.enable_thread_safety());
    FT_ASSERT_EQ(3, static_cast<int>(source_file.write_buffer("uvw", 3)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.move(source_file));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_file._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, destination_file.is_thread_safe());
    FT_ASSERT_EQ(0, destination_file.seek(0, SEEK_SET));
    read_buffer[0] = '\0';
    read_buffer[1] = '\0';
    read_buffer[2] = '\0';
    read_buffer[3] = '\0';
    FT_ASSERT_EQ(3, static_cast<int>(destination_file.read(read_buffer, 3)));
    FT_ASSERT_EQ(0, ft_strcmp(read_buffer, "uvw"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.destroy());
    unlink(path_buffer);
    return (1);
}

FT_TEST(test_cpp_class_file_move_into_destroyed_destination_preserves_thread_safety)
{
    char path_buffer[64];
    char read_buffer[4];
    int32_t template_descriptor;
    ft_file source_file;
    ft_file destination_file;

    file_copy_move_make_template(path_buffer);
    template_descriptor = test_create_temp_file_from_template(path_buffer,
            sizeof(path_buffer), path_buffer);
    FT_ASSERT(template_descriptor >= 0);
    close(template_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.open(path_buffer, O_RDWR | O_TRUNC));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.enable_thread_safety());
    FT_ASSERT_EQ(3, static_cast<int>(source_file.write_buffer("rst", 3)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.move(source_file));
    FT_ASSERT_EQ(FT_TRUE, destination_file.is_thread_safe());
    FT_ASSERT_EQ(0, destination_file.seek(0, SEEK_SET));
    read_buffer[0] = '\0';
    read_buffer[1] = '\0';
    read_buffer[2] = '\0';
    read_buffer[3] = '\0';
    FT_ASSERT_EQ(3, static_cast<int>(destination_file.read(read_buffer, 3)));
    FT_ASSERT_EQ(0, ft_strcmp(read_buffer, "rst"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.destroy());
    unlink(path_buffer);
    return (1);
}

FT_TEST(test_cpp_class_file_move_from_destroyed_source_marks_destination_destroyed)
{
    ft_file source_file;
    ft_file destination_file;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_file.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_file.move(source_file));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination_file._initialised_state);
    FT_ASSERT_EQ(-1, destination_file._file_descriptor);
    FT_ASSERT_EQ(ft_nullptr, destination_file._mutex);
    return (1);
}
