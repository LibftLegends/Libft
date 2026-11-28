#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Networking/networking_secure_channel.hpp"
#include "../../Modules/Errno/errno.hpp"

FT_TEST(test_networking_secure_channel_failure_clears_previous_output)
{
    uint8_t key[32] = {0x31U};
    uint8_t initialization_vector[12] = {0x52U};
    uint8_t authentication_tag[16];
    uint8_t associated_data[4] = {0x10U, 0x20U, 0x30U, 0x40U};
    const uint8_t message[5] = {1U, 3U, 5U, 7U, 9U};
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> plaintext;
    networking_secure_channel sender;
    networking_secure_channel receiver;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(key, sizeof(key),
        initialization_vector, sizeof(initialization_vector)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(key, sizeof(key),
        initialization_vector, sizeof(initialization_vector)));
    FT_ASSERT_EQ(FT_TRUE, sender.seal(1U, associated_data,
        sizeof(associated_data), message, sizeof(message), ciphertext,
        authentication_tag));
    FT_ASSERT_EQ(FT_TRUE, receiver.open(1U, associated_data,
        sizeof(associated_data), &ciphertext[0], ciphertext.size(),
        authentication_tag, plaintext));
    FT_ASSERT_EQ(sizeof(message), plaintext.size());
    associated_data[0] ^= 1U;
    FT_ASSERT_EQ(FT_FALSE, receiver.open(2U, associated_data,
        sizeof(associated_data), &ciphertext[0], ciphertext.size(),
        authentication_tag, plaintext));
    FT_ASSERT_EQ(0U, plaintext.size());
    associated_data[0] ^= 1U;
    ciphertext[0] ^= 1U;
    FT_ASSERT_EQ(FT_FALSE, receiver.open(2U, associated_data,
        sizeof(associated_data), &ciphertext[0], ciphertext.size(),
        authentication_tag, plaintext));
    FT_ASSERT_EQ(0U, plaintext.size());
    ciphertext[0] ^= 1U;
    authentication_tag[0] ^= 1U;
    FT_ASSERT_EQ(FT_FALSE, receiver.open(2U, associated_data,
        sizeof(associated_data), &ciphertext[0], ciphertext.size(),
        authentication_tag, plaintext));
    FT_ASSERT_EQ(0U, plaintext.size());
    authentication_tag[0] ^= 1U;
    FT_ASSERT_EQ(FT_FALSE, sender.seal(1U, associated_data,
        sizeof(associated_data), message, sizeof(message), ciphertext,
        authentication_tag));
    FT_ASSERT_EQ(0U, ciphertext.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, plaintext.destroy());
    return (1);
}

FT_TEST(test_networking_secure_channel_replay_commit_is_transactional)
{
    uint8_t key[32] = {0x39U};
    uint8_t initialization_vector[12] = {0x64U};
    uint8_t authentication_tag[16];
    const uint8_t message[4] = {2U, 4U, 6U, 8U};
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> plaintext;
    networking_secure_channel sender;
    networking_secure_channel receiver;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(key, sizeof(key),
        initialization_vector, sizeof(initialization_vector)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(key, sizeof(key),
        initialization_vector, sizeof(initialization_vector)));
    FT_ASSERT_EQ(FT_TRUE, sender.seal(7U, ft_nullptr, 0U, message,
        sizeof(message), ciphertext, authentication_tag));
    FT_ASSERT_EQ(FT_TRUE, receiver.open_at_epoch(7U, 0U, ft_nullptr, 0U,
        &ciphertext[0], ciphertext.size(), authentication_tag, plaintext,
        FT_FALSE));
    FT_ASSERT_EQ(FT_TRUE, receiver.open_at_epoch(7U, 0U, ft_nullptr, 0U,
        &ciphertext[0], ciphertext.size(), authentication_tag, plaintext,
        FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.commit_received_packet(7U, 0U));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
        receiver.commit_received_packet(7U, 0U));
    FT_ASSERT_EQ(FT_FALSE, receiver.open_at_epoch(7U, 0U, ft_nullptr, 0U,
        &ciphertext[0], ciphertext.size(), authentication_tag, plaintext));
    FT_ASSERT_EQ(0U, plaintext.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, plaintext.destroy());
    return (1);
}

FT_TEST(test_networking_secure_channel_destroy_wipes_secret_state)
{
    uint8_t key[32] = {0x73U};
    uint8_t initialization_vector[12] = {0x24U};
    networking_secure_channel channel;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.initialize(key, sizeof(key),
        initialization_vector, sizeof(initialization_vector)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.update_receive_key_epoch(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.destroy());
    FT_ASSERT_EQ(FT_TRUE, channel.test_secret_state_is_wiped());
    return (1);
}
