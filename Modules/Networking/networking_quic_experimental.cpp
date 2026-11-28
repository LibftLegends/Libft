#include "networking_quic_experimental.hpp"
#include "openssl_support.hpp"
#include "../Errno/errno_internal.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "networking_tls_aead.hpp"
#if NETWORKING_HAS_OPENSSL

#include <atomic>

static std::atomic<ft_bool> g_quic_experimental_enabled(FT_FALSE);

static const ft_size_t QUIC_EXPERIMENTAL_TAG_LENGTH = 16;

quic_feature_configuration::quic_feature_configuration() noexcept
{
    this->enable_datagram_pacing = FT_FALSE;
    this->enable_loss_recovery = FT_FALSE;
    return ;
}

quic_feature_configuration::~quic_feature_configuration() noexcept
{
    return ;
}

quic_datagram_plaintext::quic_datagram_plaintext() noexcept
{
    this->payload = ft_nullptr;
    this->payload_length = 0;
    this->associated_data = ft_nullptr;
    this->associated_data_length = 0;
    return ;
}

quic_datagram_plaintext::~quic_datagram_plaintext() noexcept
{
    return ;
}

ft_bool    networking_quic_enable_experimental() noexcept
{
    g_quic_experimental_enabled.store(FT_TRUE);
    return (FT_TRUE);
}

ft_bool    networking_quic_disable_experimental() noexcept
{
    g_quic_experimental_enabled.store(FT_FALSE);
    return (FT_TRUE);
}

ft_bool    networking_quic_is_experimental_enabled() noexcept
{
    ft_bool enabled;

    enabled = g_quic_experimental_enabled.load();
    return (enabled);
}

quic_experimental_session::quic_experimental_session() noexcept
{
    this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    this->_ssl_session = ft_nullptr;
    this->_outbound = FT_FALSE;
    this->_configured = FT_FALSE;
    this->_send_sequence = 0;
    this->_receive_sequence = 0;
    this->_feature_configuration = quic_feature_configuration();
    return ;
}

quic_experimental_session::~quic_experimental_session() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t quic_experimental_session::initialize() noexcept
{
    int32_t vector_initialise_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "quic_experimental_session::initialize", "initialize called on initialised instance");
    this->_ssl_session = ft_nullptr;
    this->_outbound = FT_FALSE;
    this->_configured = FT_FALSE;
    this->_send_sequence = 0;
    this->_receive_sequence = 0;
    vector_initialise_error = this->_send_key.initialize();
    if (vector_initialise_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialise_error);
    }
    vector_initialise_error = this->_receive_key.initialize();
    if (vector_initialise_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialise_error);
    }
    vector_initialise_error = this->_send_iv.initialize();
    if (vector_initialise_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialise_error);
    }
    vector_initialise_error = this->_receive_iv.initialize();
    if (vector_initialise_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialise_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->_feature_configuration = quic_feature_configuration();
    this->clear_key_material();
    return (FT_ERR_SUCCESS);
}

int32_t quic_experimental_session::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->clear_key_material();
    this->_ssl_session = ft_nullptr;
    this->_outbound = FT_FALSE;
    this->_configured = FT_FALSE;
    this->_send_sequence = 0;
    this->_receive_sequence = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t quic_experimental_session::move(quic_experimental_session &other) noexcept
{
    return (this->initialize(static_cast<quic_experimental_session &&>(other)));
}

int32_t quic_experimental_session::initialize(const quic_experimental_session &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "quic_experimental_session::initialize(const quic_experimental_session &)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    return (this->initialize());
}

int32_t quic_experimental_session::initialize(quic_experimental_session &&other) noexcept
{
    int32_t initialize_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    initialize_error = this->initialize(other);
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

void    quic_experimental_session::clear_key_material() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "quic_experimental_session::clear_key_material");
    if (this->_send_key.size() > 0)
        ft_memset(this->_send_key.begin(), 0, this->_send_key.size());
    if (this->_receive_key.size() > 0)
        ft_memset(this->_receive_key.begin(), 0, this->_receive_key.size());
    if (this->_send_iv.size() > 0)
        ft_memset(this->_send_iv.begin(), 0, this->_send_iv.size());
    if (this->_receive_iv.size() > 0)
        ft_memset(this->_receive_iv.begin(), 0, this->_receive_iv.size());
    this->_send_key.clear();
    this->_receive_key.clear();
    this->_send_iv.clear();
    this->_receive_iv.clear();
    return ;
}

ft_bool    quic_experimental_session::ensure_feature_enabled() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "quic_experimental_session::ensure_feature_enabled");
    if (!networking_quic_is_experimental_enabled())
        return (FT_FALSE);
    return (FT_TRUE);
}

ft_bool    quic_experimental_session::ensure_configured() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "quic_experimental_session::ensure_configured");
    if (!this->_configured)
        return (FT_FALSE);
    return (FT_TRUE);
}

ft_bool    quic_experimental_session::derive_keys(SSL *ssl_session, ft_bool outbound) noexcept
{
    int32_t initialise_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "quic_experimental_session::derive_keys");
    if (this->_send_key.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        initialise_error = this->_send_key.initialize();
        if (initialise_error != FT_ERR_SUCCESS)
            return (FT_FALSE);
    }
    if (this->_send_iv.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        initialise_error = this->_send_iv.initialize();
        if (initialise_error != FT_ERR_SUCCESS)
            return (FT_FALSE);
    }
    if (this->_receive_key.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        initialise_error = this->_receive_key.initialize();
        if (initialise_error != FT_ERR_SUCCESS)
            return (FT_FALSE);
    }
    if (this->_receive_iv.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        initialise_error = this->_receive_iv.initialize();
        if (initialise_error != FT_ERR_SUCCESS)
            return (FT_FALSE);
    }
    if (!networking_tls_export_aead_keys(ssl_session, outbound,
            this->_send_key, this->_send_iv, this->_receive_key, this->_receive_iv))
        return (FT_FALSE);
    if (this->_send_key.size() == 0 || this->_receive_key.size() == 0)
        return (FT_FALSE);
    if (this->_send_iv.size() == 0 || this->_receive_iv.size() == 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

ft_size_t quic_experimental_session::prepare_nonce(uint64_t sequence_number,
        const ft_vector<unsigned char> &base_iv,
        unsigned char (&out_nonce)[EVP_MAX_IV_LENGTH]) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "quic_experimental_session::prepare_nonce");
    const ft_size_t nonce_length = base_iv.size();
    if (nonce_length == 0U || nonce_length > sizeof(out_nonce))
        return (0U);
    ft_memcpy(out_nonce, base_iv.begin(), nonce_length);
    ft_size_t index = nonce_length;
    while (index > 0U && sequence_number != 0U)
    {
        index -= 1U;
        out_nonce[index] ^= static_cast<unsigned char>(sequence_number & 0xFFU);
        sequence_number >>= 8U;
    }
    return (nonce_length);
}

ft_bool    quic_experimental_session::configure(SSL *ssl_session,
        const quic_feature_configuration &configuration,
        ft_bool outbound) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "quic_experimental_session::configure");
    if (!this->ensure_feature_enabled())
        return (FT_FALSE);
    if (ssl_session == ft_nullptr)
        return (FT_FALSE);
    if (SSL_is_init_finished(ssl_session) != 1)
        return (FT_FALSE);
    this->clear_key_material();
    if (!this->derive_keys(ssl_session, outbound))
        return (FT_FALSE);
    this->_ssl_session = ssl_session;
    this->_outbound = outbound;
    this->_configured = FT_TRUE;
    this->_feature_configuration = configuration;
    this->_send_sequence = 0;
    this->_receive_sequence = 0;
    return (FT_TRUE);
}

ft_bool    quic_experimental_session::encrypt_datagram(const quic_datagram_plaintext &plaintext,
        ft_vector<unsigned char> &out_ciphertext) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "quic_experimental_session::encrypt_datagram");
    if (!this->ensure_feature_enabled())
        return (FT_FALSE);
    if (!this->ensure_configured())
        return (FT_FALSE);
    if (this->_send_sequence == UINT64_MAX)
        return (FT_FALSE);
    if (out_ciphertext.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        if (out_ciphertext.initialize() != FT_ERR_SUCCESS)
            return (FT_FALSE);
    }
    const ft_size_t payload_length = plaintext.payload_length;
    if (payload_length > FT_SYSTEM_SIZE_MAX - QUIC_EXPERIMENTAL_TAG_LENGTH
        || (payload_length > 0U && plaintext.payload == ft_nullptr))
        return (FT_FALSE);
    if (plaintext.associated_data_length > 0 && plaintext.associated_data == ft_nullptr)
        return (FT_FALSE);
    unsigned char nonce[EVP_MAX_IV_LENGTH];
    const ft_size_t nonce_length = this->prepare_nonce(this->_send_sequence,
        this->_send_iv, nonce);
    if (nonce_length == 0U)
        return (FT_FALSE);
    out_ciphertext.resize(payload_length + QUIC_EXPERIMENTAL_TAG_LENGTH, 0);
    if (out_ciphertext.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    unsigned char *ciphertext_pointer = out_ciphertext.begin();
    unsigned char *tag_pointer = ciphertext_pointer + payload_length;
    if (!encryption_aead_encrypt(this->_send_key.begin(), this->_send_key.size(),
            nonce, nonce_length, plaintext.associated_data,
            plaintext.associated_data_length, plaintext.payload, payload_length,
            ciphertext_pointer, tag_pointer, QUIC_EXPERIMENTAL_TAG_LENGTH))
        return (FT_FALSE);
    this->_send_sequence += 1;
    return (FT_TRUE);
}

ft_bool    quic_experimental_session::decrypt_datagram(const ft_vector<unsigned char> &ciphertext,
        const unsigned char *associated_data,
        ft_size_t associated_data_length,
        ft_vector<unsigned char> &out_plaintext) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "quic_experimental_session::decrypt_datagram");
    if (!this->ensure_feature_enabled())
        return (FT_FALSE);
    if (!this->ensure_configured())
        return (FT_FALSE);
    if (this->_receive_sequence == UINT64_MAX)
        return (FT_FALSE);
    if (out_plaintext.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        if (out_plaintext.initialize() != FT_ERR_SUCCESS)
            return (FT_FALSE);
    }
    const ft_size_t ciphertext_length = ciphertext.size();
    if (ciphertext_length < QUIC_EXPERIMENTAL_TAG_LENGTH)
        return (FT_FALSE);
    if (associated_data_length > 0 && associated_data == ft_nullptr)
        return (FT_FALSE);
    unsigned char nonce[EVP_MAX_IV_LENGTH];
    const ft_size_t nonce_length = this->prepare_nonce(this->_receive_sequence,
        this->_receive_iv, nonce);
    if (nonce_length == 0U)
        return (FT_FALSE);
    const ft_size_t payload_length = ciphertext_length - QUIC_EXPERIMENTAL_TAG_LENGTH;
    const unsigned char *ciphertext_pointer = ciphertext.begin();
    const unsigned char *tag_pointer = ciphertext_pointer + payload_length;
    if (&ciphertext != &out_plaintext)
    {
        out_plaintext.resize(payload_length, 0);
        if (out_plaintext.get_error() != FT_ERR_SUCCESS)
            return (FT_FALSE);
    }
    if (!encryption_aead_decrypt(this->_receive_key.begin(), this->_receive_key.size(),
            nonce, nonce_length, associated_data, associated_data_length,
            ciphertext_pointer, payload_length, tag_pointer,
            QUIC_EXPERIMENTAL_TAG_LENGTH, out_plaintext.begin()))
        return (FT_FALSE);
    if (&ciphertext == &out_plaintext)
    {
        out_plaintext.resize(payload_length, 0);
        if (out_plaintext.get_error() != FT_ERR_SUCCESS)
            return (FT_FALSE);
    }
    this->_receive_sequence += 1;
    return (FT_TRUE);
}

ft_bool    quic_experimental_session::get_feature_configuration(quic_feature_configuration &out_configuration) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "quic_experimental_session::get_feature_configuration");
    if (!this->ensure_configured())
        return (FT_FALSE);
    out_configuration = this->_feature_configuration;
    return (FT_TRUE);
}

#endif
