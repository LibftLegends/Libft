#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_data_buffer.hpp"
#include "../../Modules/CPP_class/class_ofstream.hpp"
#include "../../Modules/CPP_class/class_stringbuf.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include <csetjmp>
#include <unistd.h>
#include <csignal>
#include <cstring>
#if !defined(_WIN32) && !defined(_WIN64)
# include <sys/wait.h>
#endif

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static sigjmp_buf g_lifecycle_abort_jump;
static ft_string g_stringbuf_abort_source_value;
static ft_stringbuf g_stringbuf_abort_buffer_value;
static ft_bool g_ofstream_get_error_returned = FT_FALSE;
static ft_bool g_ofstream_get_error_str_returned = FT_FALSE;

static void lifecycle_abort_handler(int /*signal_number*/)
{
    siglongjmp(g_lifecycle_abort_jump, 1);
    return ;
}

static int lifecycle_expect_sigabrt_signal_handler(void (*operation)(void))
{
    struct sigaction action;
    struct sigaction backup;
    int result;
    int32_t saved_stderr;
    int32_t pipe_read_descriptor;
    int32_t pipe_write_descriptor;
    char output_buffer[8192];

    ft_memset(&action, 0, sizeof(action));
    action.sa_handler = lifecycle_abort_handler;
    sigemptyset(&action.sa_mask);
    result = 0;
    if (sigaction(SIGABRT, &action, &backup) != 0)
        return (0);
    if (test_capture_abort_output_begin(saved_stderr,
            pipe_read_descriptor, pipe_write_descriptor) == 0)
    {
        if (sigaction(SIGABRT, &backup, ft_nullptr) != 0)
            return (0);
        return (0);
    }
    if (sigsetjmp(g_lifecycle_abort_jump, 1) == 0)
    {
        operation();
        result = 0;
    }
    else
        result = 1;
    if (test_capture_abort_output_end(saved_stderr, pipe_read_descriptor,
            pipe_write_descriptor, output_buffer, sizeof(output_buffer)) == 0)
        return (0);
    if (sigaction(SIGABRT, &backup, ft_nullptr) != 0)
        return (0);
    return (result);
}

static void data_buffer_initialize_copy_uninitialised_source_aborts(void)
{
    DataBuffer source_buffer;
    DataBuffer destination_buffer;

    (void)destination_buffer.initialize(source_buffer);
    return ;
}

static void data_buffer_move_uninitialised_source_aborts(void)
{
    DataBuffer source_buffer;
    DataBuffer destination_buffer;

    (void)destination_buffer.move(source_buffer);
    return ;
}

static void stringbuf_initialize_twice_aborts(void)
{
    if (g_stringbuf_abort_source_value.initialize("abc") != FT_ERR_SUCCESS)
        return ;
    if (g_stringbuf_abort_buffer_value.initialize(g_stringbuf_abort_source_value)
        != FT_ERR_SUCCESS)
        return ;
    (void)g_stringbuf_abort_buffer_value.initialize(g_stringbuf_abort_source_value);
    return ;
}

static void ofstream_initialize_twice_aborts(void)
{
    ft_ofstream stream_value;

    if (stream_value.initialize() != FT_ERR_SUCCESS)
        return ;
    (void)stream_value.initialize();
    return ;
}

static void ofstream_get_error_uninitialised_operation(void)
{
    ft_ofstream stream_value;

    (void)stream_value.get_error();
    g_ofstream_get_error_returned = FT_TRUE;
    return ;
}

static void ofstream_get_error_str_uninitialised_operation(void)
{
    ft_ofstream stream_value;

    (void)stream_value.get_error_str();
    g_ofstream_get_error_str_returned = FT_TRUE;
    return ;
}

FT_TEST(test_data_buffer_initialize_copy_into_uninitialised_destination)
{
    DataBuffer source_buffer;
    DataBuffer destination_buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (source_buffer << 42).get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.initialize(source_buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.destroy());
    return (1);
}

FT_TEST(test_data_buffer_initialize_copy_into_destroyed_destination)
{
    DataBuffer source_buffer;
    DataBuffer destination_buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (source_buffer << 7).get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.initialize(source_buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.destroy());
    return (1);
}

FT_TEST(test_data_buffer_move_into_uninitialised_destination)
{
    DataBuffer source_buffer;
    DataBuffer destination_buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (source_buffer << 99).get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.move(source_buffer));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_buffer._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.destroy());
    return (1);
}

FT_TEST(test_data_buffer_move_into_destroyed_destination)
{
    DataBuffer source_buffer;
    DataBuffer destination_buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (source_buffer << 123).get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.move(source_buffer));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_buffer._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.destroy());
    return (1);
}

FT_TEST(test_data_buffer_move_self_is_noop_success)
{
    DataBuffer buffer_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.move(buffer_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.destroy());
    return (1);
}

FT_TEST(test_data_buffer_initialize_copy_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, lifecycle_expect_sigabrt_signal_handler(
        data_buffer_initialize_copy_uninitialised_source_aborts));
    return (1);
}

FT_TEST(test_data_buffer_move_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, lifecycle_expect_sigabrt_signal_handler(
        data_buffer_move_uninitialised_source_aborts));
    return (1);
}

FT_TEST(test_data_buffer_destroy_tolerates_destroyed_instance)
{
    DataBuffer buffer_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.destroy());
    return (1);
}

FT_TEST(test_data_buffer_string_round_trip_preserves_instance_error_success)
{
    DataBuffer buffer_value;
    ft_string write_value;
    ft_string read_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, write_value.initialize("payload"));
    buffer_value << write_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.get_error());
    buffer_value >> read_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_value.get_error());
    FT_ASSERT(read_value == "payload");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.destroy());
    return (1);
}

FT_TEST(test_data_buffer_bulk_records_preserve_encoding_and_copy_cursor)
{
    DataBuffer source;
    DataBuffer copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (source << -17).get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (source << std::string("abc\0def", 7)).get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (source << "").get_error());

    const ft_vector<uint8_t> &bytes = source.data();
    FT_ASSERT_EQ(3 * sizeof(ft_size_t) + 6, bytes.size());
    ft_size_t wire_length = 0;
    ft_memcpy(&wire_length, bytes.begin(), sizeof(wire_length));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), wire_length);
    FT_ASSERT_EQ(0, ft_memcmp(bytes.begin() + sizeof(wire_length), "-17", 3));
    ft_memcpy(&wire_length, bytes.begin() + sizeof(wire_length) + 3, sizeof(wire_length));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), wire_length);
    FT_ASSERT_EQ(0, ft_memcmp(bytes.begin() + 2 * sizeof(wire_length) + 3, "abc", 3));

    int32_t number = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (source >> number).get_error());
    FT_ASSERT_EQ(-17, number);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(source.tell(), copy.tell());
    ft_string text;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (copy >> text).get_error());
    FT_ASSERT(text == "abc");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (copy >> text).get_error());
    FT_ASSERT(text == "");
    FT_ASSERT_EQ(copy.size(), copy.tell());
    return (1);
}

FT_TEST(test_data_buffer_failed_payload_growth_leaves_previous_record_intact)
{
    DataBuffer buffer_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (buffer_value << 19).get_error());
    const ft_size_t original_size = buffer_value.size();
    const std::string large_value(4096, 'x');
    buffer_value._buffer.reserve(original_size + sizeof(ft_size_t));
    cma_set_alloc_limit(1);
    const int32_t write_error = (buffer_value << large_value).get_error();
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, write_error);
    FT_ASSERT_EQ(original_size, buffer_value.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (buffer_value << 23).get_error());
    int32_t number = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (buffer_value >> number).get_error());
    FT_ASSERT_EQ(19, number);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (buffer_value >> number).get_error());
    FT_ASSERT_EQ(23, number);
    return (1);
}

FT_TEST(test_data_buffer_rejects_out_of_bounds_record_lengths)
{
    DataBuffer buffer_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize());
    const ft_size_t invalid_lengths[] = { 1, FT_SYSTEM_SIZE_MAX };
    ft_size_t index = 0;
    while (index < sizeof(invalid_lengths) / sizeof(*invalid_lengths))
    {
        buffer_value.clear();
        FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value._buffer.append(
            reinterpret_cast<const uint8_t *>(&invalid_lengths[index]), sizeof(ft_size_t)));
        int32_t output = 42;
        FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, (buffer_value >> output).get_error());
        FT_ASSERT_EQ(42, output);
        FT_ASSERT_EQ(sizeof(ft_size_t), buffer_value.tell());
        ++index;
    }
    buffer_value.clear();
    int32_t output = 42;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, (buffer_value >> output).get_error());
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), buffer_value.tell());
    return (1);
}

struct data_buffer_custom_value
{
    int32_t number;
};

static ft_istringstream &operator>>(ft_istringstream &input,
    data_buffer_custom_value &output)
{
    return (input >> output.number);
}

FT_TEST(test_data_buffer_custom_extraction_uses_the_stream)
{
    DataBuffer buffer_value;
    data_buffer_custom_value output = { 0 };
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (buffer_value << 123).get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (buffer_value >> output).get_error());
    FT_ASSERT_EQ(123, output.number);
    return (1);
}

FT_TEST(test_data_buffer_move_constructor_preserves_state_and_thread_safety)
{
    DataBuffer source_buffer;
    ft_string write_value;
    ft_string read_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, write_value.initialize("payload"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (source_buffer << write_value).get_error());
    FT_ASSERT_EQ(FT_FALSE, source_buffer.seek(source_buffer.size() + 1));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, source_buffer.get_operation_error());
    FT_ASSERT_EQ(FT_TRUE, source_buffer.seek(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.get_operation_error());
    {
        DataBuffer moved_buffer;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_buffer.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_buffer.move(source_buffer));

        FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_buffer._initialised_state);
        FT_ASSERT_EQ(FT_TRUE, moved_buffer.is_thread_safe());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_buffer.get_operation_error());
        FT_ASSERT_EQ(static_cast<ft_size_t>(0), moved_buffer.tell());
        moved_buffer >> read_value;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_buffer.get_operation_error());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, read_value.get_error());
        FT_ASSERT(read_value == "payload");
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_buffer.destroy());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, write_value.destroy());
    return (1);
}


FT_TEST(test_ft_stringbuf_initialize_destroy_cycle)
{
    ft_string source_value;
    ft_stringbuf buffer_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("abcdef"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize(source_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_ft_stringbuf_initialize_twice_aborts)
{
    FT_ASSERT_EQ(1, lifecycle_expect_sigabrt_signal_handler(
        stringbuf_initialize_twice_aborts));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, g_stringbuf_abort_buffer_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, g_stringbuf_abort_source_value.destroy());
    return (1);
}

FT_TEST(test_ft_ofstream_initialize_destroy_cycle)
{
    ft_ofstream stream_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    return (1);
}

FT_TEST(test_ft_ofstream_initialize_twice_aborts)
{
    FT_ASSERT_EQ(1, lifecycle_expect_sigabrt_signal_handler(
        ofstream_initialize_twice_aborts));
    return (1);
}

FT_TEST(test_ft_ofstream_error_queries_follow_lifecycle_contract)
{
    ft_ofstream stream_value;

    g_ofstream_get_error_returned = FT_FALSE;
    g_ofstream_get_error_str_returned = FT_FALSE;
    FT_ASSERT_EQ(1, lifecycle_expect_sigabrt_signal_handler(
        ofstream_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_ofstream_get_error_returned);
    FT_ASSERT_EQ(1, lifecycle_expect_sigabrt_signal_handler(
        ofstream_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_ofstream_get_error_str_returned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(stream_value.get_error_str(),
        ft_strerror(FT_ERR_SUCCESS)));
    return (1);
}

FT_TEST(test_ft_ofstream_move_constructor_preserves_open_file)
{
    ft_string file_path_storage;
    const char *file_path;
    ft_ofstream source_stream;
    FILE *file_pointer;
    char read_buffer[32];
    size_t bytes_read;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_path_storage.initialize());
#if defined(_WIN32) || defined(_WIN64)
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path(
        "libft_ofstream_move_constructor", "txt", &file_path_storage));
    file_path = file_path_storage.c_str();
#else
    file_path = "/tmp/libft_ofstream_move_constructor.txt";
#endif
    (void)unlink(file_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.open(file_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.enable_thread_safety());
    pt_recursive_mutex *source_mutex = source_stream._mutex;
    FT_ASSERT_EQ(static_cast<ssize_t>(3), source_stream.write("one"));
    {
        ft_ofstream moved_stream;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.initialize());
        pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        const int32_t failed_move = moved_stream.move(source_stream);
        pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS);
        FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, failed_move);
        FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, moved_stream._initialised_state);
        FT_ASSERT_EQ(source_mutex, source_stream._mutex);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.move(source_stream));
        FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_stream._initialised_state);
        FT_ASSERT_EQ(ft_nullptr, source_stream._mutex);
        FT_ASSERT_EQ(source_mutex, moved_stream._mutex);
        FT_ASSERT_EQ(static_cast<ssize_t>(3), moved_stream.write("two"));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.close());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.destroy());
    }
    file_pointer = ft_fopen(file_path, "rb");
    FT_ASSERT(file_pointer != ft_nullptr);
    ft_memset(read_buffer, 0, sizeof(read_buffer));
    bytes_read = fread(read_buffer, 1, sizeof(read_buffer) - 1,
        file_pointer);
    ft_fclose(file_pointer);
    FT_ASSERT_EQ(static_cast<size_t>(6), bytes_read);
    FT_ASSERT_EQ(0, ft_strcmp(read_buffer, "onetwo"));
    (void)unlink(file_path);
#if defined(_WIN32) || defined(_WIN64)
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_path_storage.destroy());
#endif
    return (1);
}

FT_TEST(test_ft_ofstream_close_reports_native_descriptor_failure)
{
    ft_string path;
    ft_ofstream stream;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path("libft_ofstream_close_error", "txt", &path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.open(path.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_close(stream._file.get_file_descriptor()));
    const int32_t result = stream.close();
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, result);
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, stream.get_error());
    FT_ASSERT_EQ(-1, stream._file._file_descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.close());
    (void)unlink(path.c_str());
    return (1);
}

FT_TEST(test_data_buffer_initialize_sets_empty_success_state)
{
    DataBuffer buffer_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize());
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, buffer_value._initialised_state);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), buffer_value.size());
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), buffer_value.tell());
    FT_ASSERT_EQ(FT_TRUE, buffer_value.good());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.destroy());
    return (1);
}

FT_TEST(test_data_buffer_enable_thread_safety_installs_mutex_and_destroy_clears_it)
{
    DataBuffer buffer_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize());
    FT_ASSERT_EQ(ft_nullptr, buffer_value._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.enable_thread_safety());
    FT_ASSERT(buffer_value._mutex != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, buffer_value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.destroy());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, buffer_value._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, buffer_value._mutex);
    return (1);
}

FT_TEST(test_data_buffer_reinitialize_after_destroy_resets_cursor_and_size)
{
    DataBuffer buffer_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (buffer_value << 321).get_error());
    FT_ASSERT(buffer_value.size() > 0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.initialize());
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), buffer_value.size());
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), buffer_value.tell());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_value.destroy());
    return (1);
}

FT_TEST(test_data_buffer_move_destroyed_source_marks_destination_destroyed)
{
    DataBuffer source_buffer;
    DataBuffer destination_buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.move(source_buffer));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination_buffer._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, destination_buffer._mutex);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), destination_buffer._read_pos);
    return (1);
}

FT_TEST(test_data_buffer_move_into_destroyed_destination_preserves_thread_safety_and_payload)
{
    DataBuffer source_buffer;
    DataBuffer destination_buffer;
    ft_string read_value;
    ft_string write_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, write_value.initialize("buffer payload"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, (source_buffer << write_value).get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.move(source_buffer));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_buffer._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, destination_buffer.is_thread_safe());
    destination_buffer >> read_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.get_operation_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_value.get_error());
    FT_ASSERT(read_value == "buffer payload");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, write_value.destroy());
    return (1);
}
