#ifndef NETWORKING_REPLICATION_CLIENT_HPP
#define NETWORKING_REPLICATION_CLIENT_HPP

#include "networking_replication_protocol.hpp"

typedef int32_t (*networking_replication_apply_callback)(
    const ft_byte_buffer &payload, void *user_data) noexcept;

class networking_replication_client
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        uint64_t _server_instance_id;
        uint64_t _session_id;
        networking_replication_revision_tracker _revision_tracker;
        networking_replication_apply_budget _apply_budget;
        networking_replication_apply_callback _snapshot_callback;
        networking_replication_apply_callback _block_delta_callback;
        networking_replication_apply_callback _light_delta_callback;
        void *_callback_user_data;

        int32_t apply_payload(const ft_byte_buffer &payload,
            uint32_t operations, networking_replication_apply_callback callback)
            noexcept;

    public:
        networking_replication_client() noexcept;
        networking_replication_client(
            const networking_replication_client &other) noexcept = delete;
        networking_replication_client(networking_replication_client &&other)
            noexcept = delete;
        ~networking_replication_client() noexcept;

        networking_replication_client &operator=(
            const networking_replication_client &other) noexcept = delete;
        networking_replication_client &operator=(
            networking_replication_client &&other) noexcept = delete;

        int32_t initialize(uint64_t server_instance_id, uint64_t session_id,
            uint32_t maximum_messages, uint32_t maximum_payload_bytes,
            uint32_t maximum_operations) noexcept;
        int32_t destroy() noexcept;
        int32_t set_callbacks(networking_replication_apply_callback snapshot,
            networking_replication_apply_callback block_delta,
            networking_replication_apply_callback light_delta,
            void *user_data) noexcept;
        int32_t reset_budget() noexcept;
        int32_t apply_snapshot(uint64_t block_revision,
            uint64_t light_revision, uint64_t snapshot_generation,
            const ft_byte_buffer &payload, uint32_t operations) noexcept;
        int32_t apply_block_delta(uint64_t base_revision,
            uint64_t final_revision, const ft_byte_buffer &payload,
            uint32_t operations) noexcept;
        int32_t apply_light_delta(uint64_t base_revision,
            uint64_t final_revision, uint64_t source_block_revision,
            const ft_byte_buffer &payload, uint32_t operations) noexcept;
        uint64_t get_block_revision() const noexcept;
        uint64_t get_light_revision() const noexcept;
        uint64_t get_snapshot_generation() const noexcept;
        ft_bool is_snapshot_ready() const noexcept;
};

#endif
