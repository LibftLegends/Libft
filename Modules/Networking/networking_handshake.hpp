#ifndef NETWORKING_HANDSHAKE_HPP
#define NETWORKING_HANDSHAKE_HPP

#include "message_transport.hpp"
#include "networking_crypto_backend.hpp"
#include "../Basic/basic.hpp"
#include "../Template/vector.hpp"
#include <cstdint>

enum class networking_handshake_role : uint8_t
{
    CLIENT = 0U,
    SERVER = 1U
};

enum class networking_handshake_state : uint8_t
{
    IDLE = 0U,
    HELLO_READY = 1U,
    PEER_HELLO_ACCEPTED = 2U,
    KEYS_DERIVED = 3U,
    FINISHED = 4U,
    FAILED = 5U
};

class networking_handshake
{
    private:
        uint8_t _initialised_state;
        networking_handshake_role _role;
        networking_handshake_state _state;
        uint64_t _local_connection_id;
        uint64_t _peer_connection_id;
        uint8_t _local_private_key[32];
        uint8_t _local_public_key[32];
        uint8_t _peer_public_key[32];
        uint8_t _local_nonce[32];
        uint8_t _peer_nonce[32];
        uint8_t _send_key[32];
        uint8_t _receive_key[32];
        uint8_t _send_initialization_vector[12];
        uint8_t _receive_initialization_vector[12];
        uint8_t _retry_cookie[40];
        ft_bool _has_retry_cookie;
        networking_crypto_backend _crypto_backend;
        ft_vector<uint8_t> _local_hello;
        ft_vector<uint8_t> _peer_hello;
        ft_vector<uint8_t> _transcript;
        ft_bool _keys_derived;
        ft_bool _finished_verified;

        int32_t build_local_hello() noexcept;
        int32_t build_transcript() noexcept;
        int32_t append_u64(ft_vector<uint8_t> &buffer, uint64_t value) noexcept;

    public:
        networking_handshake() noexcept;
        networking_handshake(const networking_handshake &other) noexcept = delete;
        networking_handshake(networking_handshake &&other) noexcept = delete;
        ~networking_handshake() noexcept;

        networking_handshake &operator=(const networking_handshake &other) noexcept = delete;
        networking_handshake &operator=(networking_handshake &&other) noexcept = delete;

        int32_t initialize(networking_handshake_role role,
            uint64_t local_connection_id) noexcept;
        int32_t destroy() noexcept;
        int32_t move(networking_handshake &other) noexcept;
        int32_t get_local_hello(ft_vector<uint8_t> &hello) const noexcept;
        int32_t accept_peer_hello(const uint8_t *hello, ft_size_t hello_length) noexcept;
        int32_t derive_keys() noexcept;
        int32_t create_finished(uint8_t finished[32]) const noexcept;
        int32_t verify_finished(const uint8_t finished[32]) noexcept;
        int32_t get_traffic_keys(uint8_t send_key[32], uint8_t receive_key[32],
            uint8_t send_initialization_vector[12],
            uint8_t receive_initialization_vector[12]) const noexcept;
        int32_t set_retry_cookie(const uint8_t cookie[40]) noexcept;
        networking_handshake_state get_state() const noexcept;
        networking_handshake_role get_role() const noexcept;
        uint64_t get_peer_connection_id() const noexcept;
        int32_t get_peer_public_key(uint8_t public_key[32]) const noexcept;

        static int32_t hash_hello(const uint8_t *hello, ft_size_t hello_length,
            uint8_t digest[32]) noexcept;

        static int32_t create_retry_cookie(const uint8_t secret[32],
            const networking_message_endpoint &source,
            const uint8_t hello_digest[32], uint64_t issued_at,
            uint8_t cookie[40]) noexcept;
        static int32_t verify_retry_cookie(const uint8_t secret[32],
            const networking_message_endpoint &source,
            const uint8_t hello_digest[32], uint64_t now,
            uint64_t lifetime_milliseconds, const uint8_t cookie[40]) noexcept;
};

#endif
