#include "../test_internal.hpp"
#include "../../Modules/Encoding/encoding.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_encoding_base32_roundtrip)
{
    const uint8_t input[] = {'f', 'o', 'o', 'b', 'a', 'r'};
    char *encoded;
    uint8_t *decoded;
    ft_size_t decoded_size;

    decoded_size = 0;
    encoded = encoding_base32_encode(input, 6);
    FT_ASSERT(encoded != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(encoded, "MZXW6YTBOI======"));
    decoded = encoding_base32_decode(encoded, ft_strlen_size_t(encoded),
            &decoded_size);
    FT_ASSERT(decoded != ft_nullptr);
    FT_ASSERT_EQ(6, decoded_size);
    FT_ASSERT_EQ(0, ft_memcmp(input, decoded, decoded_size));
    cma_free(encoded);
    cma_free(decoded);
    return (1);
}

FT_TEST(test_encoding_base32_decode_rejects_invalid_character)
{
    uint8_t *decoded;
    ft_size_t decoded_size;

    decoded_size = 0;
    decoded = encoding_base32_decode("MZXW6YTB0I======", 16, &decoded_size);
    FT_ASSERT(decoded == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, encoding_get_error());
    FT_ASSERT_EQ(0, decoded_size);
    return (1);
}

FT_TEST(test_encoding_base32_roundtrip_full_groups_and_each_tail)
{
    const uint8_t input[] = {'f', 'o', 'o', 'b', 'a', 'r', '1', '2', '3'};
    const char *const expected[] = {"MZXW6YTB", "MZXW6YTBOI======",
        "MZXW6YTBOIYQ====", "MZXW6YTBOIYTE===", "MZXW6YTBOIYTEMY="};
    ft_size_t length = 5U;
    while (length <= sizeof(input))
    {
        char *encoded = encoding_base32_encode(input, length);
        FT_ASSERT(encoded != ft_nullptr);
        FT_ASSERT_EQ(0, ft_strcmp(expected[length - 5U], encoded));
        ft_size_t decoded_size = 0U;
        uint8_t *decoded = encoding_base32_decode(encoded,
                ft_strlen_size_t(encoded), &decoded_size);
        FT_ASSERT(decoded != ft_nullptr);
        FT_ASSERT_EQ(length, decoded_size);
        FT_ASSERT_EQ(0, ft_memcmp(input, decoded, decoded_size));
        FT_ASSERT_EQ(0U, decoded[decoded_size]);
        cma_free(decoded);
        cma_free(encoded);
        ++length;
    }
    return (1);
}

FT_TEST(test_encoding_base32_lowercase_unpadded_and_invalid_tail_bits)
{
    ft_size_t decoded_size = 0U;
    uint8_t *decoded = encoding_base32_decode("mzxw6ytboi", 10U, &decoded_size);
    FT_ASSERT(decoded != ft_nullptr);
    FT_ASSERT_EQ(6U, decoded_size);
    FT_ASSERT_EQ(0, ft_memcmp("foobar", decoded, decoded_size));
    cma_free(decoded);
    FT_ASSERT(encoding_base32_decode("MZXW6YTBOJ======", 16U, &decoded_size)
        == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, encoding_get_error());
    FT_ASSERT_EQ(0U, decoded_size);
    return (1);
}

