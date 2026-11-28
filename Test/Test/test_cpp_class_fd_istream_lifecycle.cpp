#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_fd_istream.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <unistd.h>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int fd_istream_expect_sigabrt(void (*operation)(void))
{
    return (test_expect_sigabrt_signal(operation));
}

static ft_bool g_fd_istream_get_error_returned = FT_FALSE;
static int32_t g_fd_istream_get_error_result = FT_ERR_SUCCESS;
static ft_bool g_fd_istream_get_error_str_returned = FT_FALSE;
static const char *g_fd_istream_get_error_str_result = ft_nullptr;

static int32_t create_pipe_descriptors(int &read_descriptor, int &write_descriptor)
{
    int descriptors[2];

    if (pipe(descriptors) != 0)
        return (FT_ERR_INVALID_HANDLE);
    read_descriptor = descriptors[0];
    write_descriptor = descriptors[1];
    return (FT_ERR_SUCCESS);
}

static void fd_istream_get_error_uninitialised_operation(void)
{
    ft_fd_istream stream_value;

    g_fd_istream_get_error_result = stream_value.get_error();
    g_fd_istream_get_error_returned = FT_TRUE;
    return ;
}

static void fd_istream_get_error_str_uninitialised_operation(void)
{
    ft_fd_istream stream_value;

    g_fd_istream_get_error_str_result = stream_value.get_error_str();
    g_fd_istream_get_error_str_returned = FT_TRUE;
    return ;
}

FT_TEST(test_cpp_class_fd_istream_lifecycle_read_destroy_reinitialize)
{
    int read_descriptor;
    int write_descriptor;
    char buffer[2];
    ft_fd_istream stream_value;

    read_descriptor = -1;
    write_descriptor = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, create_pipe_descriptors(read_descriptor, write_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    stream_value.set_file_descriptor(read_descriptor);
    FT_ASSERT_EQ(1, write(write_descriptor, "x", 1));
    buffer[0] = '\0';
    buffer[1] = '\0';
    FT_ASSERT_EQ(1, static_cast<int>(stream_value.read(buffer, 1)));
    FT_ASSERT_EQ('x', buffer[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    stream_value.set_file_descriptor(read_descriptor);
    FT_ASSERT_EQ(1, write(write_descriptor, "y", 1));
    buffer[0] = '\0';
    buffer[1] = '\0';
    FT_ASSERT_EQ(1, static_cast<int>(stream_value.read(buffer, 1)));
    FT_ASSERT_EQ('y', buffer[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    close(read_descriptor);
    close(write_descriptor);
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_thread_safety_toggle_is_explicit)
{
    int read_descriptor;
    int write_descriptor;
    ft_fd_istream stream_value;

    read_descriptor = -1;
    write_descriptor = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, create_pipe_descriptors(read_descriptor, write_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    stream_value.set_file_descriptor(read_descriptor);
    FT_ASSERT_EQ(FT_FALSE, stream_value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, stream_value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, stream_value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    close(read_descriptor);
    close(write_descriptor);
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_destroy_is_no_op_for_destroyed_state)
{
    ft_fd_istream stream_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_error_queries_follow_lifecycle_contract)
{
    int read_descriptor;
    int write_descriptor;
    ft_fd_istream stream_value;

    read_descriptor = -1;
    write_descriptor = -1;
    g_fd_istream_get_error_returned = FT_FALSE;
    g_fd_istream_get_error_result = FT_ERR_SUCCESS;
    g_fd_istream_get_error_str_returned = FT_FALSE;
    g_fd_istream_get_error_str_result = ft_nullptr;
    FT_ASSERT_EQ(1, fd_istream_expect_sigabrt(
        fd_istream_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_fd_istream_get_error_returned);
    FT_ASSERT_EQ(1, fd_istream_expect_sigabrt(
        fd_istream_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_fd_istream_get_error_str_returned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, create_pipe_descriptors(read_descriptor, write_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    stream_value.set_file_descriptor(read_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(stream_value.get_error_str(),
        ft_strerror(FT_ERR_SUCCESS)));
    close(read_descriptor);
    close(write_descriptor);
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_copy_constructor_preserves_readability)
{
    int read_descriptor;
    int write_descriptor;
    char buffer[2];
    ft_fd_istream source_stream;

    read_descriptor = -1;
    write_descriptor = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, create_pipe_descriptors(read_descriptor, write_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.initialize());
    source_stream.set_file_descriptor(read_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.enable_thread_safety());

    ft_fd_istream copied_stream;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_stream.move(source_stream));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, copied_stream._initialised_state);
    FT_ASSERT_EQ(read_descriptor, copied_stream.get_file_descriptor());
    FT_ASSERT_EQ(FT_TRUE, copied_stream.is_thread_safe());
    FT_ASSERT_EQ(1, write(write_descriptor, "c", 1));
    buffer[0] = '\0';
    buffer[1] = '\0';
    FT_ASSERT_EQ(1, static_cast<int>(copied_stream.read(buffer, 1)));
    FT_ASSERT_EQ('c', buffer[0]);
    FT_ASSERT_EQ(1U, copied_stream.gcount());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_stream.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_stream.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.destroy());
    close(read_descriptor);
    close(write_descriptor);
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_move_constructor_preserves_readability)
{
    int read_descriptor;
    int write_descriptor;
    char buffer[2];
    ft_fd_istream source_stream;

    read_descriptor = -1;
    write_descriptor = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, create_pipe_descriptors(read_descriptor, write_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.initialize());
    source_stream.set_file_descriptor(read_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.enable_thread_safety());

    ft_fd_istream moved_stream;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.ft_istream::enable_thread_safety());
    pt_recursive_mutex *descriptor_mutex = source_stream._mutex;
    pt_recursive_mutex *base_mutex = source_stream.ft_istream::_mutex;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.move(source_stream));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_stream._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, source_stream._mutex);
    FT_ASSERT_EQ(ft_nullptr, source_stream.ft_istream::_mutex);
    FT_ASSERT_EQ(descriptor_mutex, moved_stream._mutex);
    FT_ASSERT_EQ(base_mutex, moved_stream.ft_istream::_mutex);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, moved_stream._initialised_state);
    FT_ASSERT_EQ(read_descriptor, moved_stream.get_file_descriptor());
    FT_ASSERT_EQ(FT_TRUE, moved_stream.is_thread_safe());
    FT_ASSERT_EQ(1, write(write_descriptor, "m", 1));
    buffer[0] = '\0';
    buffer[1] = '\0';
    FT_ASSERT_EQ(1, static_cast<int>(moved_stream.read(buffer, 1)));
    FT_ASSERT_EQ('m', buffer[0]);
    FT_ASSERT_EQ(1U, moved_stream.gcount());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.destroy());
    close(read_descriptor);
    close(write_descriptor);
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_failed_descriptor_lock_keeps_source_for_retry)
{
    ft_fd_istream source;
    ft_fd_istream destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_file_descriptor(17);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    pt_recursive_mutex *descriptor_mutex = source._mutex;

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED);
    const int32_t result = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source.ft_istream::_initialised_state);
    FT_ASSERT_EQ(descriptor_mutex, source._mutex);
    FT_ASSERT_EQ(17, source.get_file_descriptor());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(descriptor_mutex, destination._mutex);
    FT_ASSERT_EQ(17, destination.get_file_descriptor());
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_default_constructor_sets_uninitialised_state)
{
    ft_fd_istream stream_value;

    FT_ASSERT_EQ(FT_CLASS_STATE_UNINITIALISED, stream_value._initialised_state);
    FT_ASSERT_EQ(-1, stream_value._file_descriptor);
    FT_ASSERT_EQ(ft_nullptr, stream_value._mutex);
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_initialize_sets_initialised_state_and_default_descriptor)
{
    ft_fd_istream stream_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, stream_value._initialised_state);
    FT_ASSERT_EQ(-1, stream_value._file_descriptor);
    FT_ASSERT_EQ(ft_nullptr, stream_value._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_destroy_clears_mutex_and_descriptor)
{
    int read_descriptor;
    int write_descriptor;
    ft_fd_istream stream_value;

    read_descriptor = -1;
    write_descriptor = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, create_pipe_descriptors(read_descriptor, write_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    stream_value.set_file_descriptor(read_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.enable_thread_safety());
    FT_ASSERT(stream_value._mutex != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, stream_value._initialised_state);
    FT_ASSERT_EQ(-1, stream_value._file_descriptor);
    FT_ASSERT_EQ(ft_nullptr, stream_value._mutex);
    close(read_descriptor);
    close(write_descriptor);
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_reinitialize_after_thread_safe_destroy_resets_members)
{
    int read_descriptor;
    int write_descriptor;
    ft_fd_istream stream_value;

    read_descriptor = -1;
    write_descriptor = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, create_pipe_descriptors(read_descriptor, write_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    stream_value.set_file_descriptor(read_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, stream_value._initialised_state);
    FT_ASSERT_EQ(-1, stream_value._file_descriptor);
    FT_ASSERT_EQ(ft_nullptr, stream_value._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    close(read_descriptor);
    close(write_descriptor);
    return (1);
}

FT_TEST(test_cpp_class_fd_istream_copy_and_move_from_destroyed_source_produce_destroyed_destination)
{
    ft_fd_istream source_stream;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.destroy());
    {
        ft_fd_istream copied_stream;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_stream.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_stream.move(source_stream));
        FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, copied_stream._initialised_state);
        FT_ASSERT_EQ(-1, copied_stream._file_descriptor);
        FT_ASSERT_EQ(ft_nullptr, copied_stream._mutex);
    }
    {
        ft_fd_istream moved_stream;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.move(source_stream));
        FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, moved_stream._initialised_state);
        FT_ASSERT_EQ(-1, moved_stream._file_descriptor);
        FT_ASSERT_EQ(ft_nullptr, moved_stream._mutex);
    }
    return (1);
}
