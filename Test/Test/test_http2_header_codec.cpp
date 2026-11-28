#include "../../Modules/Networking/http2_client.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if NETWORKING_HAS_OPENSSL

FT_TEST(test_http2_header_codec_preserves_exact_bytes)
{
    constexpr unsigned char INPUT[] = {
        0, 2, 0, 3, 'a', 0, 'b', 0, 2, 0xFF, 0x80, 0, 0, 0, 0
    };
    ft_string wire;
    ft_string encoded;
    ft_vector<http2_header_field> headers;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wire.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, headers.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wire.assign(reinterpret_cast<const char *>(INPUT), sizeof(INPUT)));
    int32_t error_code = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_TRUE, http2_decompress_headers(wire, headers, error_code));
    FT_ASSERT_EQ(2U, headers.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, headers[0].enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, http2_compress_headers(headers, encoded, error_code));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(sizeof(INPUT), encoded.size());
    FT_ASSERT_EQ(0, ft_memcmp(INPUT, encoded.c_str(), sizeof(INPUT)));
    return (1);
}

FT_TEST(test_http2_header_codec_rejects_impossible_count_and_truncated_lengths)
{
    constexpr char INVALID_COUNT[] = {0, 2, 0, 0, 0, 0};
    constexpr char INVALID_NAME[] = {0, 1, 0, 3, 'x', 0};
    constexpr char INVALID_VALUE[] = {0, 1, 0, 0, 0, 2, 'x'};
    constexpr char EMPTY[] = {0, 0};
    ft_string wire;
    ft_vector<http2_header_field> headers;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wire.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, headers.initialize());
    int32_t error_code = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wire.assign(INVALID_COUNT, sizeof(INVALID_COUNT)));
    FT_ASSERT_EQ(FT_FALSE, http2_decompress_headers(wire, headers, error_code));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wire.assign(INVALID_NAME, sizeof(INVALID_NAME)));
    FT_ASSERT_EQ(FT_FALSE, http2_decompress_headers(wire, headers, error_code));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wire.assign(INVALID_VALUE, sizeof(INVALID_VALUE)));
    FT_ASSERT_EQ(FT_FALSE, http2_decompress_headers(wire, headers, error_code));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wire.assign(EMPTY, sizeof(EMPTY)));
    FT_ASSERT_EQ(FT_TRUE, http2_decompress_headers(wire, headers, error_code));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(0U, headers.size());
    return (1);
}

FT_TEST(test_http2_header_codec_reports_vector_allocation_failure)
{
    constexpr uint32_t HEADER_COUNT = 64;
    constexpr char INPUT[2U + HEADER_COUNT * 4U] = {0, HEADER_COUNT};
    ft_string wire;
    ft_vector<http2_header_field> headers;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wire.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, headers.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wire.assign(INPUT, sizeof(INPUT)));
    int32_t error_code = FT_ERR_SUCCESS;
    cma_set_alloc_limit(1U);
    const ft_bool success = http2_decompress_headers(wire, headers, error_code);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_FALSE, success);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(0U, headers.size());
    FT_ASSERT_EQ(FT_TRUE, http2_decompress_headers(wire, headers, error_code));
    FT_ASSERT_EQ(HEADER_COUNT, headers.size());
    return (1);
}

FT_TEST(test_http2_header_field_move_transfers_storage)
{
    http2_header_field source;
    http2_header_field destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, source.assign_from_buffers("name", 4U, "a\0b", 3U));
    const char *name_storage = source._name.c_str();
    const char *value_storage = source._value.c_str();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT(name_storage == destination._name.c_str());
    FT_ASSERT(value_storage == destination._value.c_str());
    FT_ASSERT_EQ(3U, destination._value.size());
    FT_ASSERT_EQ(0, ft_memcmp(destination._value.c_str(), "a\0b", 3U));
    return (1);
}

#endif
