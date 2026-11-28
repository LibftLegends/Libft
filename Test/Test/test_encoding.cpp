#include "../test_internal.hpp"
#include "../../Modules/Encoding/encoding.hpp"
#include "../../Modules/Encoding/encoding_internal.hpp"
#include "../../Modules/Compression/compression.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_encoding_base64url_caller_buffer_checks_capacity)
{
    const uint8_t bytes[] = {0xfbU, 0xffU};
    char encoded[4] = {'x', 'x', 'x', '\0'};
    uint8_t decoded[3] = {0xa5U, 0xa5U, 0xa5U};
    ft_size_t decoded_size = 1U;

    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, encoding_base64url_encode_into(bytes,
        sizeof(bytes), FT_FALSE, encoded, sizeof(encoded) - 1U));
    FT_ASSERT_EQ(0, ft_strcmp(encoded, "xxx"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoding_base64url_encode_into(bytes,
        sizeof(bytes), FT_FALSE, encoded, sizeof(encoded)));
    FT_ASSERT_EQ(0, ft_strcmp(encoded, "-_8"));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        encoding_base64url_decode_into(encoded, 3U, decoded, 1U, &decoded_size));
    FT_ASSERT_EQ(0U, decoded_size);
    FT_ASSERT_EQ(0xa5U, decoded[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        encoding_base64url_decode_into("-_8=", 4U, decoded, 2U, &decoded_size));
    FT_ASSERT_EQ(2U, decoded_size);
    FT_ASSERT_EQ(0, ft_memcmp(bytes, decoded, sizeof(bytes)));
    FT_ASSERT_EQ(0xa5U, decoded[2]);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        encoding_base64url_decode_into("+/8=", 4U, decoded, 2U, &decoded_size));
    return (1);
}

FT_TEST(test_encoding_base64_roundtrip)
{
    const uint8_t input[] = {'h', 'e', 'l', 'l', 'o'};
    char *encoded;
    uint8_t *decoded;
    ft_size_t decoded_size;

    decoded_size = 0;
    encoded = encoding_base64_encode(input, 5);
    FT_ASSERT(encoded != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(encoded, "aGVsbG8="));
    decoded = encoding_base64_decode(encoded, ft_strlen_size_t(encoded),
            &decoded_size);
    FT_ASSERT(decoded != ft_nullptr);
    FT_ASSERT_EQ(5, decoded_size);
    FT_ASSERT_EQ(0, ft_memcmp(input, decoded, decoded_size));
    cma_free(encoded);
    cma_free(decoded);
    return (1);
}

FT_TEST(test_encoding_and_compression_base64_agree_on_binary_payloads)
{
    uint8_t input[260];
    uint32_t random_state = 63231U;
    ft_size_t index = 0;
    while (index < sizeof(input))
    {
        random_state = random_state * 1664525U + 1013904223U;
        input[index] = static_cast<uint8_t>(random_state >> 24U);
        ++index;
    }
    ft_size_t input_size = 1;
    while (input_size <= sizeof(input))
    {
        std::size_t compression_size = 0;
        unsigned char *compression_encoded = ft_base64_encode(input, input_size, &compression_size);
        char *encoding_encoded = encoding_base64_encode(input, input_size);
        FT_ASSERT(compression_encoded != ft_nullptr);
        FT_ASSERT(encoding_encoded != ft_nullptr);
        FT_ASSERT_EQ(compression_size, ft_strlen_size_t(encoding_encoded));
        FT_ASSERT_EQ(0, ft_memcmp(compression_encoded, encoding_encoded, compression_size));
        ft_size_t encoding_decoded_size = 0;
        std::size_t compression_decoded_size = 0;
        uint8_t *encoding_decoded = encoding_base64_decode(
            reinterpret_cast<const char *>(compression_encoded), compression_size, &encoding_decoded_size);
        unsigned char *compression_decoded = ft_base64_decode(
            reinterpret_cast<const unsigned char *>(encoding_encoded), compression_size, &compression_decoded_size);
        FT_ASSERT(encoding_decoded != ft_nullptr);
        FT_ASSERT(compression_decoded != ft_nullptr);
        FT_ASSERT_EQ(input_size, encoding_decoded_size);
        FT_ASSERT_EQ(input_size, compression_decoded_size);
        FT_ASSERT_EQ(0, ft_memcmp(input, encoding_decoded, input_size));
        FT_ASSERT_EQ(0, ft_memcmp(input, compression_decoded, input_size));
        cma_free(compression_decoded);
        cma_free(encoding_decoded);
        cma_free(encoding_encoded);
        cma_free(compression_encoded);
        ++input_size;
    }
    return (1);
}

FT_TEST(test_encoding_base64url_without_padding)
{
    const uint8_t input[] = {0xFB, 0xEF, 0xFF};
    char *encoded;
    uint8_t *decoded;
    ft_size_t decoded_size;

    decoded_size = 0;
    encoded = encoding_base64url_encode(input, 3, FT_FALSE);
    FT_ASSERT(encoded != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(encoded, "--__"));
    decoded = encoding_base64url_decode(encoded, ft_strlen_size_t(encoded),
            &decoded_size);
    FT_ASSERT(decoded != ft_nullptr);
    FT_ASSERT_EQ(3, decoded_size);
    FT_ASSERT_EQ(0, ft_memcmp(input, decoded, decoded_size));
    cma_free(encoded);
    cma_free(decoded);
    return (1);
}

FT_TEST(test_encoding_base64url_decode_rejects_standard_alphabet)
{
    uint8_t *decoded;
    ft_size_t decoded_size;

    decoded_size = 0;
    decoded = encoding_base64url_decode("+/==", 4, &decoded_size);
    FT_ASSERT(decoded == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, encoding_get_error());
    FT_ASSERT_EQ(0, decoded_size);
    return (1);
}

FT_TEST(test_encoding_base64url_mixed_full_groups_and_short_tails)
{
    const uint8_t input[] = {0x00U, 0xFFU, 0xFBU, 0xFFU, 0xFFU};
    const char *const unpadded[] = {"AP_7", "AP_7_w", "AP_7__8"};
    const char *const padded[] = {"AP_7", "AP_7_w==", "AP_7__8="};
    ft_size_t length = 3U;
    while (length <= sizeof(input))
    {
        char *short_text = encoding_base64url_encode(input, length, FT_FALSE);
        char *padded_text = encoding_base64url_encode(input, length, FT_TRUE);
        FT_ASSERT(short_text != ft_nullptr);
        FT_ASSERT(padded_text != ft_nullptr);
        FT_ASSERT_EQ(0, ft_strcmp(short_text, unpadded[length - 3U]));
        FT_ASSERT_EQ(0, ft_strcmp(padded_text, padded[length - 3U]));
        ft_size_t decoded_size = 0U;
        uint8_t *decoded = encoding_base64url_decode(short_text,
                ft_strlen_size_t(short_text), &decoded_size);
        FT_ASSERT(decoded != ft_nullptr);
        FT_ASSERT_EQ(length, decoded_size);
        FT_ASSERT_EQ(0, ft_memcmp(input, decoded, decoded_size));
        cma_free(decoded);
        cma_free(padded_text);
        cma_free(short_text);
        ++length;
    }
    return (1);
}

FT_TEST(test_encoding_base64_rejects_non_ascii_and_excess_padding)
{
    const char non_ascii[] = {'A', 'A', static_cast<char>(0xFF), '='};
    ft_size_t decoded_size = 10U;
    FT_ASSERT(encoding_base64_decode(non_ascii, sizeof(non_ascii), &decoded_size)
        == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, encoding_get_error());
    FT_ASSERT_EQ(0U, decoded_size);
    FT_ASSERT(encoding_base64_decode("AAAAA===", 8U, &decoded_size) == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, encoding_get_error());
    return (1);
}

FT_TEST(test_encoding_hex_roundtrip)
{
    const uint8_t input[] = {0x00, 0xAB, 0xCD, 0xEF};
    char *encoded;
    uint8_t *decoded;
    ft_size_t decoded_size;

    decoded_size = 0;
    encoded = encoding_hex_encode(input, 4, FT_TRUE);
    FT_ASSERT(encoded != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(encoded, "00ABCDEF"));
    decoded = encoding_hex_decode(encoded, ft_strlen_size_t(encoded),
            &decoded_size);
    FT_ASSERT(decoded != ft_nullptr);
    FT_ASSERT_EQ(4, decoded_size);
    FT_ASSERT_EQ(0, ft_memcmp(input, decoded, decoded_size));
    cma_free(encoded);
    cma_free(decoded);
    return (1);
}

FT_TEST(test_encoding_hex_decode_rejects_invalid_input)
{
    uint8_t *decoded;
    ft_size_t decoded_size;

    decoded_size = 0;
    decoded = encoding_hex_decode("0G", 2, &decoded_size);
    FT_ASSERT(decoded == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, encoding_get_error());
    FT_ASSERT_EQ(0, decoded_size);
    return (1);
}

FT_TEST(test_encoding_percent_roundtrip)
{
    const uint8_t input[] = {'a', ' ', '/', '~'};
    char *encoded;
    uint8_t *decoded;
    ft_size_t decoded_size;

    decoded_size = 0;
    encoded = encoding_percent_encode(input, 4);
    FT_ASSERT(encoded != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(encoded, "a%20%2F~"));
    decoded = encoding_percent_decode(encoded, ft_strlen_size_t(encoded),
            &decoded_size);
    FT_ASSERT(decoded != ft_nullptr);
    FT_ASSERT_EQ(4, decoded_size);
    FT_ASSERT_EQ(0, ft_memcmp(input, decoded, decoded_size));
    cma_free(encoded);
    cma_free(decoded);
    return (1);
}

FT_TEST(test_encoding_percent_decode_rejects_short_escape)
{
    uint8_t *decoded;
    ft_size_t decoded_size;

    decoded_size = 0;
    decoded = encoding_percent_decode("abc%", 4, &decoded_size);
    FT_ASSERT(decoded == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, encoding_get_error());
    FT_ASSERT_EQ(0, decoded_size);
    return (1);
}

FT_TEST(test_encoding_percent_decode_preserves_literal_and_escaped_binary_bytes)
{
    const char input[] = {'a', '+', '\0', '%', '0', '0', '%', 'f', 'F', '~'};
    const uint8_t expected[] = {'a', '+', 0U, 0U, 0xFFU, '~'};
    ft_size_t decoded_size = 0U;
    uint8_t *decoded = encoding_percent_decode(input, sizeof(input), &decoded_size);
    FT_ASSERT(decoded != ft_nullptr);
    FT_ASSERT_EQ(sizeof(expected), decoded_size);
    FT_ASSERT_EQ(0, ft_memcmp(decoded, expected, decoded_size));
    cma_free(decoded);
    return (1);
}

FT_TEST(test_encoding_utf8_validate_accepts_valid_text)
{
    const char *input;

    input = "hello " "\xF0\x9F\x99\x82";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoding_utf8_validate(input,
        ft_strlen_size_t(input)));
    return (1);
}

FT_TEST(test_encoding_utf8_validate_rejects_invalid_text)
{
    const char invalid_input[] = {'a', static_cast<char>(0xC0),
        static_cast<char>(0xAF), '\0'};

    FT_ASSERT(encoding_utf8_validate(invalid_input, 3) != FT_ERR_SUCCESS);
    FT_ASSERT(encoding_get_error() != FT_ERR_SUCCESS);
    return (1);
}

FT_TEST(test_encoding_utf8_validate_keeps_span_and_error_contract)
{
    const char input[] = {'A', '\0', static_cast<char>(0xC3), static_cast<char>(0xA9)};
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, encoding_utf8_validate(ft_nullptr, 1U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, encoding_utf8_validate(input, 3U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, encoding_get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoding_utf8_validate(input, sizeof(input)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoding_utf8_validate(ft_nullptr, 0U));
    return (1);
}

FT_TEST(test_encoding_utf8_next_codepoint_iterates)
{
    const char *input;
    ft_size_t index;
    uint32_t code_point;

    input = "A" "\xC3\xA9";
    index = 0;
    code_point = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoding_utf8_next_codepoint(input,
        ft_strlen_size_t(input), &index, &code_point));
    FT_ASSERT_EQ('A', code_point);
    FT_ASSERT_EQ(1, index);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoding_utf8_next_codepoint(input,
        ft_strlen_size_t(input), &index, &code_point));
    FT_ASSERT_EQ(0xE9U, code_point);
    FT_ASSERT_EQ(3, index);
    return (1);
}
