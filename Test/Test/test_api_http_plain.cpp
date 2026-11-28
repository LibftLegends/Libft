#include "../test_internal.hpp"
#include "../../Modules/API/api_http_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

struct api_http_chunk_test_sink
{
    ft_string output;
    uint32_t body_calls;
    uint32_t final_calls;
};

static ft_bool api_http_collect_test_chunk(const char *bytes, ft_size_t length,
    ft_bool is_final_chunk, void *context)
{
    auto &sink = *static_cast<api_http_chunk_test_sink *>(context);
    if (is_final_chunk)
    {
        ++sink.final_calls;
        return (length == 0);
    }
    ++sink.body_calls;
    return (sink.output.append(bytes, length) == FT_ERR_SUCCESS);
}

FT_TEST(test_api_http_stream_chunked_preserves_binary_payload_and_trailer_boundary)
{
    constexpr char FIRST_PART[] = "3\r\na\0";
    constexpr char SECOND_PART[] = "b\r\n2\r\ncd\r\n0\r\nName: value\r";
    constexpr char EXPECTED[] = "a\0bcd";
    api_http_chunk_test_sink sink{};
    api_streaming_handler handler;
    ft_string buffer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handler.initialize());
    handler.set_user_data(&sink);
    handler.set_body_callback(api_http_collect_test_chunk);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append(FIRST_PART, sizeof(FIRST_PART) - 1U));
    int64_t remaining = -1;
    ft_bool trailers = FT_FALSE;
    ft_bool final = FT_FALSE;
    int32_t error = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_TRUE, api_http_stream_process_chunked_buffer(buffer,
        remaining, trailers, final, &handler, error));
    FT_ASSERT_EQ(3, remaining);
    FT_ASSERT_EQ(2U, buffer.size());
    FT_ASSERT_EQ(0U, sink.body_calls);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append(SECOND_PART, sizeof(SECOND_PART) - 1U));
    FT_ASSERT_EQ(FT_TRUE, api_http_stream_process_chunked_buffer(buffer,
        remaining, trailers, final, &handler, error));
    FT_ASSERT_EQ(FT_TRUE, trailers);
    FT_ASSERT_EQ(FT_FALSE, final);
    FT_ASSERT_EQ(FT_TRUE, buffer == "Name: value\r");
    FT_ASSERT_EQ(2U, sink.body_calls);
    FT_ASSERT_EQ(sizeof(EXPECTED) - 1U, sink.output.size());
    FT_ASSERT_EQ(0, ft_memcmp(EXPECTED, sink.output.c_str(), sink.output.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append("\n\r\nNEXT", 7U));
    FT_ASSERT_EQ(FT_TRUE, api_http_stream_process_chunked_buffer(buffer,
        remaining, trailers, final, &handler, error));
    FT_ASSERT_EQ(FT_FALSE, trailers);
    FT_ASSERT_EQ(FT_TRUE, final);
    FT_ASSERT_EQ(FT_TRUE, buffer == "NEXT");
    FT_ASSERT_EQ(FT_TRUE, api_http_stream_process_chunked_buffer(buffer,
        remaining, trailers, final, &handler, error));
    FT_ASSERT_EQ(1U, sink.final_calls);
    FT_ASSERT_EQ(FT_TRUE, buffer == "NEXT");
    return (1);
}

FT_TEST(test_api_http_stream_chunked_validates_delimiter_before_callback)
{
    api_http_chunk_test_sink sink{};
    api_streaming_handler handler;
    ft_string buffer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handler.initialize());
    handler.set_user_data(&sink);
    handler.set_body_callback(api_http_collect_test_chunk);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize("3\r\nabcX\n"));
    int64_t remaining = -1;
    ft_bool trailers = FT_FALSE;
    ft_bool final = FT_FALSE;
    int32_t error = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_FALSE, api_http_stream_process_chunked_buffer(buffer,
        remaining, trailers, final, &handler, error));
    FT_ASSERT_EQ(FT_ERR_IO, error);
    FT_ASSERT_EQ(0U, sink.body_calls);
    FT_ASSERT_EQ(0U, sink.final_calls);
    return (1);
}
