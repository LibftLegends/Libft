#include "networking_replication_protocol.hpp"
#include "../Crypto/crypto_primitives.hpp"

networking_replication_envelope::networking_replication_envelope() noexcept
    : protocol_version(NETWORKING_REPLICATION_PROTOCOL_VERSION),
      message_type(0U), payload_size(0U), server_instance_id(0U),
      session_id(0U), message_sequence(0U)
{
    return ;
}

networking_replication_envelope::~networking_replication_envelope() noexcept
{
    return ;
}

int32_t networking_replication_envelope_serialize(
    const networking_replication_envelope &envelope,
    ft_byte_buffer &buffer) noexcept
{
    int32_t error_code;

    if (envelope.protocol_version != NETWORKING_REPLICATION_PROTOCOL_VERSION
        || envelope.payload_size > NETWORKING_REPLICATION_MAX_PAYLOAD
        || envelope.server_instance_id == 0U
        || envelope.session_id == 0U
        || envelope.message_sequence == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.append_u16_le(envelope.protocol_version);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u16_le(envelope.message_type);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u32_le(envelope.payload_size);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(envelope.server_instance_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(envelope.session_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(envelope.message_sequence);
    return (error_code);
}

int32_t networking_replication_envelope_deserialize(
    networking_replication_envelope &envelope,
    ft_byte_buffer &buffer) noexcept
{
    networking_replication_envelope temporary_envelope;
    ft_size_t initial_read_position;
    int32_t error_code;
    int32_t restore_error;

    initial_read_position = buffer.read_position();
    error_code = buffer.read_u16_le(&temporary_envelope.protocol_version);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u16_le(&temporary_envelope.message_type);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&temporary_envelope.payload_size);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&temporary_envelope.server_instance_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&temporary_envelope.session_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&temporary_envelope.message_sequence);
    if (error_code != FT_ERR_SUCCESS)
    {
        restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (error_code);
    }
    if (temporary_envelope.protocol_version
            != NETWORKING_REPLICATION_PROTOCOL_VERSION
        || temporary_envelope.payload_size > NETWORKING_REPLICATION_MAX_PAYLOAD
        || temporary_envelope.server_instance_id == 0U
        || temporary_envelope.session_id == 0U
        || temporary_envelope.message_sequence == 0U
        || static_cast<ft_size_t>(temporary_envelope.payload_size)
            > buffer.remaining())
    {
        restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    envelope = temporary_envelope;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_decode_message(
    const networking_received_message &message,
    networking_replication_envelope &envelope,
    ft_byte_buffer &payload) noexcept
{
    ft_byte_buffer encoded_message;
    ft_byte_buffer decoded_payload;
    networking_replication_envelope temporary_envelope;
    ft_size_t payload_offset;
    int32_t error_code;
    int32_t destroy_error;

    if (message.payload.is_initialised() == FT_FALSE
        || payload.is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (message.payload.size() < NETWORKING_REPLICATION_ENVELOPE_SIZE)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = encoded_message.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = encoded_message.append(&message.payload[0U],
            message.payload.size());
    if (error_code == FT_ERR_SUCCESS)
        error_code = networking_replication_envelope_deserialize(
            temporary_envelope, encoded_message);
    if (error_code == FT_ERR_SUCCESS)
    {
        payload_offset = encoded_message.read_position();
        if (temporary_envelope.payload_size != encoded_message.remaining()
            || static_cast<ft_size_t>(temporary_envelope.payload_size)
                != message.payload.size() - payload_offset)
            error_code = FT_ERR_INVALID_ARGUMENT;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = decoded_payload.initialize();
    if (error_code == FT_ERR_SUCCESS && temporary_envelope.payload_size > 0U)
        error_code = decoded_payload.append(&message.payload[payload_offset],
            temporary_envelope.payload_size);
    if (error_code == FT_ERR_SUCCESS)
    {
        destroy_error = payload.destroy();
        if (destroy_error == FT_ERR_SUCCESS)
            error_code = payload.move(decoded_payload);
        else
            error_code = destroy_error;
    }
    if (error_code == FT_ERR_SUCCESS)
        envelope = temporary_envelope;
    destroy_error = decoded_payload.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = destroy_error;
    destroy_error = encoded_message.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = destroy_error;
    return (error_code);
}

int32_t networking_replication_hash_payload(const ft_byte_buffer &payload,
    uint8_t digest[32]) noexcept
{
    const uint8_t *payload_data;

    if (payload.is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (digest == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    payload_data = payload.data();
    return (crypto_sha256_hash(payload_data, payload.size(), digest));
}

networking_replication_peer_cursor::networking_replication_peer_cursor() noexcept
    : server_instance_id(0U), session_id(0U), subscription_id(0U),
      block_revision(0U), light_revision(0U), snapshot_generation(0U),
      snapshot_acknowledged(FT_FALSE)
{
    return ;
}

networking_replication_peer_cursor::~networking_replication_peer_cursor() noexcept
{
    return ;
}

int32_t networking_replication_peer_cursor_serialize(
    const networking_replication_peer_cursor &cursor,
    ft_byte_buffer &buffer) noexcept
{
    ft_byte_buffer encoded;
    int32_t error_code;
    int32_t destroy_error;

    if (cursor.server_instance_id == 0U || cursor.session_id == 0U
        || cursor.subscription_id == 0U
        || (cursor.snapshot_acknowledged != FT_FALSE
            && cursor.snapshot_acknowledged != FT_TRUE))
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = encoded.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = encoded.append_u16_be(
            static_cast<uint16_t>(NETWORKING_REPLICATION_CURSOR_VERSION));
    if (error_code == FT_ERR_SUCCESS)
        error_code = encoded.append_u16_be(
            cursor.snapshot_acknowledged == FT_TRUE ? 1U : 0U);
    if (error_code == FT_ERR_SUCCESS)
        error_code = encoded.append_u32_be(0U);
    if (error_code == FT_ERR_SUCCESS)
        error_code = encoded.append_u64_be(cursor.server_instance_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = encoded.append_u64_be(cursor.session_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = encoded.append_u64_be(cursor.subscription_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = encoded.append_u64_be(cursor.block_revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = encoded.append_u64_be(cursor.light_revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = encoded.append_u64_be(cursor.snapshot_generation);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_buffer(encoded);
    destroy_error = encoded.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = destroy_error;
    return (error_code);
}

int32_t networking_replication_peer_cursor_deserialize(
    networking_replication_peer_cursor &cursor,
    ft_byte_buffer &buffer) noexcept
{
    networking_replication_peer_cursor decoded;
    ft_size_t initial_read_position;
    uint16_t version;
    uint16_t flags;
    uint32_t reserved;
    int32_t error_code;
    int32_t restore_error;

    initial_read_position = buffer.read_position();
    error_code = buffer.read_u16_be(&version);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u16_be(&flags);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_be(&reserved);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_be(&decoded.server_instance_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_be(&decoded.session_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_be(&decoded.subscription_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_be(&decoded.block_revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_be(&decoded.light_revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_be(&decoded.snapshot_generation);
    if (error_code != FT_ERR_SUCCESS)
    {
        restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (error_code);
    }
    if (version != NETWORKING_REPLICATION_CURSOR_VERSION || reserved != 0U
        || flags > 1U || decoded.server_instance_id == 0U
        || decoded.session_id == 0U || decoded.subscription_id == 0U)
    {
        restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    decoded.snapshot_acknowledged = flags == 1U ? FT_TRUE : FT_FALSE;
    cursor = decoded;
    return (FT_ERR_SUCCESS);
}

networking_replication_sender::networking_replication_sender() noexcept
{
    return ;
}

networking_replication_sender::~networking_replication_sender() noexcept
{
    return ;
}

int32_t networking_replication_sender::send_payload(
    networking_message_connection &connection, uint16_t message_type,
    const ft_byte_buffer &payload, networking_message_delivery delivery,
    uint8_t lane, uint32_t channel, uint64_t server_instance_id,
    uint64_t session_id, uint64_t message_sequence) const noexcept
{
    ft_byte_buffer message;
    networking_replication_envelope envelope;
    networking_message_send_options options;
    const uint8_t *message_data;
    int32_t error_code;
    int32_t destroy_error;

    if (payload.size() > NETWORKING_REPLICATION_MAX_PAYLOAD)
        return (FT_ERR_OUT_OF_RANGE);
    error_code = message.initialize();
    if (error_code == FT_ERR_SUCCESS)
    {
        envelope.message_type = message_type;
        envelope.payload_size = static_cast<uint32_t>(payload.size());
        envelope.server_instance_id = server_instance_id;
        envelope.session_id = session_id;
        envelope.message_sequence = message_sequence;
        error_code = networking_replication_envelope_serialize(envelope,
            message);
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = message.append_buffer(payload);
    if (error_code == FT_ERR_SUCCESS)
        error_code = message.view(0U, message.size(), &message_data);
    if (error_code == FT_ERR_SUCCESS)
    {
        options.delivery = delivery;
        options.lane = lane;
        options.channel = channel;
        error_code = connection.send_message(message_data, message.size(),
            options);
    }
    destroy_error = message.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = destroy_error;
    return (error_code);
}

int32_t networking_replication_sender::send_reliable_control(
    networking_message_connection &connection, uint16_t message_type,
    const ft_byte_buffer &payload, uint64_t server_instance_id,
    uint64_t session_id, uint64_t message_sequence) const noexcept
{
    return (this->send_payload(connection, message_type, payload,
        networking_message_delivery::RELIABLE_ORDERED, 0U, 0U,
        server_instance_id, session_id, message_sequence));
}

int32_t networking_replication_sender::send_reliable_delta(
    networking_message_connection &connection, uint16_t message_type,
    const ft_byte_buffer &payload, uint64_t server_instance_id,
    uint64_t session_id, uint64_t message_sequence) const noexcept
{
    return (this->send_payload(connection, message_type, payload,
        networking_message_delivery::RELIABLE_ORDERED, 1U, 1U,
        server_instance_id, session_id, message_sequence));
}

int32_t networking_replication_sender::send_reliable_snapshot(
    networking_message_connection &connection, uint16_t message_type,
    const ft_byte_buffer &payload, uint64_t server_instance_id,
    uint64_t session_id, uint64_t message_sequence) const noexcept
{
    return (this->send_payload(connection, message_type, payload,
        networking_message_delivery::RELIABLE_ORDERED, 2U, 2U,
        server_instance_id, session_id, message_sequence));
}

int32_t networking_replication_sender::send_unreliable_sequenced(
    networking_message_connection &connection, uint16_t message_type,
    const ft_byte_buffer &payload, uint64_t server_instance_id,
    uint64_t session_id, uint64_t message_sequence) const noexcept
{
    return (this->send_payload(connection, message_type, payload,
        networking_message_delivery::UNRELIABLE_SEQUENCED, 3U, 3U,
        server_instance_id, session_id, message_sequence));
}
