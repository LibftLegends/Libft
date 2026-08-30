#ifndef NETWORKING_SECURE_CHANNEL_HPP
#define NETWORKING_SECURE_CHANNEL_HPP

#include "../Basic/basic.hpp"
#include "../Template/vector.hpp"
#include "networking_crypto_backend.hpp"
#include <cstdint>

class networking_secure_channel
{
    private:
        uint8_t _initialised_state;
        uint8_t _send_initialization_vector[12];
        uint8_t _receive_initialization_vector[12];
        uint8_t _previous_receive_initialization_vector[12];
        uint8_t _send_key[32];
        uint8_t _receive_key[32];
        uint8_t _previous_receive_key[32];
        uint64_t _send_key_epoch;
        uint64_t _receive_key_epoch;
        uint64_t _previous_receive_key_epoch;
        uint64_t _highest_sent_packet;
        uint64_t _highest_received_packet;
        ft_bool _has_sent_packet;
        uint64_t _received_window;
        ft_bool _has_received_packet;
        uint64_t _previous_highest_received_packet;
        uint64_t _previous_received_window;
        ft_bool _has_previous_received_packet;
        ft_bool _has_previous_receive_key;
        networking_crypto_backend _send_backend;
        networking_crypto_backend _receive_backend;
        networking_crypto_backend _previous_receive_backend;

        ft_bool prepare_nonce(const uint8_t initialization_vector[12],
            uint64_t packet_number,
            uint8_t nonce[12]) const noexcept;
        ft_bool accepts_packet_number(uint64_t packet_number) const noexcept;
        void record_packet_number(uint64_t packet_number) noexcept;
        ft_bool accepts_previous_packet_number(uint64_t packet_number) const noexcept;
        void record_previous_packet_number(uint64_t packet_number) noexcept;

    public:
        networking_secure_channel() noexcept;
        networking_secure_channel(const networking_secure_channel &other) noexcept = delete;
        networking_secure_channel(networking_secure_channel &&other) noexcept = delete;
        ~networking_secure_channel() noexcept;

        networking_secure_channel &operator=(const networking_secure_channel &other) noexcept = delete;
        networking_secure_channel &operator=(networking_secure_channel &&other) noexcept = delete;

        int32_t initialize(const uint8_t *key, ft_size_t key_length,
            const uint8_t *initialization_vector, ft_size_t initialization_vector_length) noexcept;
        int32_t initialize_directional(const uint8_t send_key[32],
            const uint8_t receive_key[32],
            const uint8_t send_initialization_vector[12],
            const uint8_t receive_initialization_vector[12]) noexcept;
        int32_t update_send_key_epoch(uint64_t next_epoch) noexcept;
        int32_t update_receive_key_epoch(uint64_t next_epoch) noexcept;
        int32_t clear_previous_receive_key() noexcept;
        int32_t update_key_epoch(uint64_t next_epoch) noexcept;
        uint64_t get_key_epoch() const noexcept;
        uint64_t get_send_key_epoch() const noexcept;
        uint64_t get_receive_key_epoch() const noexcept;
        int32_t destroy() noexcept;
        int32_t move(networking_secure_channel &other) noexcept;
        ft_bool seal(uint64_t packet_number, const uint8_t *associated_data,
            ft_size_t associated_data_length, const uint8_t *plaintext,
            ft_size_t plaintext_length, ft_vector<uint8_t> &ciphertext,
            uint8_t authentication_tag[16]) noexcept;
        ft_bool open(uint64_t packet_number, const uint8_t *associated_data,
            ft_size_t associated_data_length, const uint8_t *ciphertext,
            ft_size_t ciphertext_length, const uint8_t authentication_tag[16],
            ft_vector<uint8_t> &plaintext) noexcept;
        ft_bool open_at_epoch(uint64_t packet_number, uint64_t key_epoch,
            const uint8_t *associated_data, ft_size_t associated_data_length,
            const uint8_t *ciphertext, ft_size_t ciphertext_length,
            const uint8_t authentication_tag[16],
            ft_vector<uint8_t> &plaintext) noexcept;
        ft_bool has_receive_epoch(uint64_t key_epoch) const noexcept;
        ft_bool is_replay(uint64_t packet_number) const noexcept;
};

#endif
