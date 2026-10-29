#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Crypto/crypto_session.hpp"
#include "../../Modules/Crypto/crypto_x25519.hpp"
#include "../../Modules/Errno/errno.hpp"

FT_TEST(test_crypto_session_directional_keys_and_key_updates)
{
    uint8_t client_private[32];
    uint8_t server_private[32];
    uint8_t client_public[32];
    uint8_t server_public[32];
    uint8_t client_send_key[32];
    uint8_t client_receive_key[32];
    uint8_t server_send_key[32];
    uint8_t server_receive_key[32];
    uint8_t client_send_iv[12];
    uint8_t client_receive_iv[12];
    uint8_t server_send_iv[12];
    uint8_t server_receive_iv[12];
    uint8_t updated_key[32];
    uint8_t repeated_key[32];
    uint8_t updated_iv[12];
    uint8_t repeated_iv[12];
    uint8_t transcript[4] = {0x54U, 0x45U, 0x53U, 0x54U};

    ft_memset(client_private, 1, sizeof(client_private));
    ft_memset(server_private, 2, sizeof(server_private));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(client_private,
        client_public));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(server_private,
        server_public));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_session_keys(client_private,
        server_public, transcript, sizeof(transcript), crypto_session_role::CLIENT,
        client_send_key, client_receive_key, client_send_iv,
        client_receive_iv));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_session_keys(server_private,
        client_public, transcript, sizeof(transcript), crypto_session_role::SERVER,
        server_send_key, server_receive_key, server_send_iv,
        server_receive_iv));
    FT_ASSERT_EQ(0, ft_memcmp(client_send_key, server_receive_key, 32U));
    FT_ASSERT_EQ(0, ft_memcmp(client_receive_key, server_send_key, 32U));
    FT_ASSERT_EQ(0, ft_memcmp(client_send_iv, server_receive_iv, 12U));
    FT_ASSERT_EQ(0, ft_memcmp(client_receive_iv, server_send_iv, 12U));
    FT_ASSERT(ft_memcmp(client_send_key, client_receive_key, 32U) != 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_key_update(client_send_key,
        7U, updated_key, updated_iv));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_key_update(client_send_key,
        7U, repeated_key, repeated_iv));
    FT_ASSERT_EQ(0, ft_memcmp(updated_key, repeated_key, 32U));
    FT_ASSERT_EQ(0, ft_memcmp(updated_iv, repeated_iv, 12U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_key_update(client_send_key,
        8U, repeated_key, repeated_iv));
    FT_ASSERT(ft_memcmp(updated_key, repeated_key, 32U) != 0);
    return (1);
}

FT_TEST(test_crypto_session_transcript_and_failure_are_isolated)
{
    uint8_t private_key[32];
    uint8_t peer_private_key[32];
    uint8_t peer_public_key[32];
    uint8_t first_transcript[2] = {'a', 'b'};
    uint8_t second_transcript[2] = {'a', 'c'};
    uint8_t first_send[32];
    uint8_t second_send[32];
    uint8_t receive[32];
    uint8_t send_iv[12];
    uint8_t receive_iv[12];
    uint8_t zero_peer[32];

    ft_memset(private_key, 3, sizeof(private_key));
    ft_memset(peer_private_key, 4, sizeof(peer_private_key));
    ft_memset(zero_peer, 0, sizeof(zero_peer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(peer_private_key,
        peer_public_key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_session_keys(private_key,
        peer_public_key, first_transcript, sizeof(first_transcript),
        crypto_session_role::CLIENT, first_send, receive, send_iv,
        receive_iv));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_session_keys(private_key,
        peer_public_key, second_transcript, sizeof(second_transcript),
        crypto_session_role::CLIENT, second_send, receive, send_iv,
        receive_iv));
    FT_ASSERT(ft_memcmp(first_send, second_send, sizeof(first_send)) != 0);
    ft_memset(first_send, 0xff, sizeof(first_send));
    ft_memset(receive, 0xff, sizeof(receive));
    FT_ASSERT(crypto_derive_session_keys(private_key, zero_peer,
        first_transcript, sizeof(first_transcript), crypto_session_role::CLIENT,
        first_send, receive, send_iv, receive_iv) != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(0, ft_memcmp(first_send, zero_peer, 32U));
    FT_ASSERT_EQ(0, ft_memcmp(receive, zero_peer, 32U));
    return (1);
}

FT_TEST(test_crypto_session_binds_length_delimited_fields_peer_and_role)
{
    uint8_t private_key[32];
    uint8_t first_peer_private[32];
    uint8_t second_peer_private[32];
    uint8_t first_peer_public[32];
    uint8_t second_peer_public[32];
    const uint8_t first_transcript[11] = {
        0U, 0U, 0U, 2U, 'a', 'b', 0U, 0U, 0U, 1U, 'c'};
    const uint8_t second_transcript[11] = {
        0U, 0U, 0U, 1U, 'a', 0U, 0U, 0U, 2U, 'b', 'c'};
    uint8_t first_send_key[32];
    uint8_t changed_transcript_key[32];
    uint8_t changed_peer_key[32];
    uint8_t changed_role_key[32];
    uint8_t receive_key[32];
    uint8_t send_iv[12];
    uint8_t receive_iv[12];

    ft_memset(private_key, 0x31, sizeof(private_key));
    ft_memset(first_peer_private, 0x42, sizeof(first_peer_private));
    ft_memset(second_peer_private, 0x53, sizeof(second_peer_private));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(
        first_peer_private, first_peer_public));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(
        second_peer_private, second_peer_public));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_session_keys(private_key,
        first_peer_public, first_transcript, sizeof(first_transcript),
        crypto_session_role::CLIENT, first_send_key, receive_key, send_iv,
        receive_iv));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_session_keys(private_key,
        first_peer_public, second_transcript, sizeof(second_transcript),
        crypto_session_role::CLIENT, changed_transcript_key, receive_key,
        send_iv, receive_iv));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_session_keys(private_key,
        second_peer_public, first_transcript, sizeof(first_transcript),
        crypto_session_role::CLIENT, changed_peer_key, receive_key, send_iv,
        receive_iv));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_session_keys(private_key,
        first_peer_public, first_transcript, sizeof(first_transcript),
        crypto_session_role::SERVER, changed_role_key, receive_key, send_iv,
        receive_iv));
    FT_ASSERT(ft_memcmp(first_send_key, changed_transcript_key, 32U) != 0);
    FT_ASSERT(ft_memcmp(first_send_key, changed_peer_key, 32U) != 0);
    FT_ASSERT(ft_memcmp(first_send_key, changed_role_key, 32U) != 0);
    return (1);
}
