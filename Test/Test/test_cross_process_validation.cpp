#include "../test_internal.hpp"
#include "../../Modules/CrossProcess/cross_process.hpp"
#include "../../Modules/Compatebility/compatebility_cross_process.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include <cerrno>
#include <cstring>

FT_TEST(test_cross_process_write_rejects_address_below_base)
{
    cross_process_message message;
    const uint8_t payload[1] = {1U};
    int32_t result;

    std::memset(&message, 0, sizeof(message));
    message.stack_base_address = 0x100000U;
    message.remote_memory_address = message.stack_base_address - 1U;
    message.remote_memory_size = 16U;
    errno = 0;
    result = cp_write_memory(message, payload, sizeof(payload), 0);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    FT_ASSERT_EQ(EINVAL, errno);
    return (1);
}

FT_TEST(test_cross_process_write_rejects_unterminated_shared_memory_name)
{
    cross_process_message message;
    const uint8_t payload[1] = {1U};
    int32_t result;

    std::memset(&message, 0, sizeof(message));
    std::memset(message.shared_memory_name, 0xff,
        sizeof(message.shared_memory_name));
    message.stack_base_address = 0x100000U;
    message.remote_memory_address = message.stack_base_address + 1U;
    message.remote_memory_size = 16U;
    errno = 0;
    result = cp_write_memory(message, payload, sizeof(payload), 0);
    FT_ASSERT(result != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(EINVAL, errno);
    return (1);
}

FT_TEST(test_cross_process_descriptor_wire_round_trip)
{
    cross_process_message source;
    cross_process_message decoded;
    uint8_t wire[CMP_CROSS_PROCESS_WIRE_SIZE];
    int32_t result;

    std::memset(&source, 0, sizeof(source));
    source.stack_base_address = 0x100000U;
    source.remote_memory_address = 0x100100U;
    source.remote_memory_size = 0x2000U;
    source.shared_mutex_address = 0x100020U;
    source.error_memory_address = 0x100180U;
    std::memcpy(source.shared_memory_name, "/cross_process_wire", 20U);
    result = cmp_cross_process_encode_wire(source, wire, sizeof(wire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    std::memset(&decoded, 0, sizeof(decoded));
    result = cmp_cross_process_decode_wire(wire, sizeof(wire), decoded);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(source.stack_base_address, decoded.stack_base_address);
    FT_ASSERT_EQ(source.remote_memory_address, decoded.remote_memory_address);
    FT_ASSERT_EQ(source.remote_memory_size, decoded.remote_memory_size);
    FT_ASSERT_EQ(source.shared_mutex_address, decoded.shared_mutex_address);
    FT_ASSERT_EQ(source.error_memory_address, decoded.error_memory_address);
    FT_ASSERT(std::strcmp(source.shared_memory_name,
        decoded.shared_memory_name) == 0);
    return (1);
}

FT_TEST(test_cross_process_descriptor_wire_decode_is_transactional)
{
    cross_process_message source;
    cross_process_message message;
    cross_process_message original;
    uint8_t wire[CMP_CROSS_PROCESS_WIRE_SIZE];
    ft_size_t length;
    int32_t result;

    std::memset(&source, 0, sizeof(source));
    source.stack_base_address = 0x100000U;
    source.remote_memory_address = 0x100100U;
    source.remote_memory_size = 0x2000U;
    source.shared_mutex_address = 0x100020U;
    std::memcpy(source.shared_memory_name, "/cross_process_wire", 20U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cmp_cross_process_encode_wire(source, wire,
        sizeof(wire)));
    length = 0U;
    while (length < CMP_CROSS_PROCESS_WIRE_SIZE)
    {
        std::memset(&message, 0x5a, sizeof(message));
        original = message;
        result = cmp_cross_process_decode_wire(wire, length, message);
        FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
        FT_ASSERT(std::memcmp(&message, &original, sizeof(message)) == 0);
        length += 1U;
    }
    std::memset(&message, 0x5a, sizeof(message));
    original = message;
    std::memset(wire, 0, sizeof(wire));
    result = cmp_cross_process_decode_wire(wire, sizeof(wire), message);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    FT_ASSERT(std::memcmp(&message, &original, sizeof(message)) == 0);
    return (1);
}
