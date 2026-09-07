#include "../../Modules/Networking/networking_replication_protocol.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_networking_replication_envelope_round_trip)
{
    networking_replication_envelope source;
    networking_replication_envelope restored;
    ft_byte_buffer buffer;

    source.message_type = 5U;
    source.payload_size = 0U;
    source.server_instance_id = 41U;
    source.session_id = 42U;
    source.message_sequence = 43U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_envelope_serialize(source, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_envelope_deserialize(restored, buffer));
    FT_ASSERT_EQ(source.protocol_version, restored.protocol_version);
    FT_ASSERT_EQ(source.message_type, restored.message_type);
    FT_ASSERT_EQ(source.payload_size, restored.payload_size);
    FT_ASSERT_EQ(source.server_instance_id, restored.server_instance_id);
    FT_ASSERT_EQ(source.session_id, restored.session_id);
    FT_ASSERT_EQ(source.message_sequence, restored.message_sequence);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_networking_replication_envelope_rejects_payload_overflow)
{
    networking_replication_envelope envelope;
    ft_byte_buffer buffer;

    envelope.payload_size = NETWORKING_REPLICATION_MAX_PAYLOAD + 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        networking_replication_envelope_serialize(envelope, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_networking_replication_envelope_rejects_missing_identity)
{
    networking_replication_envelope envelope;
    ft_byte_buffer buffer;

    envelope.server_instance_id = 0U;
    envelope.session_id = 2U;
    envelope.message_sequence = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        networking_replication_envelope_serialize(envelope, buffer));
    envelope.server_instance_id = 1U;
    envelope.session_id = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        networking_replication_envelope_serialize(envelope, buffer));
    envelope.session_id = 2U;
    envelope.message_sequence = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        networking_replication_envelope_serialize(envelope, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_networking_replication_envelope_deserialize_is_transactional)
{
    networking_replication_envelope envelope;
    networking_replication_envelope original;
    ft_byte_buffer buffer;
    ft_size_t initial_read_position;

    original.message_type = 9U;
    original.payload_size = 7U;
    original.server_instance_id = 11U;
    original.session_id = 12U;
    original.message_sequence = 13U;
    envelope = original;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u16_le(
        NETWORKING_REPLICATION_PROTOCOL_VERSION));
    initial_read_position = buffer.read_position();
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        networking_replication_envelope_deserialize(envelope, buffer));
    FT_ASSERT_EQ(initial_read_position, buffer.read_position());
    FT_ASSERT_EQ(original.message_type, envelope.message_type);
    FT_ASSERT_EQ(original.payload_size, envelope.payload_size);
    FT_ASSERT_EQ(original.session_id, envelope.session_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_networking_replication_envelope_invalid_length_preserves_cursor)
{
    networking_replication_envelope envelope;
    ft_byte_buffer buffer;
    ft_size_t initial_read_position;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u16_le(
        NETWORKING_REPLICATION_PROTOCOL_VERSION));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u16_le(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u32_le(4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u64_le(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u64_le(3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u64_le(4U));
    initial_read_position = buffer.read_position();
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        networking_replication_envelope_deserialize(envelope, buffer));
    FT_ASSERT_EQ(initial_read_position, buffer.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_networking_replication_decode_message_copies_payload)
{
    networking_received_message message;
    networking_replication_envelope source;
    networking_replication_envelope decoded;
    ft_byte_buffer encoded;
    ft_byte_buffer payload;
    const uint8_t input[3] = {7U, 8U, 9U};
    const uint8_t *output;
    ft_size_t index;

    source.message_type = 17U;
    source.payload_size = sizeof(input);
    source.server_instance_id = 21U;
    source.session_id = 22U;
    source.message_sequence = 23U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.append_u16_le(
        source.protocol_version));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.append_u16_le(source.message_type));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.append_u32_le(source.payload_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.append_u64_le(
        source.server_instance_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.append_u64_le(source.session_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.append_u64_le(
        source.message_sequence));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.append(input, sizeof(input)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message.payload.initialize());
    index = 0U;
    while (index < encoded.size())
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, message.payload.push_back(
            encoded.data()[index]));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_decode_message(message, decoded, payload));
    FT_ASSERT_EQ(source.message_type, decoded.message_type);
    FT_ASSERT_EQ(source.session_id, decoded.session_id);
    FT_ASSERT_EQ(sizeof(input), payload.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.view(0U, payload.size(), &output));
    FT_ASSERT_EQ(input[0], output[0]);
    FT_ASSERT_EQ(input[1], output[1]);
    FT_ASSERT_EQ(input[2], output[2]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message.payload.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.destroy());
    return (1);
}

FT_TEST(test_networking_replication_decode_message_rejects_trailing_bytes)
{
    networking_received_message message;
    networking_replication_envelope envelope;
    networking_replication_envelope original;
    ft_byte_buffer encoded;
    ft_byte_buffer payload;
    const uint8_t extra_byte = 4U;
    ft_size_t index;

    original.message_type = 31U;
    original.payload_size = 0U;
    original.server_instance_id = 32U;
    original.session_id = 33U;
    original.message_sequence = 34U;
    envelope = original;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_envelope_serialize(original, encoded));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.append_u8(extra_byte));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message.payload.initialize());
    index = 0U;
    while (index < encoded.size())
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, message.payload.push_back(
            encoded.data()[index]));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.append_u8(55U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        networking_replication_decode_message(message, envelope, payload));
    FT_ASSERT_EQ(original.message_type, envelope.message_type);
    FT_ASSERT_EQ(original.session_id, envelope.session_id);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1U), payload.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message.payload.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.destroy());
    return (1);
}

FT_TEST(test_networking_replication_hash_payload_is_stable_and_content_based)
{
    ft_byte_buffer first;
    ft_byte_buffer second;
    uint8_t first_digest[32] = {0U};
    uint8_t second_digest[32] = {0U};
    uint8_t changed_digest[32] = {0U};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.append_u8(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.append_u8(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.append_u8(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.append_u8(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_hash_payload(first, first_digest));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_hash_payload(second, second_digest));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.append_u8(3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_hash_payload(first, changed_digest));
    FT_ASSERT_EQ(FT_TRUE, ft_memcmp(first_digest, second_digest, 32U) == 0);
    FT_ASSERT_EQ(FT_FALSE, ft_memcmp(first_digest, changed_digest, 32U) == 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_networking_replication_hash_payload_rejects_invalid_pointer)
{
    ft_byte_buffer payload;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER,
        networking_replication_hash_payload(payload, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.destroy());
    return (1);
}

FT_TEST(test_networking_replication_peer_cursor_round_trip_is_transactional)
{
    networking_replication_peer_cursor source;
    networking_replication_peer_cursor restored;
    ft_byte_buffer encoded;
    ft_byte_buffer truncated;
    ft_size_t initial_read_position;

    source.server_instance_id = 11U;
    source.session_id = 22U;
    source.subscription_id = 33U;
    source.block_revision = 44U;
    source.light_revision = 55U;
    source.snapshot_generation = 66U;
    source.snapshot_acknowledged = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_peer_cursor_serialize(source, encoded));
    FT_ASSERT_EQ(static_cast<ft_size_t>(NETWORKING_REPLICATION_CURSOR_SIZE),
        encoded.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.append(encoded.data(),
        encoded.size() - 1U));
    restored.server_instance_id = 99U;
    initial_read_position = truncated.read_position();
    FT_ASSERT_NE(FT_ERR_SUCCESS,
        networking_replication_peer_cursor_deserialize(restored, truncated));
    FT_ASSERT_EQ(99U, restored.server_instance_id);
    FT_ASSERT_EQ(initial_read_position, truncated.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.reset_read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_peer_cursor_deserialize(restored, encoded));
    FT_ASSERT_EQ(source.server_instance_id, restored.server_instance_id);
    FT_ASSERT_EQ(source.session_id, restored.session_id);
    FT_ASSERT_EQ(source.subscription_id, restored.subscription_id);
    FT_ASSERT_EQ(source.block_revision, restored.block_revision);
    FT_ASSERT_EQ(source.light_revision, restored.light_revision);
    FT_ASSERT_EQ(source.snapshot_generation, restored.snapshot_generation);
    FT_ASSERT_EQ(source.snapshot_acknowledged,
        restored.snapshot_acknowledged);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.destroy());
    return (1);
}
