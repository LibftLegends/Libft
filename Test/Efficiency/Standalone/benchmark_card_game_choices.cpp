#include "../../../Modules/CardGame/card_game_choices.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    const ft_bool serialize = argument_count == 2 && ft_strcmp(arguments[1], "--serialize") == 0;
    const ft_bool restore = argument_count == 2 && ft_strcmp(arguments[1], "--restore") == 0;
    constexpr uint32_t LOOKUP_ITERATIONS = 1000000U;
    constexpr uint32_t WIRE_ITERATIONS = 1000U;
    card_game_choice_ledger ledger;
    card_game_choice_ledger restored;
    uint8_t output[FT_CARD_GAME_CHOICES_SERIALIZED_BYTES];
    uint32_t size = 0U;
    if (ledger.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < FT_CARD_GAME_MAX_CHOICES)
    {
        uint32_t choice_id = 0U;
        if (ledger.open(index, CARD_GAME_CHOICE_MODE, 123U, 0U, &choice_id) != FT_ERR_SUCCESS)
            return (1);
        uint32_t option_id = 1U;
        while (option_id <= 8U)
        {
            if (ledger.add_option(choice_id, {option_id, index, choice_id}) != FT_ERR_SUCCESS)
                return (1);
            ++option_id;
        }
        ++index;
    }
    if (ledger.serialize(output, sizeof(output), &size) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iterations = LOOKUP_ITERATIONS;
    if (serialize || restore)
        iterations = WIRE_ITERATIONS;
    uint32_t checksum = 0U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (serialize)
        {
            if (ledger.serialize(output, sizeof(output), &size) != FT_ERR_SUCCESS)
                return (1);
        }
        else if (restore)
        {
            if (restored.deserialize(output, size) != FT_ERR_SUCCESS)
                return (1);
        }
        else
        {
            card_game_choice choice{};
            if (ledger.get((index % FT_CARD_GAME_MAX_CHOICES) + 1U, &choice) != FT_ERR_SUCCESS)
                return (1);
            checksum += choice.player_id;
        }
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (restore)
    {
        if (restored.serialize(output, sizeof(output), &size) != FT_ERR_SUCCESS)
            return (1);
    }
    if (restore || serialize)
    {
        index = 0U;
        while (index < size)
        {
            checksum = checksum * 31U + output[index];
            ++index;
        }
    }
    std::printf("choices checksum %" PRIu32 " %.3f ns/op\n", checksum, elapsed / iterations);
    return (0);
}
