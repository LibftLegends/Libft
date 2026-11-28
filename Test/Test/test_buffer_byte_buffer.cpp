#include "../test_internal.hpp"
#include "../../Modules/Buffer/byte_buffer.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <cstring>
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_buffer_byte_buffer_append_and_read_bytes)
{
    ft_byte_buffer buffer;
    uint8_t input[4];
    uint8_t output[4];

    input[0] = 1;
    input[1] = 2;
    input[2] = 3;
    input[3] = 4;
    output[0] = 0;
    output[1] = 0;
    output[2] = 0;
    output[3] = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append(input, 4));
    FT_ASSERT_EQ(4, buffer.size());
    FT_ASSERT_EQ(4, buffer.remaining());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read(output, 4));
    FT_ASSERT_EQ(0, ft_memcmp(input, output, 4));
    FT_ASSERT_EQ(0, buffer.remaining());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

static int32_t test_buffer_partial_reader(uint8_t *destination,
    ft_size_t length, void *user_data) noexcept
{
    destination[length - 1U] = *static_cast<uint8_t *>(user_data);
    return (FT_ERR_IO);
}

static int32_t test_buffer_binary_reader(uint8_t *destination,
    ft_size_t length, void *user_data) noexcept
{
    ft_memcpy(destination, user_data, length);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_buffer_initialize_from_reader_rolls_back_and_retries)
{
    ft_byte_buffer buffer;
    uint8_t bytes[] = {0x17U, 0x00U, 0xFFU, 0x3CU};
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        buffer.initialize_from_reader(sizeof(bytes), ft_nullptr, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_IO, buffer.initialize_from_reader(sizeof(bytes),
        test_buffer_partial_reader, bytes));
    FT_ASSERT_EQ(FT_FALSE, buffer.is_initialised());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize_from_reader(sizeof(bytes),
        test_buffer_binary_reader, bytes));
    FT_ASSERT_EQ(sizeof(bytes), buffer.size());
    FT_ASSERT_EQ(sizeof(bytes), buffer.capacity());
    FT_ASSERT_EQ(0U, buffer.read_position());
    FT_ASSERT_EQ(FT_TRUE, buffer.is_fixed_capacity());
    FT_ASSERT_EQ(0, ft_memcmp(buffer.data(), bytes, sizeof(bytes)));
    FT_ASSERT_EQ(FT_ERR_FULL, buffer.append_u8(0x42U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize_from_reader(0U,
        test_buffer_partial_reader, bytes));
    FT_ASSERT_EQ(0U, buffer.size());
    FT_ASSERT_EQ(FT_TRUE, buffer.is_fixed_capacity());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_consume_advances_read_position)
{
    ft_byte_buffer buffer;
    uint8_t input[3];
    uint8_t value;

    input[0] = 9;
    input[1] = 8;
    input[2] = 7;
    value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append(input, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.consume(2));
    FT_ASSERT_EQ(2, buffer.read_position());
    FT_ASSERT_EQ(1, buffer.remaining());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_u8(&value));
    FT_ASSERT_EQ(7, value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_fixed_capacity_rejects_growth)
{
    ft_byte_buffer buffer;
    uint8_t input[3];

    input[0] = 1;
    input[1] = 2;
    input[2] = 3;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(2, FT_TRUE));
    FT_ASSERT_EQ(FT_TRUE, buffer.is_fixed_capacity());
    FT_ASSERT_EQ(FT_ERR_FULL, buffer.append(input, 3));
    FT_ASSERT_EQ(0, buffer.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append(input, 2));
    FT_ASSERT_EQ(2, buffer.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_overwrites_existing_u32_without_moving_cursor)
{
    ft_byte_buffer buffer;
    uint32_t value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(5U, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u8(0xABU));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u32_le(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.set_read_position(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.overwrite_u32_le(1U, 0x12345678U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, buffer.overwrite_u32_le(2U, 0U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, buffer.overwrite_u32_le(UINT64_MAX, 0U));
    FT_ASSERT_EQ(5U, buffer.size());
    FT_ASSERT_EQ(1U, buffer.read_position());
    FT_ASSERT_EQ(0xABU, buffer.data()[0U]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_u32_le(&value));
    FT_ASSERT_EQ(0x12345678U, value);
    return (1);
}

FT_TEST(test_buffer_byte_buffer_endian_helpers_roundtrip)
{
    ft_byte_buffer buffer;
    uint16_t value_u16_be;
    uint16_t value_u16_le;
    uint32_t value_u32_be;
    uint32_t value_u32_le;
    uint64_t value_u64_be;
    uint64_t value_u64_le;

    value_u16_be = 0;
    value_u16_le = 0;
    value_u32_be = 0;
    value_u32_le = 0;
    value_u64_be = 0;
    value_u64_le = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u16_be(0x1234U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u16_le(0x5678U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u32_be(0x12345678U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u32_le(0x90ABCDEFU));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u64_be(0x0123456789ABCDEFULL));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u64_le(0xFEDCBA9876543210ULL));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_u16_be(&value_u16_be));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_u16_le(&value_u16_le));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_u32_be(&value_u32_be));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_u32_le(&value_u32_le));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_u64_be(&value_u64_be));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_u64_le(&value_u64_le));
    FT_ASSERT_EQ(0x1234U, value_u16_be);
    FT_ASSERT_EQ(0x5678U, value_u16_le);
    FT_ASSERT_EQ(0x12345678U, value_u32_be);
    FT_ASSERT_EQ(0x90ABCDEFU, value_u32_le);
    FT_ASSERT_EQ(0x0123456789ABCDEFULL, value_u64_be);
    FT_ASSERT_EQ(0xFEDCBA9876543210ULL, value_u64_le);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_view_checks_bounds)
{
    ft_byte_buffer buffer;
    uint8_t input[4];
    const uint8_t *view_pointer;

    input[0] = 10;
    input[1] = 20;
    input[2] = 30;
    input[3] = 40;
    view_pointer = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append(input, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.view(1, 2, &view_pointer));
    FT_ASSERT(view_pointer != ft_nullptr);
    FT_ASSERT_EQ(20, view_pointer[0]);
    FT_ASSERT_EQ(30, view_pointer[1]);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, buffer.view(3, 2, &view_pointer));
    FT_ASSERT(view_pointer == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_copy_preserves_state)
{
    ft_byte_buffer source;
    ft_byte_buffer copy;
    uint8_t value;

    value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.append_u8(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.append_u8(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.consume(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(1, copy.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.read_u8(&value));
    FT_ASSERT_EQ(2, value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_thread_safety_toggle)
{
    ft_byte_buffer buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_FALSE, buffer.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, buffer.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, buffer.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_varint_helpers_roundtrip)
{
    ft_byte_buffer buffer;
    uint64_t unsigned_value;
    int64_t signed_value;

    unsigned_value = 0;
    signed_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_varuint64(300U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        buffer.append_varint64(static_cast<int64_t>(-123456789)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_varuint64(&unsigned_value));
    FT_ASSERT_EQ(300U, unsigned_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_varint64(&signed_value));
    FT_ASSERT_EQ(static_cast<int64_t>(-123456789), signed_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_search_peek_skip_and_slice_helpers)
{
    ft_byte_buffer buffer;
    ft_byte_buffer slice;
    uint8_t output[3];
    const char input[] = "abc123abc";
    ft_size_t found_position;

    output[0] = 0;
    output[1] = 0;
    output[2] = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append(input, 9));

    found_position = buffer.find("123", 3);
    FT_ASSERT_EQ(3, found_position);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.get_error());

    found_position = buffer.find("zzz", 3);
    FT_ASSERT_EQ(static_cast<ft_size_t>(-1), found_position);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, buffer.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.peek(3, output, 3));
    FT_ASSERT_EQ(0, std::memcmp(output, "123", 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.skip(3));
    FT_ASSERT_EQ(3, buffer.read_position());
    FT_ASSERT_EQ(6U, buffer.find("abc", 3U));
    FT_ASSERT_EQ(3U, buffer.find(ft_nullptr, 0U));
    FT_ASSERT_EQ(3U, buffer.read_position());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.slice(3, 3, slice));
    FT_ASSERT_EQ(3, slice.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, slice.read(output, 3));
    FT_ASSERT_EQ(0, std::memcmp(output, "123", 3));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, slice.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_append_buffer_appends_source_bytes)
{
    ft_byte_buffer destination_buffer;
    ft_byte_buffer source_buffer;
    uint8_t output[6];

    output[0] = 0;
    output[1] = 0;
    output[2] = 0;
    output[3] = 0;
    output[4] = 0;
    output[5] = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.append("foo", 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.append("bar", 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.append_buffer(source_buffer));
    FT_ASSERT_EQ(6, destination_buffer.size());
    FT_ASSERT_EQ(3, source_buffer.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.read(output, 6));
    FT_ASSERT_EQ(0, std::memcmp(output, "foobar", 6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_prepend_buffer_prepends_source_bytes)
{
    ft_byte_buffer destination_buffer;
    ft_byte_buffer source_buffer;
    uint8_t output[2];

    output[0] = 0;
    output[1] = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.append("bar", 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.consume(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.append("foo", 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.prepend_buffer(source_buffer));
    FT_ASSERT_EQ(4, destination_buffer.read_position());
    FT_ASSERT_EQ(2, destination_buffer.remaining());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.read(output, 2));
    FT_ASSERT_EQ(0, std::memcmp(output, "ar", 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_shrink_to_fit_reduces_capacity)
{
    ft_byte_buffer buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(32, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append("abc", 3));
    FT_ASSERT_EQ(32, buffer.capacity());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.shrink_to_fit());
    FT_ASSERT_EQ(3, buffer.capacity());
    FT_ASSERT_EQ(3, buffer.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_self_append_keeps_source_after_growth)
{
    ft_byte_buffer buffer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(16U, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append("0123456789abcdef", 16U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.consume(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append(buffer.data() + 2U, 5U));
    FT_ASSERT_EQ(21U, buffer.size());
    FT_ASSERT_EQ(2U, buffer.read_position());
    FT_ASSERT_EQ(0, std::memcmp(buffer.data(), "0123456789abcdef23456", 21U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_buffer(buffer));
    FT_ASSERT_EQ(42U, buffer.size());
    FT_ASSERT_EQ(0, std::memcmp(buffer.data(), buffer.data() + 21U, 21U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_self_prepend_preserves_cursor_and_failure)
{
    ft_byte_buffer buffer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(9U, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append("abcde", 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.consume(2U));
    FT_ASSERT_EQ(FT_ERR_FULL, buffer.prepend_buffer(buffer));
    FT_ASSERT_EQ(5U, buffer.size());
    FT_ASSERT_EQ(2U, buffer.read_position());
    FT_ASSERT_EQ(0, std::memcmp(buffer.data(), "abcde", 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(5U, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append("abcde", 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.consume(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.prepend_buffer(buffer));
    FT_ASSERT_EQ(7U, buffer.read_position());
    FT_ASSERT_EQ(10U, buffer.size());
    FT_ASSERT_EQ(0, std::memcmp(buffer.data(), "abcdeabcde", 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_buffer_byte_buffer_slice_owns_independent_storage)
{
    ft_byte_buffer source;
    ft_byte_buffer output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.append("abcdef", 6U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.append("old", 3U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, source.slice(4U, 3U, output));
    FT_ASSERT_EQ(3U, output.size());
    FT_ASSERT_EQ(0, std::memcmp(output.data(), "old", 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.slice(1U, 4U, output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_TRUE, output.is_fixed_capacity());
    FT_ASSERT_EQ(4U, output.capacity());
    FT_ASSERT_EQ(0U, output.read_position());
    FT_ASSERT_EQ(0, std::memcmp(output.data(), "bcde", 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    return (1);
}
