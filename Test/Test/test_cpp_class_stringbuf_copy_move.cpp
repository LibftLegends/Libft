#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_stringbuf.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int stringbuf_copy_move_expect_sigabrt(void (*operation)(void))
{
    return (test_expect_sigabrt_signal(operation));
}

static ft_bool g_stringbuf_get_error_returned = FT_FALSE;
static ft_bool g_stringbuf_get_error_str_returned = FT_FALSE;

static void stringbuf_get_error_uninitialised_operation(void)
{
    ft_stringbuf buffer;

    (void)buffer.get_error();
    g_stringbuf_get_error_returned = FT_TRUE;
    return ;
}

static void stringbuf_get_error_str_uninitialised_operation(void)
{
    ft_stringbuf buffer;

    (void)buffer.get_error_str();
    g_stringbuf_get_error_str_returned = FT_TRUE;
    return ;
}

FT_TEST(test_cpp_class_stringbuf_error_queries_follow_lifecycle_contract)
{
    ft_string source_string;
    ft_stringbuf buffer;

    g_stringbuf_get_error_returned = FT_FALSE;
    g_stringbuf_get_error_str_returned = FT_FALSE;
    FT_ASSERT_EQ(1, stringbuf_copy_move_expect_sigabrt(
        stringbuf_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_stringbuf_get_error_returned);
    FT_ASSERT_EQ(1, stringbuf_copy_move_expect_sigabrt(
        stringbuf_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_stringbuf_get_error_str_returned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_string.initialize("abcdef"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(source_string));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(buffer.get_error_str(), ft_strerror(FT_ERR_SUCCESS)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_string.destroy());
    return (1);
}

FT_TEST(test_cpp_class_stringbuf_move_constructor_transfers_state)
{
    ft_string source_string;
    ft_string remaining_string;
    ft_stringbuf source_buffer;
    char prefix_buffer[3];
    ssize_t bytes_read;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_string.initialize("abcdef"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, remaining_string.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize(source_string));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.enable_thread_safety());
    pt_recursive_mutex *source_mutex = source_buffer._mutex;
    bytes_read = source_buffer.read(prefix_buffer, 2);
    FT_ASSERT_EQ(2, static_cast<int>(bytes_read));
    prefix_buffer[2] = '\0';
    FT_ASSERT_EQ(0, ft_strcmp(prefix_buffer, "ab"));

    ft_stringbuf moved_buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_buffer.initialize(source_string));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_buffer.move(source_buffer));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_buffer._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, source_buffer._mutex);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, moved_buffer._initialised_state);
    FT_ASSERT_EQ(2U, moved_buffer._position);
    FT_ASSERT_EQ(FT_TRUE, moved_buffer.is_thread_safe());
    FT_ASSERT_EQ(source_mutex, moved_buffer._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_buffer.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_buffer.get_string(remaining_string));
    FT_ASSERT_EQ(0, ft_strcmp(remaining_string.c_str(), "cdef"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize(source_string));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, remaining_string.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_string.destroy());
    return (1);
}
