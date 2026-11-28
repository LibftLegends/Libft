#include "../../../Modules/CardGame/card_game_script_bridge.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int argc, char **argv)
{
    const ft_bool serialize = argc == 2 && ft_strcmp(argv[1], "--serialize") == 0;
    if (argc != 1 && serialize == FT_FALSE)
        return (1);
    card_game_engine engine;
    card_game_script_bridge bridge;
    const card_game_rules rules{2U, 2U, 20U, 1U, 3U, 10U};
    if (engine.initialize(rules) != FT_ERR_SUCCESS
        || bridge.initialize(engine) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < FT_CARD_GAME_SCRIPT_MAX_CONTEXT_KEYS)
    {
        const uint32_t key_id = (index + 1U) * 3U;
        if (bridge.register_context_key(key_id) != FT_ERR_SUCCESS
            || bridge.set_context_value(key_id, index) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    std::vector<uint8_t> bytes;
    if (bridge.serialize_context_values(bytes) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iterations = 1000000U;
    if (serialize)
        iterations = 16000U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (serialize)
        {
            if (bridge.serialize_context_values(bytes) != FT_ERR_SUCCESS)
                return (1);
        }
        else
        {
            const uint32_t key_id = (index % FT_CARD_GAME_SCRIPT_MAX_CONTEXT_KEYS + 1U) * 3U;
            if (bridge.set_context_value(key_id, index) != FT_ERR_SUCCESS)
                return (1);
        }
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (bridge.serialize_context_values(bytes) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    index = 0U;
    while (index < bytes.size())
    {
        checksum = checksum * UINT64_C(31) + bytes[index];
        ++index;
    }
    std::printf("script context checksum %" PRIu64 " %.6f ns/operation\n",
        checksum, elapsed / iterations);
    return (0);
}
