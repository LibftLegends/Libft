#include "../test_internal.hpp"
#include "../../Modules/Compression/compression.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include <random>
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_compression_base64_decodes_wrapped_groups_and_unpadded_binary_tail)
{
    const uint8_t input[] = "AAEC\r\nAw\tQF \vBg\f";
    const uint8_t expected[] = {0U, 1U, 2U, 3U, 4U, 5U, 6U};
    ft_size_t decoded_size = 0U;
    uint8_t *decoded = ft_base64_decode(input, sizeof(input) - 1U, &decoded_size);
    FT_ASSERT(decoded != ft_nullptr);
    FT_ASSERT_EQ(sizeof(expected), decoded_size);
    FT_ASSERT_EQ(0, ft_memcmp(expected, decoded, sizeof(expected)));
    cma_free(decoded);
    return (1);
}

FT_TEST(test_compression_base64_encode_rejects_output_size_overflow)
{
    const uint8_t input = 0U;
    ft_size_t encoded_size = 9U;
    FT_ASSERT_EQ(ft_nullptr, ft_base64_encode(&input, FT_SYSTEM_SIZE_MAX,
            &encoded_size));
    FT_ASSERT_EQ(0U, encoded_size);
    return (1);
}

FT_TEST(test_compression_base64_roundtrips_seeded_binary_inputs)
{
    std::mt19937 random(201991U);
    uint8_t input[97];
    uint32_t sample = 0U;
    while (sample < 1000U)
    {
        const ft_size_t length = sample % (sizeof(input) + 1U);
        ft_size_t index = 0U;
        while (index < length)
        {
            input[index] = static_cast<uint8_t>(random());
            ++index;
        }
        ft_size_t encoded_size = 0U;
        uint8_t *encoded = ft_base64_encode(input, length, &encoded_size);
        FT_ASSERT(encoded != ft_nullptr);
        ft_size_t decoded_size = 0U;
        uint8_t *decoded = ft_base64_decode(encoded, encoded_size, &decoded_size);
        cma_free(encoded);
        FT_ASSERT(decoded != ft_nullptr);
        const ft_bool matches = (decoded_size == length
            && ft_memcmp(input, decoded, length) == 0);
        cma_free(decoded);
        FT_ASSERT(matches);
        ++sample;
    }
    return (1);
}

FT_TEST(test_base64_encode_null_input_sets_errno)
{
    std::size_t encoded_length;

    encoded_length = 123;
    FT_ASSERT_EQ(ft_nullptr, ft_base64_encode(ft_nullptr, 4, &encoded_length));
    FT_ASSERT_EQ(static_cast<std::size_t>(0), encoded_length);
    return (1);
}

FT_TEST(test_base64_encode_null_size_sets_errno)
{
    unsigned char input_buffer[3];

    input_buffer[0] = 'a';
    input_buffer[1] = 'b';
    input_buffer[2] = 'c';
    FT_ASSERT_EQ(ft_nullptr, ft_base64_encode(input_buffer, 3, ft_nullptr));
    return (1);
}

FT_TEST(test_base64_encode_allocation_failure_sets_errno)
{
    unsigned char   input_buffer[3];
    unsigned char   *result_buffer;
    std::size_t     encoded_length;

    input_buffer[0] = 'A';
    input_buffer[1] = 'B';
    input_buffer[2] = 'C';
    encoded_length = 0;
    cma_set_alloc_limit(2);
    result_buffer = ft_base64_encode(input_buffer, 3, &encoded_length);
    FT_ASSERT_EQ(ft_nullptr, result_buffer);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), encoded_length);
    cma_set_alloc_limit(0);
    return (1);
}

FT_TEST(test_base64_encode_success_resets_errno)
{
    unsigned char   input_buffer[1];
    unsigned char   *encoded_buffer;
    std::size_t     encoded_length;

    input_buffer[0] = '\0';
    encoded_length = 0;
    encoded_buffer = ft_base64_encode(input_buffer, 0, &encoded_length);
    FT_ASSERT(encoded_buffer != ft_nullptr);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), encoded_length);
    cma_free(encoded_buffer);
    return (1);
}

FT_TEST(test_base64_decode_null_input_sets_errno)
{
    std::size_t decoded_length;

    decoded_length = 77;
    FT_ASSERT_EQ(ft_nullptr, ft_base64_decode(ft_nullptr, 4, &decoded_length));
    FT_ASSERT_EQ(static_cast<std::size_t>(77), decoded_length);
    return (1);
}

FT_TEST(test_base64_decode_null_size_sets_errno)
{
    const unsigned char *input_buffer;

    input_buffer = reinterpret_cast<const unsigned char *>("AAAA");
    FT_ASSERT_EQ(ft_nullptr, ft_base64_decode(input_buffer, 4, ft_nullptr));
    return (1);
}

FT_TEST(test_base64_decode_empty_input_resets_errno)
{
    unsigned char   *decoded_buffer;
    std::size_t     decoded_length;

    decoded_length = 99;
    decoded_buffer = ft_base64_decode(reinterpret_cast<const unsigned char *>(""), 0, &decoded_length);
    FT_ASSERT(decoded_buffer != ft_nullptr);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), decoded_length);
    cma_free(decoded_buffer);
    return (1);
}

FT_TEST(test_base64_decode_short_block_sets_errno)
{
    const unsigned char   *input_buffer;
    unsigned char         *decoded_buffer;
    std::size_t           decoded_length;

    input_buffer = reinterpret_cast<const unsigned char *>("A");
    decoded_length = 0;
    decoded_buffer = ft_base64_decode(input_buffer, 1, &decoded_length);
    FT_ASSERT_EQ(ft_nullptr, decoded_buffer);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), decoded_length);
    return (1);
}

FT_TEST(test_base64_decode_allocation_failure_sets_errno)
{
    const unsigned char   *input_buffer;
    unsigned char         *decoded_buffer;
    std::size_t           decoded_length;

    input_buffer = reinterpret_cast<const unsigned char *>("AAAA");
    decoded_length = 0;
    cma_set_alloc_limit(2);
    decoded_buffer = ft_base64_decode(input_buffer, 4, &decoded_length);
    FT_ASSERT_EQ(ft_nullptr, decoded_buffer);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), decoded_length);
    cma_set_alloc_limit(0);
    return (1);
}

FT_TEST(test_base64_decode_invalid_first_character_sets_errno)
{
    const unsigned char   *input_buffer;
    unsigned char         *decoded_buffer;
    std::size_t           decoded_length;

    input_buffer = reinterpret_cast<const unsigned char *>("!AAA");
    decoded_length = 0;
    decoded_buffer = ft_base64_decode(input_buffer, 4, &decoded_length);
    FT_ASSERT_EQ(ft_nullptr, decoded_buffer);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), decoded_length);
    return (1);
}

FT_TEST(test_base64_decode_invalid_second_character_sets_errno)
{
    const unsigned char   *input_buffer;
    unsigned char         *decoded_buffer;
    std::size_t           decoded_length;

    input_buffer = reinterpret_cast<const unsigned char *>("A!AA");
    decoded_length = 0;
    decoded_buffer = ft_base64_decode(input_buffer, 4, &decoded_length);
    FT_ASSERT_EQ(ft_nullptr, decoded_buffer);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), decoded_length);
    return (1);
}

FT_TEST(test_base64_decode_invalid_third_character_sets_errno)
{
    const unsigned char   *input_buffer;
    unsigned char         *decoded_buffer;
    std::size_t           decoded_length;

    input_buffer = reinterpret_cast<const unsigned char *>("AA!A");
    decoded_length = 0;
    decoded_buffer = ft_base64_decode(input_buffer, 4, &decoded_length);
    FT_ASSERT_EQ(ft_nullptr, decoded_buffer);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), decoded_length);
    return (1);
}

FT_TEST(test_base64_decode_invalid_fourth_character_sets_errno)
{
    const unsigned char   *input_buffer;
    unsigned char         *decoded_buffer;
    std::size_t           decoded_length;

    input_buffer = reinterpret_cast<const unsigned char *>("AAA!");
    decoded_length = 0;
    decoded_buffer = ft_base64_decode(input_buffer, 4, &decoded_length);
    FT_ASSERT_EQ(ft_nullptr, decoded_buffer);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), decoded_length);
    return (1);
}

FT_TEST(test_base64_decode_invalid_padding_mismatch_sets_errno)
{
    const unsigned char   *input_buffer;
    unsigned char         *decoded_buffer;
    std::size_t           decoded_length;

    input_buffer = reinterpret_cast<const unsigned char *>("AA=A");
    decoded_length = 0;
    decoded_buffer = ft_base64_decode(input_buffer, 4, &decoded_length);
    FT_ASSERT_EQ(ft_nullptr, decoded_buffer);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), decoded_length);
    return (1);
}

FT_TEST(test_base64_decode_trailing_data_after_padding_sets_errno)
{
    const unsigned char   *input_buffer;
    unsigned char         *decoded_buffer;
    std::size_t           decoded_length;

    input_buffer = reinterpret_cast<const unsigned char *>("AAA=AAAA");
    decoded_length = 0;
    decoded_buffer = ft_base64_decode(input_buffer, 8, &decoded_length);
    FT_ASSERT_EQ(ft_nullptr, decoded_buffer);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), decoded_length);
    return (1);
}

FT_TEST(test_base64_decode_success_resets_errno)
{
    const unsigned char   *input_buffer;
    unsigned char         *decoded_buffer;
    std::size_t           decoded_length;

    input_buffer = reinterpret_cast<const unsigned char *>("TQ==");
    decoded_length = 0;
    decoded_buffer = ft_base64_decode(input_buffer, 4, &decoded_length);
    FT_ASSERT(decoded_buffer != ft_nullptr);
    FT_ASSERT_EQ(static_cast<std::size_t>(1), decoded_length);
    FT_ASSERT_EQ('M', decoded_buffer[0]);
    cma_free(decoded_buffer);
    return (1);
}

