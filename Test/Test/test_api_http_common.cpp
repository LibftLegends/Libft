#include "../test_internal.hpp"
#include "../../Modules/API/api_http_common.hpp"
#include "../../Modules/CPP_class/class_big_number.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_api_http_common_bounds_crlf_search)
{
    constexpr char INPUT[] = "abcdefghijklmno\r stray carriage return\r\nnext\r";
    constexpr ft_size_t LENGTH = sizeof(INPUT) - 1U;
    const char *line_end = api_http_find_crlf(INPUT, INPUT + LENGTH);
    FT_ASSERT_EQ(INPUT + 38, line_end);
    FT_ASSERT_EQ(ft_nullptr, api_http_find_crlf(INPUT, line_end + 1));
    FT_ASSERT_EQ(line_end, api_http_find_crlf(INPUT, line_end + 2));
    FT_ASSERT_EQ(ft_nullptr, api_http_find_crlf(line_end + 2, INPUT + LENGTH));
    return (1);
}

FT_TEST(test_api_http_common_rejects_overflow_and_incomplete_numbers)
{
    constexpr char MAX_DECIMAL[] = "9223372036854775807";
    constexpr char OVERFLOW_DECIMAL[] = "9223372036854775808";
    constexpr char MAX_HEX[] = "7fFfFfFfFfFfFfFf;extension";
    constexpr char OVERFLOW_HEX[] = "8000000000000000";
    constexpr char TRAILING[] = "0012x";
    int64_t value = 0;
    FT_ASSERT_EQ(FT_TRUE, api_http_parse_decimal(MAX_DECIMAL, MAX_DECIMAL + sizeof(MAX_DECIMAL) - 1U, value));
    FT_ASSERT_EQ(INT64_MAX, value);
    FT_ASSERT_EQ(FT_FALSE, api_http_parse_decimal(OVERFLOW_DECIMAL,
        OVERFLOW_DECIMAL + sizeof(OVERFLOW_DECIMAL) - 1U, value));
    FT_ASSERT_EQ(FT_FALSE, api_http_parse_decimal(TRAILING, TRAILING + sizeof(TRAILING) - 1U, value));
    FT_ASSERT_EQ(FT_FALSE, api_http_parse_decimal(TRAILING + 1, TRAILING, value));
    FT_ASSERT_EQ(INT64_MAX, value);
    FT_ASSERT_EQ(FT_TRUE, api_http_parse_hex(MAX_HEX, MAX_HEX + sizeof(MAX_HEX) - 1U, value));
    FT_ASSERT_EQ(INT64_MAX, value);
    FT_ASSERT_EQ(FT_FALSE, api_http_parse_hex(OVERFLOW_HEX, OVERFLOW_HEX + sizeof(OVERFLOW_HEX) - 1U, value));
    FT_ASSERT_EQ(INT64_MAX, value);
    return (1);
}

FT_TEST(test_api_http_common_chunk_decoder_preserves_binary_and_trailer_boundary)
{
    constexpr char BODY[] = "3;key=value\r\na\0b\r\n2\r\ncd\r\n0\r\nName: field\r\n\r\nNEXT";
    constexpr char EXPECTED[] = "a\0bcd";
    constexpr ft_size_t BODY_LENGTH = sizeof(BODY) - 1U;
    constexpr ft_size_t CONSUMED = BODY_LENGTH - 4U;
    ft_size_t consumed = 0;
    FT_ASSERT_EQ(FT_TRUE, api_http_chunked_body_complete(BODY, BODY_LENGTH, consumed));
    FT_ASSERT_EQ(CONSUMED, consumed);
    FT_ASSERT_EQ(FT_FALSE, api_http_chunked_body_complete(BODY, CONSUMED - 1U, consumed));
    FT_ASSERT_EQ(0U, consumed);
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("stale"));
    FT_ASSERT_EQ(FT_TRUE, api_http_decode_chunked(BODY, BODY_LENGTH, output, consumed));
    FT_ASSERT_EQ(CONSUMED, consumed);
    FT_ASSERT_EQ(sizeof(EXPECTED) - 1U, output.size());
    FT_ASSERT_EQ(0, ft_memcmp(EXPECTED, output.c_str(), output.size()));
    return (1);
}

FT_TEST(test_api_http_common_chunk_scan_resumes_across_every_byte_boundary)
{
    constexpr char FIRST_BUFFER[] = "3;long-extension=value\r\na\0b\r\n2\r\ncd\r\n0\r\nName: value\r\n\r\nNEXT";
    constexpr char SECOND_BUFFER[] = "3;long-extension=value\r\na\0b\r\n2\r\ncd\r\n0\r\nName: value\r\n\r\nNEXT";
    constexpr ft_size_t COMPLETE_SIZE = sizeof(FIRST_BUFFER) - 5U;
    api_http_chunk_scan scan;
    ft_size_t available = 0;
    while (available < COMPLETE_SIZE)
    {
        const char *buffer = FIRST_BUFFER;
        if (available % 2U != 0)
            buffer = SECOND_BUFFER;
        FT_ASSERT_EQ(FT_FALSE, api_http_chunked_body_complete(buffer, available, scan));
        FT_ASSERT_EQ(FT_FALSE, scan.invalid);
        ++available;
    }
    FT_ASSERT_EQ(FT_TRUE, api_http_chunked_body_complete(SECOND_BUFFER, available, scan));
    FT_ASSERT_EQ(COMPLETE_SIZE, scan.position);
    FT_ASSERT_EQ(FT_TRUE, api_http_chunked_body_complete(FIRST_BUFFER, sizeof(FIRST_BUFFER) - 1U, scan));
    FT_ASSERT_EQ(COMPLETE_SIZE, scan.position);
    return (1);
}

FT_TEST(test_api_http_common_chunk_scan_rejects_malformed_framing)
{
    constexpr char BODY[] = "1\r\na\r\n1\r\nbX\n0\r\n\r\n";
    api_http_chunk_scan scan;
    FT_ASSERT_EQ(FT_FALSE, api_http_chunked_body_complete(BODY, 11U, scan));
    FT_ASSERT_EQ(FT_FALSE, scan.invalid);
    FT_ASSERT_EQ(FT_FALSE, api_http_chunked_body_complete(BODY, sizeof(BODY) - 1U, scan));
    FT_ASSERT_EQ(FT_TRUE, scan.invalid);
    ft_size_t consumed = 1U;
    FT_ASSERT_EQ(FT_FALSE, api_http_chunked_body_complete(BODY, sizeof(BODY) - 1U, consumed));
    FT_ASSERT_EQ(0U, consumed);
    return (1);
}

struct api_http_test_payload_sink
{
    ft_string output;
    uint32_t calls;
    uint32_t fail_on_call;
    const char *direct_value;
    ft_bool saw_direct_value;
};

static ft_bool api_http_test_collect_payload(const char *bytes, ft_size_t length,
    void *context, int32_t &error_code)
{
    auto &sink = *static_cast<api_http_test_payload_sink *>(context);
    ++sink.calls;
    if (sink.calls == sink.fail_on_call)
    {
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    if (bytes == sink.direct_value)
        sink.saw_direct_value = FT_TRUE;
    error_code = sink.output.append(bytes, length);
    return (error_code == FT_ERR_SUCCESS);
}

FT_TEST(test_api_http_common_measures_exact_buffered_json_request)
{
    char group_name[] = "group";
    char key[] = "key";
    char value[] = "value";
    ft_big_number number;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.initialize());
    number.assign("123456789012345678901");
    char numeric_key[] = "numeric";
    json_item items[3]{};
    items[0].key = key;
    items[0].value = value;
    items[0].next = &items[1];
    items[1].next = &items[2];
    items[2].key = numeric_key;
    items[2].is_big_number = FT_TRUE;
    items[2].big_number = &number;
    json_group groups[2]{};
    groups[0].name = group_name;
    groups[0].items = items;
    groups[0].next = &groups[1];
    constexpr char EXPECTED[] = "{\n  \"group\": {\n    \"key\": \"value\",\n    \"\": \"\",\n"
        "    \"numeric\": 123456789012345678901\n  },\n  \"\": {\n  }\n}\n";
    api_http_test_payload_sink sink{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.output.initialize());
    ft_size_t measured = 0;
    int32_t error_code = FT_ERR_INVALID_OPERATION;
    FT_ASSERT_EQ(FT_TRUE, api_http_measure_json_payload(groups, measured));
    FT_ASSERT_EQ(FT_TRUE, api_http_stream_json_payload(groups, api_http_test_collect_payload, &sink, error_code));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(1U, sink.calls);
    FT_ASSERT_EQ(sizeof(EXPECTED) - 1U, measured);
    FT_ASSERT_EQ(measured, sink.output.size());
    FT_ASSERT_EQ(FT_TRUE, sink.output == EXPECTED);
    return (1);
}

FT_TEST(test_api_http_common_flushes_before_sending_large_values_and_propagates_errors)
{
    char large_value[6001];
    ft_memset(large_value, 'x', sizeof(large_value) - 1U);
    large_value[sizeof(large_value) - 1U] = '\0';
    json_item item{};
    item.value = large_value;
    json_group group{};
    group.items = &item;
    api_http_test_payload_sink sink{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.output.initialize());
    sink.direct_value = large_value;
    ft_size_t measured = 0U;
    int32_t error_code = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_TRUE, api_http_measure_json_payload(&group, measured));
    FT_ASSERT_EQ(FT_TRUE, api_http_stream_json_payload(&group, api_http_test_collect_payload, &sink, error_code));
    FT_ASSERT_EQ(FT_TRUE, sink.saw_direct_value);
    FT_ASSERT_EQ(measured, sink.output.size());
    FT_ASSERT_EQ(3U, sink.calls);
    sink.calls = 0;
    sink.fail_on_call = 2U;
    FT_ASSERT_EQ(FT_FALSE, api_http_stream_json_payload(&group, api_http_test_collect_payload, &sink, error_code));
    FT_ASSERT_EQ(FT_ERR_IO, error_code);
    FT_ASSERT_EQ(2U, sink.calls);
    return (1);
}
