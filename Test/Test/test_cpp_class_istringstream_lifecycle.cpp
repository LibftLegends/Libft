#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_istringstream.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int istringstream_expect_sigabrt(void (*operation)(void))
{
    return (test_expect_sigabrt_signal(operation));
}

static ft_bool g_istringstream_get_error_returned = FT_FALSE;
static int32_t g_istringstream_get_error_result = FT_ERR_SUCCESS;
static ft_bool g_istringstream_get_error_str_returned = FT_FALSE;
static const char *g_istringstream_get_error_str_result = ft_nullptr;

static void istringstream_get_error_uninitialised_operation(void)
{
    ft_istringstream stream_value;

    g_istringstream_get_error_result = stream_value.get_error();
    g_istringstream_get_error_returned = FT_TRUE;
    return ;
}

static void istringstream_get_error_str_uninitialised_operation(void)
{
    ft_istringstream stream_value;

    g_istringstream_get_error_str_result = stream_value.get_error_str();
    g_istringstream_get_error_str_returned = FT_TRUE;
    return ;
}

FT_TEST(test_cpp_class_istringstream_initialize_destroy_cycle)
{
    ft_istringstream stream_value;
    ft_string source_value;
    int32_t parsed_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("123"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize(source_value));
    parsed_value = 0;
    stream_value >> parsed_value;
    FT_ASSERT_EQ(123, parsed_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize(source_value));
    parsed_value = 0;
    stream_value >> parsed_value;
    FT_ASSERT_EQ(123, parsed_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istringstream_destroy_is_no_op_after_destroy)
{
    ft_istringstream stream_value;
    ft_string source_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("7"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize(source_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istringstream_error_queries_follow_lifecycle_contract)
{
    ft_istringstream stream_value;
    ft_string source_value;

    g_istringstream_get_error_returned = FT_FALSE;
    g_istringstream_get_error_result = FT_ERR_SUCCESS;
    g_istringstream_get_error_str_returned = FT_FALSE;
    g_istringstream_get_error_str_result = ft_nullptr;
    FT_ASSERT_EQ(1, istringstream_expect_sigabrt(
        istringstream_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_istringstream_get_error_returned);
    FT_ASSERT_EQ(1, istringstream_expect_sigabrt(
        istringstream_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_istringstream_get_error_str_returned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("17"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize(source_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(stream_value.get_error_str(),
        ft_strerror(FT_ERR_SUCCESS)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istringstream_move_transfers_buffer)
{
    ft_istringstream source_stream;
    ft_istringstream destination_stream;
    ft_string source_value;
    int32_t parsed_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("42"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.initialize(source_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stream.move(source_stream));
    parsed_value = 0;
    destination_stream >> parsed_value;
    FT_ASSERT_EQ(42, parsed_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stream.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istringstream_move_constructor_transfers_buffer)
{
    ft_istringstream source_stream;
    ft_string source_value;
    int32_t parsed_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("84"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.initialize(source_value));
    {
        ft_istringstream moved_stream;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.initialize(source_value));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.move(source_stream));
        parsed_value = 0;
        moved_stream >> parsed_value;
        FT_ASSERT_EQ(84, parsed_value);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.get_error());
        FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_stream._initialised_state);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.destroy());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istringstream_move_self_is_noop_and_preserves_content)
{
    ft_istringstream stream_value;
    ft_string source_value;
    ft_string *content_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("self"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize(source_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.move(stream_value));
    content_value = stream_value.get_string();
    FT_ASSERT(content_value != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content_value->get_error());
    FT_ASSERT_EQ(0, ft_strcmp(content_value->c_str(), "self"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content_value->destroy());
    delete content_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istringstream_destroy_clears_buffer_lifecycle_state)
{
    ft_istringstream stream_value;
    ft_string source_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("gone"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize(source_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, stream_value._buffer._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, stream_value._buffer._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istringstream_reinitialize_after_destroy_restores_content)
{
    ft_istringstream stream_value;
    ft_string source_value;
    int32_t parsed_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("901"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize(source_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.initialize(source_value));
    parsed_value = 0;
    stream_value >> parsed_value;
    FT_ASSERT_EQ(901, parsed_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istringstream_move_into_destroyed_destination_transfers_content)
{
    ft_istringstream source_stream;
    ft_istringstream destination_stream;
    ft_string source_value;
    int32_t parsed_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("305"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.initialize(source_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stream.initialize(source_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stream.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stream.move(source_stream));
    parsed_value = 0;
    destination_stream >> parsed_value;
    FT_ASSERT_EQ(305, parsed_value);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_stream._buffer._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stream.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istringstream_get_string_after_move_constructor_matches_source)
{
    ft_istringstream source_stream;
    ft_istringstream moved_stream;
    ft_string source_value;
    ft_string *content_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("alpha beta"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.initialize(source_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.move(source_stream));
    content_value = moved_stream.get_string();
    FT_ASSERT(content_value != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content_value->get_error());
    FT_ASSERT_EQ(0, ft_strcmp(content_value->c_str(), "alpha beta"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content_value->destroy());
    delete content_value;
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_stream._buffer._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istringstream_extraction_preserves_remaining_text_and_cursor)
{
    ft_istringstream input;
    ft_string source;
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize("prefix -42tail"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize(source));
    char prefix[6];
    input.read(prefix, sizeof(prefix));
    FT_ASSERT_EQ(sizeof(prefix), input.gcount());
    FT_ASSERT_EQ(0, ft_memcmp("prefix", prefix, sizeof(prefix)));
    int32_t number = 0;
    input >> number;
    FT_ASSERT_EQ(-42, number);
    input >> output;
    FT_ASSERT(output == " -42tail");
    char next = '\0';
    input.read(&next, 1);
    FT_ASSERT_EQ(' ', next);
    return (1);
}

FT_TEST(test_cpp_class_istringstream_failed_extraction_leaves_outputs_unchanged)
{
    ft_istringstream input;
    ft_string source;
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize("12345678"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("original"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize(source));
    int32_t number = -1;
    cma_set_alloc_limit(1);
    ft_string *owned = input.get_string();
    input >> number;
    input >> output;
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, owned);
    FT_ASSERT_EQ(-1, number);
    FT_ASSERT(output == "original");
    input >> number;
    FT_ASSERT_EQ(12345678, number);
    return (1);
}
