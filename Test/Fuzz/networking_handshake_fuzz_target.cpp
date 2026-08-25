#include "../../Modules/Networking/networking_handshake.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <cstddef>
#include <cstdint>

static void networking_handshake_fuzz_endpoint(
    networking_message_endpoint &endpoint) noexcept
{
    ft_memset(&endpoint, 0, sizeof(endpoint));
    endpoint.address.ss_family = AF_INET;
    endpoint.length = sizeof(struct sockaddr_in);
    return ;
}

static void networking_handshake_fuzz_run(const uint8_t *data,
    ft_size_t size) noexcept
{
    networking_handshake client;
    networking_handshake server;
    ft_vector<uint8_t> client_hello;
    ft_vector<uint8_t> server_hello;
    networking_message_endpoint endpoint;
    uint8_t client_finished[32];
    uint8_t server_finished[32];
    uint8_t secret[32];
    uint8_t digest[32];
    uint8_t cookie[40];
    ft_size_t index;
    uint64_t issued_at;
    int32_t result;
    int32_t cleanup_result;

    index = 0U;
    while (index < sizeof(secret))
    {
        secret[index] = static_cast<uint8_t>(index * 7U + 3U);
        index += 1U;
    }
    if (size > 0U)
        secret[0U] ^= data[0U];
    networking_handshake_fuzz_endpoint(endpoint);
    if (client.initialize(networking_handshake_role::CLIENT, 1U)
        != FT_ERR_SUCCESS
        || server.initialize(networking_handshake_role::SERVER, 2U)
        != FT_ERR_SUCCESS)
        return ;
    if (client.get_local_hello(client_hello) != FT_ERR_SUCCESS
        || server.get_local_hello(server_hello) != FT_ERR_SUCCESS)
        return ;
    if (size > 1U && server_hello.size() > 0U)
        server_hello[data[1U] % server_hello.size()] ^= data[1U];
    result = client.accept_peer_hello(&server_hello[0], server_hello.size());
    if (result == FT_ERR_SUCCESS)
    {
        result = server.accept_peer_hello(&client_hello[0], client_hello.size());
        if (result == FT_ERR_SUCCESS)
        {
            result = client.derive_keys();
            if (result == FT_ERR_SUCCESS)
                result = server.derive_keys();
            if (result == FT_ERR_SUCCESS)
                result = client.create_finished(client_finished);
            if (result == FT_ERR_SUCCESS)
                result = server.create_finished(server_finished);
            if (result == FT_ERR_SUCCESS)
                result = client.verify_finished(server_finished);
            if (result == FT_ERR_SUCCESS)
                result = server.verify_finished(client_finished);
        }
    }
    result = networking_handshake::hash_hello(&client_hello[0],
        client_hello.size(), digest);
    issued_at = 100U;
    if (size > 2U)
        issued_at += static_cast<uint64_t>(data[2U]);
    if (result == FT_ERR_SUCCESS)
    {
        result = networking_handshake::create_retry_cookie(secret, endpoint,
            digest, issued_at, cookie);
        if (result == FT_ERR_SUCCESS)
            result = networking_handshake::verify_retry_cookie(secret, endpoint,
                digest, issued_at, 1000U, cookie);
        if (size > 3U)
            cookie[data[3U] % sizeof(cookie)] ^= data[3U];
        if (result == FT_ERR_SUCCESS)
            result = networking_handshake::verify_retry_cookie(secret, endpoint,
                digest, issued_at + 2000U, 1000U, cookie);
    }
    cleanup_result = client.destroy();
    if (result == FT_ERR_SUCCESS && cleanup_result != FT_ERR_SUCCESS)
        result = cleanup_result;
    cleanup_result = server.destroy();
    if (result == FT_ERR_SUCCESS && cleanup_result != FT_ERR_SUCCESS)
        result = cleanup_result;
    cleanup_result = client_hello.destroy();
    if (result == FT_ERR_SUCCESS && cleanup_result != FT_ERR_SUCCESS)
        result = cleanup_result;
    cleanup_result = server_hello.destroy();
    if (result == FT_ERR_SUCCESS && cleanup_result != FT_ERR_SUCCESS)
        result = cleanup_result;
    return ;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ft_size_t bounded_size;

    if (data == ft_nullptr && size != 0U)
        return (0);
    bounded_size = static_cast<ft_size_t>(size);
    if (bounded_size > 512U)
        bounded_size = 512U;
    networking_handshake_fuzz_run(data, bounded_size);
    return (0);
}
