#ifndef NETWORKING_REPLICATION_PROTOCOL_HPP
#define NETWORKING_REPLICATION_PROTOCOL_HPP

#include "message_transport.hpp"
#include "../Buffer/byte_buffer.hpp"
#include "networking_replication_revision_tracker.hpp"
#include "networking_replication_apply_budget.hpp"
#include "networking_replication_retention_window.hpp"
#include <cstdint>

#define NETWORKING_REPLICATION_PROTOCOL_VERSION 1U
#define NETWORKING_REPLICATION_MAX_PAYLOAD (4U * 1024U * 1024U)
#define NETWORKING_REPLICATION_ENVELOPE_SIZE 32U
#define NETWORKING_REPLICATION_CURSOR_VERSION 1U
#define NETWORKING_REPLICATION_CURSOR_SIZE 56U

/*
 * Generic application envelope for authoritative replication protocols.
 * Message type and payload meaning belong to the consuming application (for
 * example Minecraft); Networking only validates framing and transports bytes.
 */
struct networking_replication_envelope
{
    uint16_t protocol_version;
    uint16_t message_type;
    uint32_t payload_size;
    uint64_t server_instance_id;
    uint64_t session_id;
    uint64_t message_sequence;

    networking_replication_envelope() noexcept;
    ~networking_replication_envelope() noexcept;
};

int32_t networking_replication_envelope_serialize(
    const networking_replication_envelope &envelope,
    ft_byte_buffer &buffer) noexcept;
int32_t networking_replication_envelope_deserialize(
    networking_replication_envelope &envelope,
    ft_byte_buffer &buffer) noexcept;

/*
 * Decode one received transport message without exposing a view into the
 * transport-owned payload.  The output envelope and payload are committed
 * only after the complete frame has passed validation. The received message
 * and destination payload must be initialised by their owners.
 */
int32_t networking_replication_decode_message(
    const networking_received_message &message,
    networking_replication_envelope &envelope,
    ft_byte_buffer &payload) noexcept;
int32_t networking_replication_hash_payload(const ft_byte_buffer &payload,
    uint8_t digest[32]) noexcept;

struct networking_replication_peer_cursor
{
    uint64_t server_instance_id;
    uint64_t session_id;
    uint64_t subscription_id;
    uint64_t block_revision;
    uint64_t light_revision;
    uint64_t snapshot_generation;
    ft_bool snapshot_acknowledged;

    networking_replication_peer_cursor() noexcept;
    ~networking_replication_peer_cursor() noexcept;
};

int32_t networking_replication_peer_cursor_serialize(
    const networking_replication_peer_cursor &cursor,
    ft_byte_buffer &buffer) noexcept;
int32_t networking_replication_peer_cursor_deserialize(
    networking_replication_peer_cursor &cursor,
    ft_byte_buffer &buffer) noexcept;

/* Stateless transport utility: it owns no resources or mutable state, so it
 * intentionally does not expose lifecycle or optional thread-safety helpers.
 * All operation failures are returned directly to the caller. */
class networking_replication_sender
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        int32_t send_payload(networking_message_connection &connection,
            uint16_t message_type, const ft_byte_buffer &payload,
            networking_message_delivery delivery, uint8_t lane,
            uint32_t channel, uint64_t server_instance_id,
            uint64_t session_id, uint64_t message_sequence) const noexcept;

    public:
        networking_replication_sender() noexcept;
        networking_replication_sender(
            const networking_replication_sender &other) noexcept = delete;
        networking_replication_sender(networking_replication_sender &&other)
            noexcept = delete;
        ~networking_replication_sender() noexcept;

        networking_replication_sender &operator=(
            const networking_replication_sender &other) noexcept = delete;
        networking_replication_sender &operator=(
            networking_replication_sender &&other) noexcept = delete;

        int32_t send_reliable_control(networking_message_connection &connection,
            uint16_t message_type, const ft_byte_buffer &payload,
            uint64_t server_instance_id, uint64_t session_id,
            uint64_t message_sequence) const noexcept;
        int32_t send_reliable_delta(networking_message_connection &connection,
            uint16_t message_type, const ft_byte_buffer &payload,
            uint64_t server_instance_id, uint64_t session_id,
            uint64_t message_sequence) const noexcept;
        int32_t send_reliable_snapshot(networking_message_connection &connection,
            uint16_t message_type, const ft_byte_buffer &payload,
            uint64_t server_instance_id, uint64_t session_id,
            uint64_t message_sequence) const noexcept;
        int32_t send_unreliable_sequenced(networking_message_connection &connection,
            uint16_t message_type, const ft_byte_buffer &payload,
            uint64_t server_instance_id, uint64_t session_id,
            uint64_t message_sequence) const noexcept;
};

#endif
