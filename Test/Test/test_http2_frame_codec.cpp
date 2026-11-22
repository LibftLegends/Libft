#include "../../Modules/Networking/http2_client.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if NETWORKING_HAS_OPENSSL

FT_TEST(test_http2_frame_codec_preserves_wire_bytes)
{
    constexpr unsigned char EXPECTED[] = {
        0, 0, 4, 0, 1, 0x12, 0x34, 0x56, 0x78, 0, 0xFF, 0x80, 0x22
    };
    constexpr unsigned char INPUT[] = {
        0x99, 0, 0, 4, 0, 1, 0x92, 0x34, 0x56, 0x78, 0, 0xFF, 0x80, 0x22,
        0, 0, 0, 4, 1, 0, 0, 0, 0
    };
    http2_frame frame;
    ft_string payload;
    ft_string encoded;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.initialize());
    int32_t error_code = FT_ERR_SUCCESS;
    ft_size_t offset = 1;
    FT_ASSERT_EQ(FT_TRUE, http2_decode_frame(INPUT, sizeof(INPUT), offset, frame, error_code));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(sizeof(EXPECTED) + 1U, offset);
    FT_ASSERT_EQ(FT_TRUE, http2_encode_frame(frame, encoded, error_code));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(sizeof(EXPECTED), encoded.size());
    FT_ASSERT_EQ(0, ft_memcmp(encoded.c_str(), EXPECTED, sizeof(EXPECTED)));
    FT_ASSERT_EQ(FT_TRUE, http2_decode_frame(INPUT, sizeof(INPUT), offset, frame, error_code));
    FT_ASSERT_EQ(sizeof(INPUT), offset);
    FT_ASSERT_EQ(FT_TRUE, frame.copy_payload(payload));
    FT_ASSERT_EQ(0U, payload.size());
    FT_ASSERT_EQ(FT_TRUE, http2_encode_frame(frame, encoded, error_code));
    FT_ASSERT_EQ(9U, encoded.size());
    FT_ASSERT_EQ(0, ft_memcmp(encoded.c_str(), INPUT + sizeof(INPUT) - 9U, 9U));
    return (1);
}

FT_TEST(test_http2_frame_codec_truncated_input_preserves_frame_and_offset)
{
    constexpr unsigned char INPUT[] = {0, 0, 3, 0, 1, 0, 0, 0, 1, 'n', 'e'};
    http2_frame frame;
    ft_string payload;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.initialize());
    FT_ASSERT_EQ(FT_TRUE, frame.set_payload_from_buffer("old", 3U));
    FT_ASSERT_EQ(FT_TRUE, frame.set_type(5U));
    FT_ASSERT_EQ(FT_TRUE, frame.set_flags(9U));
    FT_ASSERT_EQ(FT_TRUE, frame.set_stream_identifier(42U));
    ft_size_t offset = 0;
    int32_t error_code = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_FALSE, http2_decode_frame(INPUT, sizeof(INPUT), offset, frame, error_code));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, error_code);
    FT_ASSERT_EQ(0U, offset);
    uint8_t type = 0;
    uint8_t flags = 0;
    uint32_t stream_identifier = 0;
    FT_ASSERT_EQ(FT_TRUE, frame.get_type(type));
    FT_ASSERT_EQ(FT_TRUE, frame.get_flags(flags));
    FT_ASSERT_EQ(FT_TRUE, frame.get_stream_identifier(stream_identifier));
    FT_ASSERT_EQ(5U, type);
    FT_ASSERT_EQ(9U, flags);
    FT_ASSERT_EQ(42U, stream_identifier);
    FT_ASSERT_EQ(FT_TRUE, frame.copy_payload(payload));
    FT_ASSERT_EQ(FT_TRUE, payload == "old");
    return (1);
}

#endif
