#include "networking_secure_channel.hpp"

namespace
{
    static void networking_secure_wipe(void *data, ft_size_t size) noexcept
    {
        networking_crypto_backend backend;

        (void)backend.wipe(data, size);
        return ;
    }
}

networking_secure_channel::networking_secure_channel() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _send_initialization_vector(), _receive_initialization_vector(),
      _previous_receive_initialization_vector(), _send_key(), _receive_key(),
      _previous_receive_key(), _send_key_epoch(0U), _receive_key_epoch(0U),
      _previous_receive_key_epoch(0U),
      _highest_sent_packet(0U), _highest_received_packet(0U),
      _has_sent_packet(FT_FALSE), _received_window(0U),
      _has_received_packet(FT_FALSE), _previous_highest_received_packet(0U),
      _previous_received_window(0U), _has_previous_received_packet(FT_FALSE),
      _has_previous_receive_key(FT_FALSE), _send_backend(), _receive_backend(),
      _previous_receive_backend()
{
    return ;
}

networking_secure_channel::~networking_secure_channel() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_secure_channel::initialize(const uint8_t *key, ft_size_t key_length,
    const uint8_t *initialization_vector, ft_size_t initialization_vector_length) noexcept
{
    if (key == ft_nullptr || initialization_vector == ft_nullptr
        || key_length != 32U
        || initialization_vector_length != sizeof(this->_send_initialization_vector))
        return (FT_ERR_INVALID_ARGUMENT);
    return (this->initialize_directional(key, key, initialization_vector,
        initialization_vector));
}

int32_t networking_secure_channel::initialize_directional(
    const uint8_t send_key[32], const uint8_t receive_key[32],
    const uint8_t send_initialization_vector[12],
    const uint8_t receive_initialization_vector[12]) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (send_key == ft_nullptr || receive_key == ft_nullptr
        || send_initialization_vector == ft_nullptr
        || receive_initialization_vector == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_send_backend.initialize(send_key, 32U) != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INITIALIZATION_FAILED);
    }
    if (this->_receive_backend.initialize(receive_key, 32U) != FT_ERR_SUCCESS)
    {
        (void)this->_send_backend.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INITIALIZATION_FAILED);
    }
    ft_memcpy(this->_send_initialization_vector, send_initialization_vector,
        sizeof(this->_send_initialization_vector));
    ft_memcpy(this->_receive_initialization_vector, receive_initialization_vector,
        sizeof(this->_receive_initialization_vector));
    ft_memset(this->_previous_receive_initialization_vector, 0U,
        sizeof(this->_previous_receive_initialization_vector));
    ft_memcpy(this->_send_key, send_key, sizeof(this->_send_key));
    ft_memcpy(this->_receive_key, receive_key, sizeof(this->_receive_key));
    ft_memset(this->_previous_receive_key, 0U,
        sizeof(this->_previous_receive_key));
    this->_send_key_epoch = 0U;
    this->_receive_key_epoch = 0U;
    this->_previous_receive_key_epoch = 0U;
    this->_highest_sent_packet = 0U;
    this->_highest_received_packet = 0U;
    this->_has_sent_packet = FT_FALSE;
    this->_received_window = 0U;
    this->_has_received_packet = FT_FALSE;
    this->_previous_highest_received_packet = 0U;
    this->_previous_received_window = 0U;
    this->_has_previous_received_packet = FT_FALSE;
    this->_has_previous_receive_key = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_secure_channel::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    (void)this->_send_backend.destroy();
    (void)this->_receive_backend.destroy();
    (void)this->_previous_receive_backend.destroy();
    networking_secure_wipe(this->_send_initialization_vector,
        sizeof(this->_send_initialization_vector));
    networking_secure_wipe(this->_receive_initialization_vector,
        sizeof(this->_receive_initialization_vector));
    networking_secure_wipe(this->_previous_receive_initialization_vector,
        sizeof(this->_previous_receive_initialization_vector));
    networking_secure_wipe(this->_send_key, sizeof(this->_send_key));
    networking_secure_wipe(this->_receive_key, sizeof(this->_receive_key));
    networking_secure_wipe(this->_previous_receive_key,
        sizeof(this->_previous_receive_key));
    this->_send_key_epoch = 0U;
    this->_receive_key_epoch = 0U;
    this->_previous_receive_key_epoch = 0U;
    this->_highest_sent_packet = 0U;
    this->_highest_received_packet = 0U;
    this->_has_sent_packet = FT_FALSE;
    this->_received_window = 0U;
    this->_has_received_packet = FT_FALSE;
    this->_previous_highest_received_packet = 0U;
    this->_previous_received_window = 0U;
    this->_has_previous_received_packet = FT_FALSE;
    this->_has_previous_receive_key = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_secure_channel::update_key_epoch(uint64_t next_epoch) noexcept
{
    uint8_t next_send_key[32];
    uint8_t next_receive_key[32];
    uint8_t next_send_initialization_vector[12];
    uint8_t next_receive_initialization_vector[12];
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (next_epoch <= this->_send_key_epoch
        || next_epoch <= this->_receive_key_epoch)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_send_backend.derive_key_update(this->_send_key,
        next_epoch, next_send_key, next_send_initialization_vector);
    if (result == FT_ERR_SUCCESS)
        result = this->_receive_backend.derive_key_update(this->_receive_key,
            next_epoch, next_receive_key, next_receive_initialization_vector);
    if (result == FT_ERR_SUCCESS)
        result = this->_send_backend.destroy();
    if (result == FT_ERR_SUCCESS)
        result = this->_receive_backend.destroy();
    if (result == FT_ERR_SUCCESS)
        result = this->_send_backend.initialize(next_send_key, 32U);
    if (result == FT_ERR_SUCCESS)
        result = this->_receive_backend.initialize(next_receive_key, 32U);
    if (result == FT_ERR_SUCCESS)
    {
        ft_memcpy(this->_send_key, next_send_key, sizeof(this->_send_key));
        ft_memcpy(this->_receive_key, next_receive_key,
            sizeof(this->_receive_key));
        ft_memcpy(this->_send_initialization_vector,
            next_send_initialization_vector,
            sizeof(this->_send_initialization_vector));
        ft_memcpy(this->_receive_initialization_vector,
            next_receive_initialization_vector,
            sizeof(this->_receive_initialization_vector));
        this->_send_key_epoch = next_epoch;
        this->_receive_key_epoch = next_epoch;
        this->_highest_sent_packet = 0U;
        this->_highest_received_packet = 0U;
        this->_has_sent_packet = FT_FALSE;
        this->_received_window = 0U;
        this->_has_received_packet = FT_FALSE;
    }
    networking_secure_wipe(next_send_key, sizeof(next_send_key));
    networking_secure_wipe(next_receive_key, sizeof(next_receive_key));
    networking_secure_wipe(next_send_initialization_vector,
        sizeof(next_send_initialization_vector));
    networking_secure_wipe(next_receive_initialization_vector,
        sizeof(next_receive_initialization_vector));
    return (result);
}

int32_t networking_secure_channel::update_send_key_epoch(
    uint64_t next_epoch) noexcept
{
    uint8_t next_key[32];
    uint8_t next_initialization_vector[12];
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (next_epoch <= this->_send_key_epoch)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_send_backend.derive_key_update(this->_send_key,
        next_epoch, next_key, next_initialization_vector);
    if (result == FT_ERR_SUCCESS)
        result = this->_send_backend.destroy();
    if (result == FT_ERR_SUCCESS)
        result = this->_send_backend.initialize(next_key, sizeof(next_key));
    if (result == FT_ERR_SUCCESS)
    {
        ft_memcpy(this->_send_key, next_key, sizeof(this->_send_key));
        ft_memcpy(this->_send_initialization_vector,
            next_initialization_vector,
            sizeof(this->_send_initialization_vector));
        this->_send_key_epoch = next_epoch;
        this->_highest_sent_packet = 0U;
        this->_has_sent_packet = FT_FALSE;
    }
    networking_secure_wipe(next_key, sizeof(next_key));
    networking_secure_wipe(next_initialization_vector,
        sizeof(next_initialization_vector));
    return (result);
}

int32_t networking_secure_channel::update_receive_key_epoch(
    uint64_t next_epoch) noexcept
{
    uint8_t next_key[32];
    uint8_t next_initialization_vector[12];
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (next_epoch <= this->_receive_key_epoch)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_receive_backend.derive_key_update(this->_receive_key,
        next_epoch, next_key, next_initialization_vector);
    if (result == FT_ERR_SUCCESS)
        result = this->_previous_receive_backend.destroy();
    if (result == FT_ERR_SUCCESS)
        result = this->_previous_receive_backend.initialize(this->_receive_key,
            sizeof(this->_receive_key));
    if (result == FT_ERR_SUCCESS)
    {
        ft_memcpy(this->_previous_receive_key, this->_receive_key,
            sizeof(this->_previous_receive_key));
        ft_memcpy(this->_previous_receive_initialization_vector,
            this->_receive_initialization_vector,
            sizeof(this->_previous_receive_initialization_vector));
        this->_previous_receive_key_epoch = this->_receive_key_epoch;
        this->_previous_highest_received_packet =
            this->_highest_received_packet;
        this->_previous_received_window = this->_received_window;
        this->_has_previous_received_packet = this->_has_received_packet;
        this->_has_previous_receive_key = FT_TRUE;
    }
    if (result == FT_ERR_SUCCESS)
        result = this->_receive_backend.destroy();
    if (result == FT_ERR_SUCCESS)
        result = this->_receive_backend.initialize(next_key, sizeof(next_key));
    if (result == FT_ERR_SUCCESS)
    {
        ft_memcpy(this->_receive_key, next_key, sizeof(this->_receive_key));
        ft_memcpy(this->_receive_initialization_vector,
            next_initialization_vector,
            sizeof(this->_receive_initialization_vector));
        this->_receive_key_epoch = next_epoch;
        this->_highest_received_packet = 0U;
        this->_received_window = 0U;
        this->_has_received_packet = FT_FALSE;
    }
    networking_secure_wipe(next_key, sizeof(next_key));
    networking_secure_wipe(next_initialization_vector,
        sizeof(next_initialization_vector));
    return (result);
}

int32_t networking_secure_channel::clear_previous_receive_key() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_has_previous_receive_key == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)this->_previous_receive_backend.destroy();
    networking_secure_wipe(this->_previous_receive_initialization_vector,
        sizeof(this->_previous_receive_initialization_vector));
    networking_secure_wipe(this->_previous_receive_key,
        sizeof(this->_previous_receive_key));
    this->_previous_receive_key_epoch = 0U;
    this->_previous_highest_received_packet = 0U;
    this->_previous_received_window = 0U;
    this->_has_previous_received_packet = FT_FALSE;
    this->_has_previous_receive_key = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

uint64_t networking_secure_channel::get_key_epoch() const noexcept
{
    if (this->_send_key_epoch != this->_receive_key_epoch)
        return (0U);
    return (this->_send_key_epoch);
}

uint64_t networking_secure_channel::get_send_key_epoch() const noexcept
{
    return (this->_send_key_epoch);
}

uint64_t networking_secure_channel::get_receive_key_epoch() const noexcept
{
    return (this->_receive_key_epoch);
}

int32_t networking_secure_channel::move(networking_secure_channel &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    ft_memcpy(this->_send_initialization_vector,
        other._send_initialization_vector,
        sizeof(this->_send_initialization_vector));
    ft_memcpy(this->_receive_initialization_vector,
        other._receive_initialization_vector,
        sizeof(this->_receive_initialization_vector));
    ft_memcpy(this->_previous_receive_initialization_vector,
        other._previous_receive_initialization_vector,
        sizeof(this->_previous_receive_initialization_vector));
    ft_memcpy(this->_send_key, other._send_key, sizeof(this->_send_key));
    ft_memcpy(this->_receive_key, other._receive_key,
        sizeof(this->_receive_key));
    ft_memcpy(this->_previous_receive_key, other._previous_receive_key,
        sizeof(this->_previous_receive_key));
    this->_send_key_epoch = other._send_key_epoch;
    this->_receive_key_epoch = other._receive_key_epoch;
    this->_previous_receive_key_epoch = other._previous_receive_key_epoch;
    if (this->_send_backend.move(other._send_backend) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (this->_receive_backend.move(other._receive_backend) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (other._has_previous_receive_key != FT_FALSE
        && this->_previous_receive_backend.move(other._previous_receive_backend)
            != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    this->_highest_sent_packet = other._highest_sent_packet;
    this->_highest_received_packet = other._highest_received_packet;
    this->_has_sent_packet = other._has_sent_packet;
    this->_received_window = other._received_window;
    this->_has_received_packet = other._has_received_packet;
    this->_previous_highest_received_packet =
        other._previous_highest_received_packet;
    this->_previous_received_window = other._previous_received_window;
    this->_has_previous_received_packet = other._has_previous_received_packet;
    this->_has_previous_receive_key = other._has_previous_receive_key;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool networking_secure_channel::prepare_nonce(
    const uint8_t initialization_vector[12], uint64_t packet_number,
    uint8_t nonce[12]) const noexcept
{
    uint32_t index;
    uint32_t shift;

    if (initialization_vector == ft_nullptr || nonce == ft_nullptr)
        return (FT_FALSE);
    ft_memcpy(nonce, initialization_vector, 12U);
    index = 4U;
    shift = 56U;
    while (index < 12U)
    {
        nonce[index] = static_cast<uint8_t>(nonce[index] ^ ((packet_number >> shift) & 0xffU));
        index += 1U;
        if (shift == 0U)
            break ;
        shift -= 8U;
    }
    return (FT_TRUE);
}

ft_bool networking_secure_channel::accepts_packet_number(uint64_t packet_number) const noexcept
{
    uint64_t difference;

    if (this->_has_received_packet == FT_FALSE)
        return (FT_TRUE);
    if (packet_number > this->_highest_received_packet)
        return (FT_TRUE);
    difference = this->_highest_received_packet - packet_number;
    if (difference >= 64U)
        return (FT_FALSE);
    if ((this->_received_window & (1ULL << difference)) == 0U)
        return (FT_TRUE);
    return (FT_FALSE);
}

void networking_secure_channel::record_packet_number(uint64_t packet_number) noexcept
{
    uint64_t difference;

    if (this->_has_received_packet == FT_FALSE)
    {
        this->_has_received_packet = FT_TRUE;
        this->_highest_received_packet = packet_number;
        this->_received_window = 1U;
        return ;
    }
    if (packet_number > this->_highest_received_packet)
    {
        difference = packet_number - this->_highest_received_packet;
        if (difference >= 64U)
            this->_received_window = 1U;
        else
            this->_received_window = (this->_received_window << difference) | 1U;
        this->_highest_received_packet = packet_number;
        return ;
    }
    difference = this->_highest_received_packet - packet_number;
    if (difference < 64U)
        this->_received_window |= 1ULL << difference;
    return ;
}

ft_bool networking_secure_channel::accepts_previous_packet_number(
    uint64_t packet_number) const noexcept
{
    uint64_t difference;

    if (this->_has_previous_received_packet == FT_FALSE)
        return (FT_TRUE);
    if (packet_number > this->_previous_highest_received_packet)
        return (FT_TRUE);
    difference = this->_previous_highest_received_packet - packet_number;
    if (difference >= 64U)
        return (FT_FALSE);
    if ((this->_previous_received_window & (1ULL << difference)) == 0U)
        return (FT_TRUE);
    return (FT_FALSE);
}

void networking_secure_channel::record_previous_packet_number(
    uint64_t packet_number) noexcept
{
    uint64_t difference;

    if (this->_has_previous_received_packet == FT_FALSE)
    {
        this->_has_previous_received_packet = FT_TRUE;
        this->_previous_highest_received_packet = packet_number;
        this->_previous_received_window = 1U;
        return ;
    }
    if (packet_number > this->_previous_highest_received_packet)
    {
        difference = packet_number - this->_previous_highest_received_packet;
        if (difference >= 64U)
            this->_previous_received_window = 1U;
        else
            this->_previous_received_window =
                (this->_previous_received_window << difference) | 1U;
        this->_previous_highest_received_packet = packet_number;
        return ;
    }
    difference = this->_previous_highest_received_packet - packet_number;
    if (difference < 64U)
        this->_previous_received_window |= 1ULL << difference;
    return ;
}

ft_bool networking_secure_channel::seal(uint64_t packet_number,
    const uint8_t *associated_data, ft_size_t associated_data_length,
    const uint8_t *plaintext, ft_size_t plaintext_length,
    ft_vector<uint8_t> &ciphertext, uint8_t authentication_tag[16]) noexcept
{
    uint8_t nonce[12];

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || authentication_tag == ft_nullptr
        || (plaintext == ft_nullptr && plaintext_length != 0U)
        || (associated_data == ft_nullptr && associated_data_length != 0U))
        return (FT_FALSE);
    if (this->_has_sent_packet != FT_FALSE
        && packet_number <= this->_highest_sent_packet)
        return (FT_FALSE);
    if (this->prepare_nonce(this->_send_initialization_vector, packet_number,
        nonce) == FT_FALSE)
        return (FT_FALSE);
    if (ciphertext.is_initialised() != FT_CLASS_STATE_INITIALISED
        && ciphertext.initialize() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    ciphertext.resize(plaintext_length);
    if (ciphertext.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (this->_send_backend.seal(nonce, associated_data, associated_data_length,
        plaintext, plaintext_length, ciphertext, authentication_tag) == FT_FALSE)
        return (FT_FALSE);
    this->_highest_sent_packet = packet_number;
    this->_has_sent_packet = FT_TRUE;
    return (FT_TRUE);
}

ft_bool networking_secure_channel::open(uint64_t packet_number,
    const uint8_t *associated_data, ft_size_t associated_data_length,
    const uint8_t *ciphertext, ft_size_t ciphertext_length,
    const uint8_t authentication_tag[16], ft_vector<uint8_t> &plaintext) noexcept
{
    return (this->open_at_epoch(packet_number, this->_receive_key_epoch,
        associated_data, associated_data_length, ciphertext, ciphertext_length,
        authentication_tag, plaintext));
}

ft_bool networking_secure_channel::open_at_epoch(uint64_t packet_number,
    uint64_t key_epoch, const uint8_t *associated_data,
    ft_size_t associated_data_length, const uint8_t *ciphertext,
    ft_size_t ciphertext_length, const uint8_t authentication_tag[16],
    ft_vector<uint8_t> &plaintext) noexcept
{
    uint8_t nonce[12];
    const uint8_t *initialization_vector;
    networking_crypto_backend *backend;
    ft_bool use_previous;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || authentication_tag == ft_nullptr
        || (ciphertext == ft_nullptr && ciphertext_length != 0U)
        || (associated_data == ft_nullptr && associated_data_length != 0U)
        || this->has_receive_epoch(key_epoch) == FT_FALSE)
        return (FT_FALSE);
    use_previous = FT_FALSE;
    if (key_epoch == this->_receive_key_epoch)
    {
        backend = &this->_receive_backend;
        initialization_vector = this->_receive_initialization_vector;
        if (this->accepts_packet_number(packet_number) == FT_FALSE)
            return (FT_FALSE);
    }
    else
    {
        backend = &this->_previous_receive_backend;
        initialization_vector = this->_previous_receive_initialization_vector;
        use_previous = FT_TRUE;
        if (this->accepts_previous_packet_number(packet_number) == FT_FALSE)
            return (FT_FALSE);
    }
    if (this->prepare_nonce(initialization_vector, packet_number, nonce)
        == FT_FALSE)
        return (FT_FALSE);
    if (plaintext.is_initialised() != FT_CLASS_STATE_INITIALISED
        && plaintext.initialize() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    plaintext.resize(ciphertext_length);
    if (plaintext.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (backend->open(nonce, associated_data, associated_data_length,
        ciphertext, ciphertext_length, authentication_tag, plaintext) == FT_FALSE)
        return (FT_FALSE);
    if (use_previous == FT_FALSE)
        this->record_packet_number(packet_number);
    else
        this->record_previous_packet_number(packet_number);
    return (FT_TRUE);
}

ft_bool networking_secure_channel::has_receive_epoch(
    uint64_t key_epoch) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_FALSE);
    if (key_epoch == this->_receive_key_epoch)
        return (FT_TRUE);
    if (this->_has_previous_receive_key != FT_FALSE
        && key_epoch == this->_previous_receive_key_epoch)
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool networking_secure_channel::is_replay(uint64_t packet_number) const noexcept
{
    if (this->accepts_packet_number(packet_number) == FT_FALSE)
        return (FT_TRUE);
    return (FT_FALSE);
}
