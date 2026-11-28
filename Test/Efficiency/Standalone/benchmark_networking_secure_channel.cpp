#include "../../../Modules/Networking/networking_secure_channel.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_packets(uint32_t payload_length)
{
    constexpr uint64_t ITERATIONS = 12000U;
    const uint8_t key[32] = {0x11U};
    const uint8_t initialization_vector[12] = {0x22U};
    const uint8_t associated_data[28] = {0x33U};
    const uint8_t payload[1200] = {0x44U};
    uint8_t tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> plaintext;
    networking_secure_channel sender;
    networking_secure_channel receiver;
    if (sender.initialize(key, sizeof(key), initialization_vector,
            sizeof(initialization_vector)) != FT_ERR_SUCCESS
        || receiver.initialize(key, sizeof(key), initialization_vector,
            sizeof(initialization_vector)) != FT_ERR_SUCCESS)
        return (1);
    ft_size_t initial_allocations = 0U;
    if (cma_get_stats(&initial_allocations, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    uint64_t packet = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (packet < ITERATIONS)
    {
        if (!sender.seal(packet, associated_data, sizeof(associated_data),
                payload, payload_length, ciphertext, tag)
            || !receiver.open(packet, associated_data, sizeof(associated_data),
                ciphertext.begin(), ciphertext.size(), tag, plaintext))
            return (1);
        packet += 1U;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    ft_size_t final_allocations = 0U;
    if (cma_get_stats(&final_allocations, ft_nullptr) != FT_ERR_SUCCESS
        || plaintext.size() != payload_length
        || ft_memcmp(plaintext.begin(), payload, payload_length) != 0)
        return (1);
    std::printf("%u bytes: %.3f us and %.2f CMA allocations per seal/open pair\n",
        payload_length, elapsed_us / ITERATIONS,
        static_cast<double>(final_allocations - initial_allocations) / ITERATIONS);
    return (0);
}

static int32_t measure_epoch_changes()
{
    constexpr uint64_t ITERATIONS = 24000U;
    const uint8_t key[32] = {0x11U};
    const uint8_t initialization_vector[12] = {0x22U};
    networking_secure_channel source;
    networking_secure_channel destination;
    if (source.initialize(key, sizeof(key), initialization_vector,
            sizeof(initialization_vector)) != FT_ERR_SUCCESS)
        return (1);
    const auto rotate_start = std::chrono::steady_clock::now();
    uint64_t epoch = 1U;
    while (epoch <= ITERATIONS)
    {
        if (source.update_key_epoch(epoch) != FT_ERR_SUCCESS)
            return (1);
        epoch += 1U;
    }
    const double rotate_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - rotate_start).count();
    const auto move_start = std::chrono::steady_clock::now();
    uint64_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        if (destination.move(source) != FT_ERR_SUCCESS
            || source.move(destination) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double move_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - move_start).count();
    if (source.get_key_epoch() != ITERATIONS
        || !source.has_receive_epoch(ITERATIONS - 1U))
        return (1);
    std::printf("%.3f us per combined key rotation, %.3f us per channel move\n",
        rotate_us / ITERATIONS, move_us / (2U * ITERATIONS));
    return (0);
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--epochs") == 0)
        return (measure_epoch_changes());
    if (argument_count != 1)
        return (1);
    if (measure_packets(32U) != 0 || measure_packets(1200U) != 0
        || measure_epoch_changes() != 0)
        return (1);
    return (0);
}
