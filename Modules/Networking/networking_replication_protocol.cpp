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

template <ft_bool BigEndian, typename Value>
static void replication_store_integer(uint8_t *output, Value value) noexcept
{
    ft_size_t index = 0U;
    while (index < sizeof(Value))
    {
        ft_size_t shift_index = index;
        if constexpr (BigEndian)
            shift_index = sizeof(Value) - 1U - index;
        output[index] = static_cast<uint8_t>(value >> (shift_index * 8U));
        ++index;
    }
    return ;
}

template <typename Value, ft_bool BigEndian = FT_FALSE>
static Value replication_load_integer(const uint8_t *input) noexcept
{
    uint64_t value = 0U;
    ft_size_t index = 0U;
    while (index < sizeof(Value))
    {
        ft_size_t shift_index = index;
        if constexpr (BigEndian)
            shift_index = sizeof(Value) - 1U - index;
        value |= static_cast<uint64_t>(input[index]) << (shift_index * 8U);
        ++index;
    }
    return (static_cast<Value>(value));
}

static ft_bool replication_envelope_is_valid(const networking_replication_envelope &envelope) noexcept
{
    return (envelope.protocol_version == NETWORKING_REPLICATION_PROTOCOL_VERSION
        && envelope.payload_size <= NETWORKING_REPLICATION_MAX_PAYLOAD
        && envelope.server_instance_id != 0U && envelope.session_id != 0U
        && envelope.message_sequence != 0U);
}

static void replication_decode_envelope(const uint8_t *encoded,
    networking_replication_envelope &envelope) noexcept
{
    envelope.protocol_version = replication_load_integer<uint16_t>(encoded);
    envelope.message_type = replication_load_integer<uint16_t>(encoded + 2U);
    envelope.payload_size = replication_load_integer<uint32_t>(encoded + 4U);
    envelope.server_instance_id = replication_load_integer<uint64_t>(encoded + 8U);
    envelope.session_id = replication_load_integer<uint64_t>(encoded + 16U);
    envelope.message_sequence = replication_load_integer<uint64_t>(encoded + 24U);
    return ;
}

int32_t networking_replication_envelope_serialize(
    const networking_replication_envelope &envelope,
    ft_byte_buffer &buffer) noexcept
{
    if (!replication_envelope_is_valid(envelope))
        return (FT_ERR_INVALID_ARGUMENT);
    uint8_t encoded[NETWORKING_REPLICATION_ENVELOPE_SIZE];
    replication_store_integer<FT_FALSE>(encoded, envelope.protocol_version);
    replication_store_integer<FT_FALSE>(encoded + 2U, envelope.message_type);
    replication_store_integer<FT_FALSE>(encoded + 4U, envelope.payload_size);
    replication_store_integer<FT_FALSE>(encoded + 8U, envelope.server_instance_id);
    replication_store_integer<FT_FALSE>(encoded + 16U, envelope.session_id);
    replication_store_integer<FT_FALSE>(encoded + 24U, envelope.message_sequence);
    return (buffer.append(encoded, sizeof(encoded)));
}

int32_t networking_replication_envelope_deserialize(
    networking_replication_envelope &envelope,
    ft_byte_buffer &buffer) noexcept
{
    uint8_t encoded[NETWORKING_REPLICATION_ENVELOPE_SIZE];
    int32_t error_code = buffer.peek(0U, encoded, sizeof(encoded));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    networking_replication_envelope temporary_envelope;
    replication_decode_envelope(encoded, temporary_envelope);
    const ft_size_t remaining = buffer.remaining();
    if (!replication_envelope_is_valid(temporary_envelope)
        || remaining < sizeof(encoded)
        || temporary_envelope.payload_size > remaining - sizeof(encoded))
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.skip(sizeof(encoded));
    if (error_code == FT_ERR_SUCCESS)
        envelope = temporary_envelope;
    return (error_code);
}

int32_t networking_replication_decode_message(
    const networking_received_message &message,
    networking_replication_envelope &envelope,
    ft_byte_buffer &payload) noexcept
{
    if (!message.payload.is_initialised() || !payload.is_initialised())
        return (FT_ERR_NOT_INITIALISED);
    const ft_size_t message_size = message.payload.size();
    if (message_size < NETWORKING_REPLICATION_ENVELOPE_SIZE)
        return (FT_ERR_INVALID_ARGUMENT);
    const uint8_t *encoded = message.payload.begin();
    networking_replication_envelope temporary_envelope;
    replication_decode_envelope(encoded, temporary_envelope);
    const ft_size_t payload_size = message_size - NETWORKING_REPLICATION_ENVELOPE_SIZE;
    if (!replication_envelope_is_valid(temporary_envelope)
        || temporary_envelope.payload_size != payload_size)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_byte_buffer decoded_payload;
    int32_t error_code = decoded_payload.initialize(payload_size, FT_FALSE);
    if (error_code == FT_ERR_SUCCESS && payload_size != 0U)
        error_code = decoded_payload.append(encoded + NETWORKING_REPLICATION_ENVELOPE_SIZE, payload_size);
    if (error_code == FT_ERR_SUCCESS)
        error_code = payload.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = payload.move(decoded_payload);
    if (error_code == FT_ERR_SUCCESS)
        envelope = temporary_envelope;
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
    if (cursor.server_instance_id == 0U || cursor.session_id == 0U
        || cursor.subscription_id == 0U
        || (cursor.snapshot_acknowledged != FT_FALSE && cursor.snapshot_acknowledged != FT_TRUE))
        return (FT_ERR_INVALID_ARGUMENT);
    uint8_t encoded[NETWORKING_REPLICATION_CURSOR_SIZE];
    replication_store_integer<FT_TRUE>(encoded,
        static_cast<uint16_t>(NETWORKING_REPLICATION_CURSOR_VERSION));
    replication_store_integer<FT_TRUE>(encoded + 2U, static_cast<uint16_t>(cursor.snapshot_acknowledged));
    replication_store_integer<FT_TRUE>(encoded + 4U, static_cast<uint32_t>(0U));
    replication_store_integer<FT_TRUE>(encoded + 8U, cursor.server_instance_id);
    replication_store_integer<FT_TRUE>(encoded + 16U, cursor.session_id);
    replication_store_integer<FT_TRUE>(encoded + 24U, cursor.subscription_id);
    replication_store_integer<FT_TRUE>(encoded + 32U, cursor.block_revision);
    replication_store_integer<FT_TRUE>(encoded + 40U, cursor.light_revision);
    replication_store_integer<FT_TRUE>(encoded + 48U, cursor.snapshot_generation);
    return (buffer.append(encoded, sizeof(encoded)));
}

int32_t networking_replication_peer_cursor_deserialize(
    networking_replication_peer_cursor &cursor,
    ft_byte_buffer &buffer) noexcept
{
    uint8_t encoded[NETWORKING_REPLICATION_CURSOR_SIZE];
    int32_t error_code = buffer.peek(0U, encoded, sizeof(encoded));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const uint16_t version = replication_load_integer<uint16_t, FT_TRUE>(encoded);
    const uint16_t flags = replication_load_integer<uint16_t, FT_TRUE>(encoded + 2U);
    const uint32_t reserved = replication_load_integer<uint32_t, FT_TRUE>(encoded + 4U);
    networking_replication_peer_cursor decoded;
    decoded.server_instance_id = replication_load_integer<uint64_t, FT_TRUE>(encoded + 8U);
    decoded.session_id = replication_load_integer<uint64_t, FT_TRUE>(encoded + 16U);
    decoded.subscription_id = replication_load_integer<uint64_t, FT_TRUE>(encoded + 24U);
    decoded.block_revision = replication_load_integer<uint64_t, FT_TRUE>(encoded + 32U);
    decoded.light_revision = replication_load_integer<uint64_t, FT_TRUE>(encoded + 40U);
    decoded.snapshot_generation = replication_load_integer<uint64_t, FT_TRUE>(encoded + 48U);
    if (version != NETWORKING_REPLICATION_CURSOR_VERSION || reserved != 0U
        || flags > 1U || decoded.server_instance_id == 0U || decoded.session_id == 0U
        || decoded.subscription_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    decoded.snapshot_acknowledged = static_cast<ft_bool>(flags);
    error_code = buffer.skip(sizeof(encoded));
    if (error_code == FT_ERR_SUCCESS)
        cursor = decoded;
    return (error_code);
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

    const ft_size_t payload_size = payload.size();
    if (payload_size > NETWORKING_REPLICATION_MAX_PAYLOAD)
        return (FT_ERR_OUT_OF_RANGE);
    envelope.message_type = message_type;
    envelope.payload_size = static_cast<uint32_t>(payload_size);
    envelope.server_instance_id = server_instance_id;
    envelope.session_id = session_id;
    envelope.message_sequence = message_sequence;
    if (!replication_envelope_is_valid(envelope))
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = message.initialize(NETWORKING_REPLICATION_ENVELOPE_SIZE + payload_size, FT_FALSE);
    if (error_code == FT_ERR_SUCCESS)
        error_code = networking_replication_envelope_serialize(envelope, message);
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
