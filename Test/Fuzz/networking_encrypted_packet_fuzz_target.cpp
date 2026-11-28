#include "../../Modules/Networking/networking_secure_channel.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "networking_structured_fuzz_mutator.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstdint>

static void networking_encrypted_fuzz_read_u64(const uint8_t *input,
    ft_size_t input_size, uint64_t &value) noexcept
{
    ft_size_t index;

    value = 0U;
    index = 0U;
    while (index < 8U && index < input_size)
    {
        value = (value << 8U) | input[index];
        index += 1U;
    }
    return ;
}

static void networking_encrypted_fuzz_run_valid_then_mutated(
    const uint8_t *data, ft_size_t size) noexcept
{
    const uint8_t key[32] = {
        0x91U, 0x2aU, 0x73U, 0x04U, 0xc5U, 0x16U, 0x67U, 0xb8U,
        0x39U, 0xeaU, 0x5bU, 0x0cU, 0xddU, 0x2eU, 0x7fU, 0x80U,
        0x11U, 0x62U, 0xb3U, 0x24U, 0x95U, 0x46U, 0xf7U, 0x38U,
        0xa9U, 0x5aU, 0x0bU, 0xccU, 0x7dU, 0x2eU, 0x9fU, 0x40U};
    const uint8_t initialization_vector[12] = {
        0x10U, 0x32U, 0x54U, 0x76U, 0x98U, 0xbaU,
        0xdcU, 0xfeU, 0x01U, 0x23U, 0x45U, 0x67U};
    const uint8_t *associated_data;
    ft_size_t associated_data_size;
    ft_size_t plaintext_size;
    const uint8_t *ciphertext_data;
    uint8_t authentication_tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> plaintext;
    networking_secure_channel sender;
    networking_secure_channel receiver;
    ft_bool opened;

    plaintext_size = size;
    if (plaintext_size > 2048U)
        plaintext_size = 2048U;
    associated_data = data;
    associated_data_size = size;
    if (associated_data_size > 32U)
        associated_data_size = 32U;
    if (associated_data_size == 0U)
        associated_data = ft_nullptr;
    if (sender.initialize(key, sizeof(key), initialization_vector,
            sizeof(initialization_vector)) != FT_ERR_SUCCESS)
        return ;
    if (receiver.initialize(key, sizeof(key), initialization_vector,
            sizeof(initialization_vector)) != FT_ERR_SUCCESS)
    {
        (void)sender.destroy();
        return ;
    }
    if (sender.seal(1U, associated_data, associated_data_size, data,
            plaintext_size, ciphertext, authentication_tag) == FT_FALSE)
        goto cleanup;
    ciphertext_data = ft_nullptr;
    if (ciphertext.size() != 0U)
        ciphertext_data = &ciphertext[0];
    opened = receiver.open(1U, associated_data, associated_data_size,
        ciphertext_data, ciphertext.size(), authentication_tag, plaintext);
    if (opened == FT_FALSE || plaintext.size() != plaintext_size)
        std::abort();
    if (sender.seal(2U, associated_data, associated_data_size, data,
            plaintext_size, ciphertext, authentication_tag) == FT_FALSE)
        goto cleanup;
    if (size != 0U)
        authentication_tag[data[0] % sizeof(authentication_tag)]
            ^= static_cast<uint8_t>(1U << (data[0] % 8U));
    ciphertext_data = ft_nullptr;
    if (ciphertext.size() != 0U)
        ciphertext_data = &ciphertext[0];
    opened = receiver.open(2U, associated_data, associated_data_size,
        ciphertext_data, ciphertext.size(), authentication_tag, plaintext);
    if (opened == FT_FALSE && plaintext.size() != 0U)
        std::abort();

cleanup:
    (void)plaintext.destroy();
    (void)ciphertext.destroy();
    (void)receiver.destroy();
    (void)sender.destroy();
    return ;
}

static void networking_encrypted_fuzz_run_raw(const uint8_t *data,
    ft_size_t size) noexcept
{
    const uint8_t key[32] = {0x41U};
    const uint8_t initialization_vector[12] = {0x82U};
    networking_secure_channel receiver;
    ft_vector<uint8_t> plaintext;
    uint8_t authentication_tag[16] = {0U};
    uint64_t packet_number;
    ft_size_t tag_size;
    ft_size_t associated_data_size;
    ft_size_t ciphertext_offset;
    ft_size_t ciphertext_size;
    const uint8_t *associated_data;
    const uint8_t *ciphertext;
    ft_bool opened;

    if (receiver.initialize(key, sizeof(key), initialization_vector,
            sizeof(initialization_vector)) != FT_ERR_SUCCESS)
        return ;
    networking_encrypted_fuzz_read_u64(data, size, packet_number);
    tag_size = size;
    if (tag_size > sizeof(authentication_tag))
        tag_size = sizeof(authentication_tag);
    if (tag_size != 0U)
        ft_memcpy(authentication_tag, data, tag_size);
    associated_data = ft_nullptr;
    associated_data_size = 0U;
    ciphertext_offset = sizeof(authentication_tag);
    if (size > ciphertext_offset)
    {
        associated_data = data + ciphertext_offset;
        associated_data_size = size - ciphertext_offset;
        if (associated_data_size > 32U)
            associated_data_size = 32U;
        ciphertext_offset += associated_data_size;
    }
    ciphertext = ft_nullptr;
    ciphertext_size = 0U;
    if (size > ciphertext_offset)
    {
        ciphertext = data + ciphertext_offset;
        ciphertext_size = size - ciphertext_offset;
    }
    opened = receiver.open(packet_number, associated_data,
        associated_data_size, ciphertext, ciphertext_size,
        authentication_tag, plaintext);
    if (opened == FT_FALSE && plaintext.size() != 0U)
        std::abort();
    (void)plaintext.destroy();
    (void)receiver.destroy();
    return ;
}

extern "C" int32_t LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ft_size_t bounded_size;

    if (data == ft_nullptr && size != 0U)
        return (0);
    bounded_size = static_cast<ft_size_t>(size);
    if (bounded_size > 4096U)
        bounded_size = 4096U;
    networking_encrypted_fuzz_run_valid_then_mutated(data, bounded_size);
    networking_encrypted_fuzz_run_raw(data, bounded_size);
    return (0);
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
    size_t maximum_size, unsigned int seed)
{
    return (networking_structured_fuzz_mutate(data, size, maximum_size,
        seed, 2080U));
}
