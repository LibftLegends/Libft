#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 8000U;
    constexpr uint32_t BATCH_SIZE = FT_CARD_GAME_OPERATION_INITIAL_CAPACITY;
    card_game_operation_buffer operations;
    if (operations.initialize() != FT_ERR_SUCCESS)
        return (1);
    card_game_host_intent intent{};
    intent.type_id = 31U;
    intent.version = 1U;
    intent.field_count = 3U;
    intent.fields[0] = {CARD_GAME_HOST_INTENT_U32, 0U};
    intent.fields[1] = {CARD_GAME_HOST_INTENT_U32, 123U};
    intent.fields[2] = {CARD_GAME_HOST_INTENT_U32, 456U};
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (operations.clear() != FT_ERR_SUCCESS)
            return (1);
        uint32_t operation_index = 0U;
        while (operation_index < BATCH_SIZE)
        {
            intent.fields[0].value = index + operation_index;
            if (operations.append_host_intent(intent) != FT_ERR_SUCCESS)
                return (1);
            ++operation_index;
        }
        card_game_operation operation;
        if (operations.get(BATCH_SIZE - 1U, &operation) != FT_ERR_SUCCESS
            || operation.host_intent.field_count != intent.field_count
            || operation.host_intent.fields[0].value != intent.fields[0].value)
            return (1);
        checksum += operation.host_intent.fields[0].value;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("host-intent append checksum %" PRIu64 " %.6f ns/operation\n",
        checksum, elapsed / (ITERATIONS * BATCH_SIZE));
    return (0);
}
