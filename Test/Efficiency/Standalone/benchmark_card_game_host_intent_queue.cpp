#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <vector>

int main()
{
    constexpr uint32_t COUNT = 8192U;
    card_game_engine engine;
    card_game_snapshot pending;
    const card_game_rules rules{4U, 4U, 20U, 5U, 10U, 10U};
    card_game_host_intent_definition definition{};
    definition.type_id = 1U;
    definition.version = 1U;
    if (engine.initialize(rules) != FT_ERR_SUCCESS
        || engine.register_host_intent_type(definition) != FT_ERR_SUCCESS
        || engine.start_match(2U) != FT_ERR_SUCCESS
        || engine.get_snapshot(&pending) != FT_ERR_SUCCESS)
        return (1);

    std::vector<card_game_host_intent_record> records(COUNT);
    uint32_t index = 0U;
    while (index < COUNT)
    {
        records[index].sequence = index + 1U;
        records[index].intent.type_id = definition.type_id;
        records[index].intent.version = definition.version;
        ++index;
    }
    if (pending.host_intents.destroy() != FT_ERR_SUCCESS
        || pending.host_intents.initialize(records.data(), COUNT) != FT_ERR_SUCCESS)
        return (1);
    pending.next_host_intent_sequence = COUNT + 1U;
    if (engine.apply_snapshot(pending) != FT_ERR_SUCCESS)
        return (1);

    uint64_t checksum = 0U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < COUNT)
    {
        card_game_host_intent_record front{};
        if (engine.peek_pending_host_intent(&front) != FT_ERR_SUCCESS
            || engine.acknowledge_host_intent(front.sequence) != FT_ERR_SUCCESS)
            return (1);
        checksum = checksum * 31U + front.sequence;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    uint32_t pending_count;
    if (engine.get_pending_host_intent_count(&pending_count) != FT_ERR_SUCCESS
        || pending_count != 0U)
        return (1);
    std::printf("host intent drain checksum %" PRIu64 " %.6f ms/drain\n", checksum, elapsed);
    return (0);
}
