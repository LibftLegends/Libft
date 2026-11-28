#include "../test_internal.hpp"
#include "../../Modules/Advanced/advanced.hpp"
#include "../../Modules/Basic/utf8.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_basic_utf8_transform_mixed_bytes_and_explicit_null)
{
    const char input[] = "a\0" "\xC3\xA9" "z";
    const char expected[] = "A\0" "\xC3\xA9" "Z";
    char output[sizeof(expected)];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_transform(input, sizeof(input) - 1U,
        output, sizeof(output), ft_utf8_case_ascii_upper));
    FT_ASSERT_EQ(0, ft_memcmp(output, expected, sizeof(expected)));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, ft_utf8_transform(input, sizeof(input) - 1U,
        output, sizeof(output) - 1U, ft_utf8_case_ascii_upper));
    return (1);
}

FT_TEST(test_basic_utf8_count_ascii)
{
    ft_size_t code_point_count;

    code_point_count = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_count("hello", &code_point_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(5), code_point_count);
    return (1);
}

FT_TEST(test_basic_utf8_next_multibyte)
{
    const char *utf8_string;
    ft_size_t string_length;
    ft_size_t index_pointer;
    uint32_t code_point_value;
    ft_size_t sequence_length;

    utf8_string = "héllo";
    string_length = ft_strlen_size_t(utf8_string);
    index_pointer = 0;
    code_point_value = 0;
    sequence_length = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_next(utf8_string, string_length,
            &index_pointer, &code_point_value, &sequence_length));
    FT_ASSERT_EQ(static_cast<uint32_t>('h'), code_point_value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), sequence_length);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_next(utf8_string, string_length,
            &index_pointer, &code_point_value, &sequence_length));
    FT_ASSERT_EQ(static_cast<uint32_t>(0xE9), code_point_value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), sequence_length);
    return (1);
}

FT_TEST(test_basic_utf8_next_four_byte_sequence)
{
    const char *utf8_string;
    ft_size_t string_length;
    ft_size_t index_pointer;
    uint32_t code_point_value;
    ft_size_t sequence_length;

    utf8_string = "A" "\xF0\x9F\x98\x80" "B";
    string_length = ft_strlen_size_t(utf8_string);
    index_pointer = 0;
    code_point_value = 0;
    sequence_length = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_next(utf8_string, string_length,
            &index_pointer, &code_point_value, &sequence_length));
    FT_ASSERT_EQ(static_cast<uint32_t>('A'), code_point_value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), sequence_length);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_next(utf8_string, string_length,
            &index_pointer, &code_point_value, &sequence_length));
    FT_ASSERT_EQ(static_cast<uint32_t>(0x1F600), code_point_value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(4), sequence_length);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_next(utf8_string, string_length,
            &index_pointer, &code_point_value, &sequence_length));
    FT_ASSERT_EQ(static_cast<uint32_t>('B'), code_point_value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), sequence_length);
    return (1);
}

FT_TEST(test_basic_utf8_next_invalid_sequence_sets_errno)
{
    char invalid_sequence[3];
    ft_size_t string_length;
    ft_size_t index_pointer;
    uint32_t code_point_value;

    invalid_sequence[0] = static_cast<char>(0xC0);
    invalid_sequence[1] = static_cast<char>(0xAF);
    invalid_sequence[2] = '\0';
    string_length = static_cast<ft_size_t>(2);
    index_pointer = 0;
    code_point_value = 0;
    FT_ASSERT(FT_ERR_SUCCESS != ft_utf8_next(invalid_sequence, string_length,
            &index_pointer, &code_point_value, ft_nullptr));
    return (1);
}

FT_TEST(test_basic_utf8_next_recovers_after_invalid_sequence)
{
    char invalid_sequence[3];
    const char *valid_string;
    ft_size_t string_length;
    ft_size_t index_pointer;
    uint32_t code_point_value;
    ft_size_t sequence_length;

    invalid_sequence[0] = static_cast<char>(0xC1);
    invalid_sequence[1] = static_cast<char>(0xBF);
    invalid_sequence[2] = '\0';
    string_length = static_cast<ft_size_t>(2);
    index_pointer = 0;
    code_point_value = 0;
    sequence_length = 0;
    FT_ASSERT(FT_ERR_SUCCESS != ft_utf8_next(invalid_sequence, string_length,
            &index_pointer, &code_point_value, &sequence_length));
    valid_string = "A";
    string_length = ft_strlen_size_t(valid_string);
    index_pointer = 0;
    code_point_value = 0;
    sequence_length = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_next(valid_string, string_length,
            &index_pointer, &code_point_value, &sequence_length));
    FT_ASSERT_EQ(static_cast<uint32_t>('A'), code_point_value);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), sequence_length);
    return (1);
}

FT_TEST(test_basic_utf8_count_null_pointer_sets_errno)
{
    ft_size_t code_point_count;

    code_point_count = 1;
    FT_ASSERT(FT_ERR_SUCCESS != ft_utf8_count(ft_nullptr, &code_point_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), code_point_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_count("ok", &code_point_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), code_point_count);
    return (1);
}

static uint32_t ft_utf8_test_invalid_hook(uint32_t code_point)
{
    (void)code_point;
    return (0x110000);
}

FT_TEST(test_basic_utf8_transform_alloc_lowercase)
{
    char *transformed_string;

    transformed_string = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_transform_alloc("TeSt",
            &transformed_string, ft_utf8_case_ascii_lower));
    FT_ASSERT(transformed_string != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp("test", transformed_string));
    cma_free(transformed_string);
    return (1);
}

FT_TEST(test_basic_utf8_transform_alloc_uppercase)
{
    char *transformed_string;

    transformed_string = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_transform_alloc("emoji: " "\xF0\x9F\x98\x80", &transformed_string,
            ft_utf8_case_ascii_upper));
    FT_ASSERT(transformed_string != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp("EMOJI: " "\xF0\x9F\x98\x80", transformed_string));
    cma_free(transformed_string);
    return (1);
}

FT_TEST(test_basic_utf8_grapheme_duplicate_combining_mark)
{
    const char *grapheme_string;
    ft_size_t string_length;
    ft_size_t index_pointer;
    char *grapheme_copy;

    grapheme_string = "e\xCC\x81o";
    string_length = ft_strlen_size_t(grapheme_string);
    index_pointer = 0;
    grapheme_copy = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_duplicate_grapheme(grapheme_string,
            string_length, &index_pointer, &grapheme_copy));
    FT_ASSERT(grapheme_copy != ft_nullptr);
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), ft_strlen_size_t(grapheme_copy));
    FT_ASSERT_EQ(0, ft_strcmp("e\xCC\x81", grapheme_copy));
    cma_free(grapheme_copy);
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), index_pointer);
    return (1);
}

FT_TEST(test_basic_utf8_count_invalid_sets_errno)
{
    char invalid_sequence[3];
    ft_size_t code_point_count;

    invalid_sequence[0] = static_cast<char>(0xE0);
    invalid_sequence[1] = static_cast<char>(0x80);
    invalid_sequence[2] = '\0';
    code_point_count = 0;
    FT_ASSERT(FT_ERR_SUCCESS != ft_utf8_count(invalid_sequence, &code_point_count));
    return (1);
}

FT_TEST(test_basic_utf8_encode_emoji)
{
    char buffer[5];
    ft_size_t encoded_length;

    buffer[0] = '\0';
    buffer[1] = '\0';
    buffer[2] = '\0';
    buffer[3] = '\0';
    buffer[4] = '\0';
    encoded_length = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_encode(0x1F642, buffer, sizeof(buffer),
            &encoded_length));
    FT_ASSERT_EQ(static_cast<ft_size_t>(4), encoded_length);
    FT_ASSERT_EQ(static_cast<char>(0xF0), buffer[0]);
    FT_ASSERT_EQ(static_cast<char>(0x9F), buffer[1]);
    FT_ASSERT_EQ(static_cast<char>(0x99), buffer[2]);
    FT_ASSERT_EQ(static_cast<char>(0x82), buffer[3]);
    FT_ASSERT_EQ('\0', buffer[4]);
    return (1);
}

FT_TEST(test_basic_utf8_encode_reports_small_buffer)
{
    char buffer[3];

    buffer[0] = '\0';
    buffer[1] = '\0';
    buffer[2] = '\0';
    FT_ASSERT(FT_ERR_SUCCESS != ft_utf8_encode(0x20AC, buffer, sizeof(buffer), ft_nullptr));
    return (1);
}

FT_TEST(test_basic_utf8_next_rejects_surrogate)
{
    const char *invalid_string;
    ft_size_t string_length;
    ft_size_t index_pointer;
    uint32_t code_point_value;

    invalid_string = "\xED\xA0\x80";
    string_length = static_cast<ft_size_t>(3);
    index_pointer = 0;
    code_point_value = 0;
    FT_ASSERT(FT_ERR_SUCCESS != ft_utf8_next(invalid_string, string_length,
            &index_pointer, &code_point_value, ft_nullptr));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), index_pointer);
    return (1);
}

FT_TEST(test_basic_utf8_next_detects_truncated_sequence)
{
    const char *invalid_string;
    ft_size_t string_length;
    ft_size_t index_pointer;
    uint32_t code_point_value;

    invalid_string = "\xE2\x82";
    string_length = static_cast<ft_size_t>(2);
    index_pointer = 0;
    code_point_value = 0;
    FT_ASSERT(FT_ERR_SUCCESS != ft_utf8_next(invalid_string, string_length,
            &index_pointer, &code_point_value, ft_nullptr));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), index_pointer);
    return (1);
}

FT_TEST(test_basic_utf8_next_null_pointer_guard)
{
    ft_size_t index_pointer;
    uint32_t code_point_value;

    index_pointer = 0;
    code_point_value = 0;
    FT_ASSERT(FT_ERR_SUCCESS != ft_utf8_next(ft_nullptr, static_cast<ft_size_t>(0),
            &index_pointer, &code_point_value, ft_nullptr));
    return (1);
}

FT_TEST(test_basic_utf8_encode_rejects_surrogate)
{
    char buffer[4];

    buffer[0] = '\0';
    buffer[1] = '\0';
    buffer[2] = '\0';
    buffer[3] = '\0';
    FT_ASSERT(FT_ERR_SUCCESS != ft_utf8_encode(0xD800, buffer, sizeof(buffer), ft_nullptr));
    return (1);
}

FT_TEST(test_basic_utf8_transform_alloc_propagates_hook_error)
{
    char *transformed_string;

    transformed_string = ft_nullptr;
    FT_ASSERT(FT_ERR_SUCCESS != ft_utf8_transform_alloc("abc", &transformed_string,
            ft_utf8_test_invalid_hook));
    FT_ASSERT_EQ(ft_nullptr, transformed_string);
    return (1);
}
