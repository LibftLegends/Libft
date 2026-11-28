#ifndef NETWORKING_QUIC_EXPERIMENTAL_HPP
#define NETWORKING_QUIC_EXPERIMENTAL_HPP

#include "ssl_wrapper.hpp"
#include "networking_tls_aead.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Template/vector.hpp"
#include "../Basic/basic.hpp"
#include "../Encryption/encryption.hpp"
#include "openssl_support.hpp"

#if NETWORKING_HAS_OPENSSL

struct quic_feature_configuration
{
    ft_bool    enable_datagram_pacing;
    ft_bool    enable_loss_recovery;

    quic_feature_configuration() noexcept;
    ~quic_feature_configuration() noexcept;
};

struct quic_datagram_plaintext
{
    const unsigned char   *payload;
    ft_size_t             payload_length;
    const unsigned char   *associated_data;
    ft_size_t             associated_data_length;

    quic_datagram_plaintext() noexcept;
    ~quic_datagram_plaintext() noexcept;
};

ft_bool    networking_quic_enable_experimental() noexcept;
ft_bool    networking_quic_disable_experimental() noexcept;
ft_bool    networking_quic_is_experimental_enabled() noexcept;

class quic_experimental_session
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t                     _initialised_state;
        void        clear_key_material() noexcept;
        ft_bool     ensure_feature_enabled() const noexcept;
        ft_bool     ensure_configured() const noexcept;
        ft_bool     derive_keys(SSL *ssl_session, ft_bool outbound) noexcept;
        ft_size_t   prepare_nonce(uint64_t sequence_number,
                        const ft_vector<unsigned char> &base_iv,
                        unsigned char (&out_nonce)[EVP_MAX_IV_LENGTH]) noexcept;

        SSL                             *_ssl_session;
        ft_bool                         _outbound;
        ft_bool                         _configured;
        quic_feature_configuration      _feature_configuration;
        ft_vector<unsigned char>        _send_key;
        ft_vector<unsigned char>        _receive_key;
        ft_vector<unsigned char>        _send_iv;
        ft_vector<unsigned char>        _receive_iv;
        uint64_t                        _send_sequence;
        uint64_t                        _receive_sequence;

    public:
        quic_experimental_session() noexcept;
        ~quic_experimental_session() noexcept;
        quic_experimental_session(const quic_experimental_session &other) noexcept = delete;
        quic_experimental_session(quic_experimental_session &&other) noexcept = delete;
        quic_experimental_session &operator=(const quic_experimental_session &other) = delete;
        quic_experimental_session &operator=(quic_experimental_session &&other) noexcept = delete;
        int32_t move(quic_experimental_session &other) noexcept;

        int32_t initialize() noexcept;
        int32_t initialize(const quic_experimental_session &other) noexcept;
        int32_t initialize(quic_experimental_session &&other) noexcept;
        int32_t destroy() noexcept;
        ft_bool configure(SSL *ssl_session,
                    const quic_feature_configuration &configuration,
                    ft_bool outbound) noexcept;
        ft_bool encrypt_datagram(const quic_datagram_plaintext &plaintext,
                    ft_vector<unsigned char> &out_ciphertext) noexcept;
        ft_bool decrypt_datagram(const ft_vector<unsigned char> &ciphertext,
                    const unsigned char *associated_data,
                    ft_size_t associated_data_length,
                    ft_vector<unsigned char> &out_plaintext) noexcept;
        ft_bool get_feature_configuration(quic_feature_configuration &out_configuration) const noexcept;
};

#endif
#endif
