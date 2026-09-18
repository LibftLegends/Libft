#include "../test_internal.hpp"
#include "../../Modules/Networking/networking_test_hooks.hpp"
#include "../../Modules/Networking/networking_secure_channel.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t networking_test_initialize_channel(
    networking_secure_channel &channel) noexcept
{
    const uint8_t key[32] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
        9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U,
        17U, 18U, 19U, 20U, 21U, 22U, 23U, 24U,
        25U, 26U, 27U, 28U, 29U, 30U, 31U, 32U};
    const uint8_t initialization_vector[12] = {41U, 42U, 43U, 44U,
        45U, 46U, 47U, 48U, 49U, 50U, 51U, 52U};

    return (channel.initialize(key, sizeof(key), initialization_vector,
        sizeof(initialization_vector)));
}

static int32_t networking_test_old_packet_is_usable(
    networking_secure_channel &sender, networking_secure_channel &receiver)
    noexcept
{
    const uint8_t plaintext[] = {'o', 'l', 'd'};
    uint8_t authentication_tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> decrypted;

    if (sender.seal(17U, ft_nullptr, 0U, plaintext, sizeof(plaintext),
            ciphertext, authentication_tag) == FT_FALSE)
        return (FT_ERR_INTERNAL);
    if (receiver.open(17U, ft_nullptr, 0U, &ciphertext[0],
            ciphertext.size(), authentication_tag, decrypted) == FT_FALSE)
        return (FT_ERR_INTERNAL);
    if (decrypted.size() != sizeof(plaintext))
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_networking_secure_channel_combined_rotation_is_transactional)
{
    const networking_test_failure_point failure_points[9] = {
        NETWORKING_TEST_SECURE_DERIVE_SEND,
        NETWORKING_TEST_SECURE_DERIVE_RECEIVE,
        NETWORKING_TEST_SECURE_INIT_PREVIOUS,
        NETWORKING_TEST_SECURE_INIT_SEND,
        NETWORKING_TEST_SECURE_INIT_RECEIVE,
        NETWORKING_TEST_SECURE_BACKEND_SWAP,
        NETWORKING_TEST_SECURE_SWAP_SEND,
        NETWORKING_TEST_SECURE_SWAP_RECEIVE,
        NETWORKING_TEST_SECURE_SWAP_PREVIOUS};
    networking_secure_channel sender;
    networking_secure_channel receiver;
    uint32_t failure_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    failure_index = 0U;
    while (failure_index < 9U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_initialize_channel(sender));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_initialize_channel(receiver));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
            failure_points[failure_index]));
        FT_ASSERT_EQ(FT_ERR_NO_MEMORY, sender.update_key_epoch(1U));
        FT_ASSERT_EQ(0U, sender.get_key_epoch());
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            networking_test_old_packet_is_usable(sender, receiver));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.update_key_epoch(1U));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.update_key_epoch(1U));
        FT_ASSERT_EQ(1U, sender.get_key_epoch());
        FT_ASSERT_EQ(1U, receiver.get_key_epoch());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
        failure_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    return (1);
}

FT_TEST(test_networking_secure_channel_combined_rotation_promotes_previous_receive_key)
{
    networking_secure_channel sender;
    networking_secure_channel receiver;
    const uint8_t key[32] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
        9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U,
        17U, 18U, 19U, 20U, 21U, 22U, 23U, 24U,
        25U, 26U, 27U, 28U, 29U, 30U, 31U, 32U};
    const uint8_t initialization_vector[12] = {41U, 42U, 43U, 44U,
        45U, 46U, 47U, 48U, 49U, 50U, 51U, 52U};
    const uint8_t plaintext[] = {'o', 'l', 'd', 'e', 'r'};
    uint8_t authentication_tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> decrypted;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(key, sizeof(key),
        initialization_vector, sizeof(initialization_vector)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(key, sizeof(key),
        initialization_vector, sizeof(initialization_vector)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.update_send_key_epoch(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.update_receive_key_epoch(1U));
    FT_ASSERT_EQ(FT_TRUE, receiver.has_receive_epoch(0U));
    FT_ASSERT_EQ(FT_TRUE, receiver.has_receive_epoch(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.update_key_epoch(2U));
    FT_ASSERT_EQ(FT_FALSE, receiver.has_receive_epoch(0U));
    FT_ASSERT_EQ(FT_TRUE, receiver.has_receive_epoch(1U));
    FT_ASSERT_EQ(FT_TRUE, receiver.has_receive_epoch(2U));
    FT_ASSERT(sender.seal(18U, ft_nullptr, 0U, plaintext, sizeof(plaintext),
        ciphertext, authentication_tag));
    FT_ASSERT(receiver.open_at_epoch(18U, 1U, ft_nullptr, 0U,
        &ciphertext[0], ciphertext.size(), authentication_tag, decrypted));
    FT_ASSERT_EQ(sizeof(plaintext), decrypted.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

FT_TEST(test_networking_secure_channel_receive_rotation_is_transactional)
{
    networking_secure_channel sender;
    networking_secure_channel receiver;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_initialize_channel(sender));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_initialize_channel(receiver));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_SECURE_INIT_PREVIOUS));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, receiver.update_receive_key_epoch(1U));
    FT_ASSERT_EQ(0U, receiver.get_receive_key_epoch());
    FT_ASSERT_EQ(FT_FALSE, receiver.has_receive_epoch(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_test_old_packet_is_usable(sender, receiver));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.update_send_key_epoch(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.update_receive_key_epoch(1U));
    FT_ASSERT_EQ(1U, receiver.get_receive_key_epoch());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    return (1);
}

FT_TEST(test_networking_secure_channel_move_failure_preserves_source)
{
    networking_secure_channel source;
    networking_secure_channel destination;
    const networking_test_failure_point failure_points[7] = {
        NETWORKING_TEST_SECURE_BACKEND_SWAP,
        NETWORKING_TEST_SECURE_MOVE_INIT_SEND,
        NETWORKING_TEST_SECURE_MOVE_INIT_RECEIVE,
        NETWORKING_TEST_SECURE_MOVE_INIT_PREVIOUS,
        NETWORKING_TEST_SECURE_MOVE_SWAP_SEND,
        NETWORKING_TEST_SECURE_MOVE_SWAP_RECEIVE,
        NETWORKING_TEST_SECURE_MOVE_SWAP_PREVIOUS};
    uint32_t failure_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    failure_index = 0U;
    while (failure_index < 7U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            networking_test_initialize_channel(source));
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            networking_test_initialize_channel(destination));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.update_receive_key_epoch(1U));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
            failure_points[failure_index]));
        FT_ASSERT_EQ(FT_ERR_NO_MEMORY, destination.move(source));
        FT_ASSERT_EQ(0U, source.get_send_key_epoch());
        FT_ASSERT_EQ(1U, source.get_receive_key_epoch());
        FT_ASSERT_EQ(0U, destination.get_send_key_epoch());
        FT_ASSERT_EQ(0U, destination.get_receive_key_epoch());
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            networking_test_old_packet_is_usable(source, destination));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
        FT_ASSERT_EQ(0U, destination.get_send_key_epoch());
        FT_ASSERT_EQ(1U, destination.get_receive_key_epoch());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
        failure_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    return (1);
}
