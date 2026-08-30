#include "networking_handshake.hpp"
#include "../Errno/errno.hpp"
#ifdef LIBFT_TEST_BUILD
# include "../../Test/Test/networking_test_hooks.hpp"
#else
# define NETWORKING_TEST_SHOULD_FAIL(point) FT_FALSE
#endif

namespace
{
    static const uint8_t HANDSHAKE_MAGIC[4] = {0x4cU, 0x46U, 0x54U, 0x48U};
    static const uint8_t HANDSHAKE_VERSION = 1U;
    static const ft_size_t HELLO_SIZE = 4U + 1U + 1U + 2U + 8U + 32U + 32U;
    static const ft_size_t COOKIE_TIMESTAMP_SIZE = 8U;

    static void wipe(void *data, ft_size_t size) noexcept
    {
        networking_crypto_backend backend;

        (void)backend.wipe(data, size);
        return ;
    }

    static void write_u64(uint8_t output[8], uint64_t value) noexcept
    {
        uint32_t index;
        uint32_t shift;

        index = 0U;
        shift = 56U;
        while (index < 8U)
        {
            output[index] = static_cast<uint8_t>((value >> shift) & 0xffU);
            index += 1U;
            if (shift == 0U)
                break ;
            shift -= 8U;
        }
        return ;
    }

    static uint64_t read_u64(const uint8_t input[8]) noexcept
    {
        uint32_t index;
        uint64_t value;

        value = 0U;
        index = 0U;
        while (index < 8U)
        {
            value = (value << 8U) | input[index];
            index += 1U;
        }
        return (value);
    }

    static int32_t cookie_mac_input(const networking_message_endpoint &source,
        const uint8_t hello_digest[32], uint64_t issued_at,
        uint8_t input[sizeof(sockaddr_storage) + 1U + 32U + 8U],
        ft_size_t &input_length) noexcept
    {
        if (source.length == 0U
            || static_cast<ft_size_t>(source.length) > sizeof(sockaddr_storage)
            || hello_digest == ft_nullptr)
            return (FT_ERR_INVALID_ARGUMENT);
        input[0] = static_cast<uint8_t>(source.length);
        ft_memcpy(input + 1U, &source.address, source.length);
        ft_memcpy(input + 1U + source.length, hello_digest, 32U);
        write_u64(input + 1U + source.length + 32U, issued_at);
        input_length = 1U + source.length + 32U + 8U;
        return (FT_ERR_SUCCESS);
    }
}

networking_handshake::networking_handshake() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _role(networking_handshake_role::CLIENT),
      _state(networking_handshake_state::IDLE), _local_connection_id(0U),
      _peer_connection_id(0U), _local_private_key(), _local_public_key(),
      _peer_public_key(), _local_nonce(), _peer_nonce(), _send_key(),
      _receive_key(), _send_initialization_vector(),
      _receive_initialization_vector(), _retry_cookie(), _has_retry_cookie(FT_FALSE),
      _crypto_backend(), _local_hello(),
      _peer_hello(), _transcript(), _keys_derived(FT_FALSE),
      _finished_verified(FT_FALSE)
{
    return ;
}

networking_handshake::~networking_handshake() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_handshake::initialize(networking_handshake_role role,
    uint64_t local_connection_id) noexcept
{
    int32_t result;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (role != networking_handshake_role::CLIENT
        && role != networking_handshake_role::SERVER)
        return (FT_ERR_INVALID_ARGUMENT);
    if (local_connection_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_local_hello.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_peer_hello.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_transcript.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_local_hello.destroy();
        (void)this->_peer_hello.destroy();
        (void)this->_transcript.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (result);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->_role = role;
    this->_local_connection_id = local_connection_id;
    this->_peer_connection_id = 0U;
    this->_keys_derived = FT_FALSE;
    this->_finished_verified = FT_FALSE;
    ft_memset(this->_retry_cookie, 0, sizeof(this->_retry_cookie));
    this->_has_retry_cookie = FT_FALSE;
    result = this->_crypto_backend.random_bytes(this->_local_private_key,
        sizeof(this->_local_private_key));
    if (result == FT_ERR_SUCCESS)
        result = this->_crypto_backend.random_bytes(this->_local_nonce,
            sizeof(this->_local_nonce));
    if (result == FT_ERR_SUCCESS)
        result = this->_crypto_backend.public_key(this->_local_private_key,
            this->_local_public_key);
    if (result == FT_ERR_SUCCESS)
        result = this->build_local_hello();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (result);
    }
    this->_state = networking_handshake_state::HELLO_READY;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    wipe(this->_local_private_key, sizeof(this->_local_private_key));
    wipe(this->_local_public_key, sizeof(this->_local_public_key));
    wipe(this->_peer_public_key, sizeof(this->_peer_public_key));
    wipe(this->_local_nonce, sizeof(this->_local_nonce));
    wipe(this->_peer_nonce, sizeof(this->_peer_nonce));
    wipe(this->_send_key, sizeof(this->_send_key));
    wipe(this->_receive_key, sizeof(this->_receive_key));
    wipe(this->_send_initialization_vector, sizeof(this->_send_initialization_vector));
    wipe(this->_receive_initialization_vector, sizeof(this->_receive_initialization_vector));
    wipe(this->_retry_cookie, sizeof(this->_retry_cookie));
    (void)this->_local_hello.destroy();
    (void)this->_peer_hello.destroy();
    (void)this->_transcript.destroy();
    this->_local_connection_id = 0U;
    this->_peer_connection_id = 0U;
    this->_keys_derived = FT_FALSE;
    this->_finished_verified = FT_FALSE;
    this->_has_retry_cookie = FT_FALSE;
    this->_state = networking_handshake_state::IDLE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::move(networking_handshake &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_role = other._role;
    this->_state = other._state;
    this->_local_connection_id = other._local_connection_id;
    this->_peer_connection_id = other._peer_connection_id;
    ft_memcpy(this->_local_private_key, other._local_private_key, 32U);
    ft_memcpy(this->_local_public_key, other._local_public_key, 32U);
    ft_memcpy(this->_peer_public_key, other._peer_public_key, 32U);
    ft_memcpy(this->_local_nonce, other._local_nonce, 32U);
    ft_memcpy(this->_peer_nonce, other._peer_nonce, 32U);
    ft_memcpy(this->_send_key, other._send_key, 32U);
    ft_memcpy(this->_receive_key, other._receive_key, 32U);
    ft_memcpy(this->_send_initialization_vector, other._send_initialization_vector, 12U);
    ft_memcpy(this->_receive_initialization_vector, other._receive_initialization_vector, 12U);
    ft_memcpy(this->_retry_cookie, other._retry_cookie, sizeof(this->_retry_cookie));
    this->_has_retry_cookie = other._has_retry_cookie;
    if (this->_local_hello.initialize(other._local_hello) != FT_ERR_SUCCESS
        || this->_peer_hello.initialize(other._peer_hello) != FT_ERR_SUCCESS
        || this->_transcript.initialize(other._transcript) != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (FT_ERR_NO_MEMORY);
    }
    this->_keys_derived = other._keys_derived;
    this->_finished_verified = other._finished_verified;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::append_u64(ft_vector<uint8_t> &buffer,
    uint64_t value) noexcept
{
    uint8_t encoded[8];
    uint32_t index;

    write_u64(encoded, value);
    index = 0U;
    while (index < sizeof(encoded))
    {
        buffer.push_back(encoded[index]);
        if (buffer.get_error() != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::build_local_hello() noexcept
{
    uint32_t index;

    this->_local_hello.clear();
    index = 0U;
    while (index < sizeof(HANDSHAKE_MAGIC))
    {
        this->_local_hello.push_back(HANDSHAKE_MAGIC[index]);
        index += 1U;
    }
    this->_local_hello.push_back(HANDSHAKE_VERSION);
    this->_local_hello.push_back(static_cast<uint8_t>(this->_role));
    this->_local_hello.push_back(0U);
    this->_local_hello.push_back(0U);
    if (this->_local_hello.get_error() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (this->append_u64(this->_local_hello, this->_local_connection_id)
        != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    index = 0U;
    while (index < sizeof(this->_local_nonce))
    {
        this->_local_hello.push_back(this->_local_nonce[index]);
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(this->_local_public_key))
    {
        this->_local_hello.push_back(this->_local_public_key[index]);
        index += 1U;
    }
    if (this->_local_hello.get_error() != FT_ERR_SUCCESS
        || this->_local_hello.size() != HELLO_SIZE)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::get_local_hello(ft_vector<uint8_t> &hello) const noexcept
{
    ft_size_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (hello.is_initialised() == FT_CLASS_STATE_INITIALISED)
        (void)hello.destroy();
    if (hello.initialize(this->_local_hello) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (this->_has_retry_cookie == FT_FALSE)
        return (FT_ERR_SUCCESS);
    index = 0U;
    while (index < sizeof(this->_retry_cookie))
    {
        if (hello.push_back(this->_retry_cookie[index]) != FT_ERR_SUCCESS)
        {
            (void)hello.destroy();
            return (FT_ERR_NO_MEMORY);
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::accept_peer_hello(const uint8_t *hello,
    ft_size_t hello_length) noexcept
{
    uint64_t peer_connection_id;
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (hello == ft_nullptr || (hello_length != HELLO_SIZE
        && hello_length != HELLO_SIZE + sizeof(this->_retry_cookie)))
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_state == networking_handshake_state::FAILED)
        return (FT_ERR_INVALID_STATE);
    if (this->_state == networking_handshake_state::KEYS_DERIVED
        || this->_state == networking_handshake_state::FINISHED)
    {
        if (this->_peer_hello.size() != HELLO_SIZE
            || ft_memcmp(hello, &this->_peer_hello[0], HELLO_SIZE) != 0)
            return (FT_ERR_PERMISSION_DENIED);
        return (FT_ERR_SUCCESS);
    }
    if (ft_memcmp(hello, HANDSHAKE_MAGIC, sizeof(HANDSHAKE_MAGIC)) != 0
        || hello[4] != HANDSHAKE_VERSION
        || hello[5] == static_cast<uint8_t>(this->_role)
        || hello[6] != 0U || hello[7] != 0U)
        return (FT_ERR_PERMISSION_DENIED);
    peer_connection_id = read_u64(hello + 8U);
    if (peer_connection_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_memcpy(this->_peer_nonce, hello + 16U, 32U);
    ft_memcpy(this->_peer_public_key, hello + 48U, 32U);
    this->_peer_connection_id = peer_connection_id;
    this->_peer_hello.clear();
    index = 0U;
    while (index < HELLO_SIZE)
    {
        this->_peer_hello.push_back(hello[index]);
        if (this->_peer_hello.get_error() != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        index += 1U;
    }
    this->_state = networking_handshake_state::PEER_HELLO_ACCEPTED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::build_transcript() noexcept
{
    const ft_vector<uint8_t> *first;
    const ft_vector<uint8_t> *second;
    ft_size_t index;

    if (this->_role == networking_handshake_role::CLIENT)
    {
        first = &this->_local_hello;
        second = &this->_peer_hello;
    }
    else
    {
        first = &this->_peer_hello;
        second = &this->_local_hello;
    }
    this->_transcript.clear();
    index = 0U;
    while (index < first->size())
    {
        this->_transcript.push_back((*first)[index]);
        index += 1U;
    }
    index = 0U;
    while (index < second->size())
    {
        this->_transcript.push_back((*second)[index]);
        index += 1U;
    }
    if (this->_transcript.get_error() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::derive_keys() noexcept
{
    int32_t result;
    networking_crypto_role session_role;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_state != networking_handshake_state::PEER_HELLO_ACCEPTED)
        return (FT_ERR_INVALID_STATE);
    session_role = networking_crypto_role::CLIENT;
    if (this->_role == networking_handshake_role::SERVER)
        session_role = networking_crypto_role::SERVER;
    result = this->build_transcript();
    if (result == FT_ERR_SUCCESS)
        result = this->_crypto_backend.derive_session_keys(this->_local_private_key,
            this->_peer_public_key, &this->_transcript[0],
            this->_transcript.size(), session_role,
            this->_send_key, this->_receive_key,
            this->_send_initialization_vector,
            this->_receive_initialization_vector);
    if (result != FT_ERR_SUCCESS)
    {
        this->_state = networking_handshake_state::FAILED;
        wipe(this->_send_key, sizeof(this->_send_key));
        wipe(this->_receive_key, sizeof(this->_receive_key));
        return (result);
    }
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_HANDSHAKE_STATE)
        != FT_FALSE)
    {
        this->_state = networking_handshake_state::FAILED;
        wipe(this->_send_key, sizeof(this->_send_key));
        wipe(this->_receive_key, sizeof(this->_receive_key));
        return (FT_ERR_NO_MEMORY);
    }
    this->_keys_derived = FT_TRUE;
    this->_state = networking_handshake_state::KEYS_DERIVED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::create_finished(uint8_t finished[32]) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_keys_derived == FT_FALSE || finished == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    return (this->_crypto_backend.hmac_sha256(this->_send_key, 32U,
        &this->_transcript[0], this->_transcript.size(), finished));
}

int32_t networking_handshake::verify_finished(const uint8_t finished[32]) noexcept
{
    uint8_t expected[32];
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_keys_derived == FT_FALSE || finished == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    result = this->_crypto_backend.hmac_sha256(this->_receive_key, 32U,
        &this->_transcript[0], this->_transcript.size(), expected);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (ft_constant_time_equal(expected, finished, sizeof(expected)) == FT_FALSE)
    {
        wipe(expected, sizeof(expected));
        this->_state = networking_handshake_state::FAILED;
        return (FT_ERR_PERMISSION_DENIED);
    }
    wipe(expected, sizeof(expected));
    this->_finished_verified = FT_TRUE;
    this->_state = networking_handshake_state::FINISHED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::get_traffic_keys(uint8_t send_key[32],
    uint8_t receive_key[32], uint8_t send_initialization_vector[12],
    uint8_t receive_initialization_vector[12]) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_keys_derived == FT_FALSE || send_key == ft_nullptr
        || receive_key == ft_nullptr || send_initialization_vector == ft_nullptr
        || receive_initialization_vector == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    ft_memcpy(send_key, this->_send_key, 32U);
    ft_memcpy(receive_key, this->_receive_key, 32U);
    ft_memcpy(send_initialization_vector, this->_send_initialization_vector, 12U);
    ft_memcpy(receive_initialization_vector, this->_receive_initialization_vector, 12U);
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::set_retry_cookie(const uint8_t cookie[40]) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_role != networking_handshake_role::CLIENT
        || this->_state != networking_handshake_state::HELLO_READY
        || cookie == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    ft_memcpy(this->_retry_cookie, cookie, sizeof(this->_retry_cookie));
    this->_has_retry_cookie = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

networking_handshake_state networking_handshake::get_state() const noexcept
{
    return (this->_state);
}

networking_handshake_role networking_handshake::get_role() const noexcept
{
    return (this->_role);
}

uint64_t networking_handshake::get_peer_connection_id() const noexcept
{
    return (this->_peer_connection_id);
}

int32_t networking_handshake::get_peer_public_key(
    uint8_t public_key[32]) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || public_key == ft_nullptr || this->_peer_connection_id == 0U)
        return (FT_ERR_NOT_FOUND);
    ft_memcpy(public_key, this->_peer_public_key, 32U);
    return (FT_ERR_SUCCESS);
}

int32_t networking_handshake::hash_hello(const uint8_t *hello,
    ft_size_t hello_length, uint8_t digest[32]) noexcept
{
    networking_crypto_backend backend;

    if (hello == ft_nullptr || digest == ft_nullptr || hello_length == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    return (backend.sha256(hello, hello_length, digest));
}

int32_t networking_handshake::create_retry_cookie(const uint8_t secret[32],
    const networking_message_endpoint &source, const uint8_t hello_digest[32],
    uint64_t issued_at, uint8_t cookie[40]) noexcept
{
    uint8_t input[sizeof(sockaddr_storage) + 1U + 32U + 8U];
    ft_size_t input_length;
    int32_t result;

    if (secret == ft_nullptr || cookie == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = cookie_mac_input(source, hello_digest, issued_at, input, input_length);
    if (result != FT_ERR_SUCCESS)
        return (result);
    write_u64(cookie, issued_at);
    networking_crypto_backend backend;

    result = backend.hmac_sha256(secret, 32U, input, input_length,
        cookie + COOKIE_TIMESTAMP_SIZE);
    wipe(input, sizeof(input));
    return (result);
}

int32_t networking_handshake::verify_retry_cookie(const uint8_t secret[32],
    const networking_message_endpoint &source, const uint8_t hello_digest[32],
    uint64_t now, uint64_t lifetime_milliseconds, const uint8_t cookie[40]) noexcept
{
    uint8_t input[sizeof(sockaddr_storage) + 1U + 32U + 8U];
    uint8_t expected[32];
    ft_size_t input_length;
    uint64_t issued_at;
    int32_t result;

    if (secret == ft_nullptr || cookie == ft_nullptr || lifetime_milliseconds == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    issued_at = read_u64(cookie);
    if (issued_at > now || now - issued_at > lifetime_milliseconds)
        return (FT_ERR_PERMISSION_DENIED);
    result = cookie_mac_input(source, hello_digest, issued_at, input, input_length);
    if (result == FT_ERR_SUCCESS)
    {
        networking_crypto_backend backend;

        result = backend.hmac_sha256(secret, 32U, input, input_length, expected);
    }
    wipe(input, sizeof(input));
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (ft_constant_time_equal(expected, cookie + COOKIE_TIMESTAMP_SIZE,
        sizeof(expected)) == FT_FALSE)
    {
        wipe(expected, sizeof(expected));
        return (FT_ERR_PERMISSION_DENIED);
    }
    wipe(expected, sizeof(expected));
    return (FT_ERR_SUCCESS);
}
