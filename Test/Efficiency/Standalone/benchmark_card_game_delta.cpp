#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    if (argument_count != 2)
        return (1);
    const ft_bool apply = ft_strcmp(arguments[1], "--apply") == 0;
    const ft_bool unchanged = ft_strcmp(arguments[1], "--unchanged") == 0;
    const ft_bool quiet = ft_strcmp(arguments[1], "--quiet-create") == 0;
    const ft_bool hash = ft_strcmp(arguments[1], "--hash") == 0;
    const ft_bool wire = ft_strcmp(arguments[1], "--wire") == 0;
    const ft_bool wire_restore = ft_strcmp(arguments[1], "--wire-restore") == 0;
    if (!apply && !unchanged && !quiet && !hash && !wire && !wire_restore
        && ft_strcmp(arguments[1], "--create") != 0)
        return (1);
    constexpr uint32_t ITERATIONS = 250U;
    uint32_t event_count = 4096U;
    uint32_t allowances = FT_CARD_GAME_MAX_ALLOWANCES;
    uint32_t players = 2U;
    if (quiet)
    {
        event_count = 0U;
        allowances = 0U;
        players = FT_CARD_GAME_MAX_PLAYERS;
    }
    card_game_engine source;
    card_game_engine replica;
    card_game_snapshot baseline;
    card_game_delta delta;
    const card_game_rules rules{4U, 4U, 20U, 5U, 10U, 10U};
    if (source.initialize(rules) != FT_ERR_SUCCESS
        || replica.initialize(rules) != FT_ERR_SUCCESS)
        return (1);
    const card_game_card_definition card{1U, CARD_GAME_CREATURE, 0U, 2, 3, CARD_GAME_NO_EFFECT};
    const card_game_zone_definition zone{2U, 4U, 1U << CARD_GAME_CREATURE, FT_TRUE};
    if ((hash || wire || wire_restore) && (source.register_zone(zone) != FT_ERR_SUCCESS
            || source.register_card(card) != FT_ERR_SUCCESS
            || replica.register_zone(zone) != FT_ERR_SUCCESS
            || replica.register_card(card) != FT_ERR_SUCCESS))
        return (1);
    card_game_host_intent_definition intent_type{};
    intent_type.type_id = 1U;
    intent_type.version = 1U;
    intent_type.field_count = 1U;
    intent_type.field_types[0U] = CARD_GAME_HOST_INTENT_U32;
    if (wire_restore && (source.register_host_intent_type(intent_type) != FT_ERR_SUCCESS
            || replica.register_host_intent_type(intent_type) != FT_ERR_SUCCESS))
        return (1);
    if (source.start_match(players) != FT_ERR_SUCCESS)
        return (1);
    if (hash || wire || wire_restore)
    {
        uint32_t resource_id;
        uint32_t instance_id;
        uint32_t choice_id;
        uint32_t limit_id;
        const card_game_choice_option option{1U, 2U, 3U};
        if (source.register_resource_pool(0U, 1U, 8U, &resource_id) != FT_ERR_SUCCESS
            || source.add_resource_units(0U, 1U, 3U, 1U, 2U, FT_TRUE, &resource_id) != FT_ERR_SUCCESS
            || source.register_usage_limit(1U, 1U, CARD_GAME_USAGE_TURN, 1U, 2U,
                CARD_GAME_USAGE_ON_RESOLUTION, 0U, &limit_id) != FT_ERR_SUCCESS
            || source.open_choice(0U, CARD_GAME_CHOICE_MODE, 3U, 1U, &choice_id) != FT_ERR_SUCCESS
            || source.add_choice_option(choice_id, option) != FT_ERR_SUCCESS
            || source.zone_push_bottom(0U, zone.zone_id, card.card_id, &instance_id) != FT_ERR_SUCCESS
            || source.zone_push_top(0U, zone.zone_id, card.card_id, &instance_id) != FT_ERR_SUCCESS
            || source.deck_push_bottom(0U, card.card_id) != FT_ERR_SUCCESS)
            return (1);
    }
    uint32_t index = 0U;
    while (index < allowances)
    {
        uint32_t allowance_id;
        if (source.grant_action_allowance(0U, index + 1U, 0U, 1U, 0U, 0U, 0U,
                0U, 0U, &allowance_id) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    if (source.get_snapshot(&baseline) != FT_ERR_SUCCESS)
        return (1);
    index = 0U;
    while (index < event_count)
    {
        if (source.emit_event(1U, index, 0U) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    if (wire_restore)
    {
        constexpr uint32_t INTENTS = 2048U;
        card_game_snapshot populated;
        std::vector<card_game_host_intent_record> records(INTENTS);
        if (source.get_snapshot(&populated) != FT_ERR_SUCCESS
            || populated.host_intents.destroy() != FT_ERR_SUCCESS)
            return (1);
        index = 0U;
        while (index < INTENTS)
        {
            records[index].sequence = index + 1U;
            records[index].action_request_id = index + 17U;
            records[index].intent.type_id = intent_type.type_id;
            records[index].intent.version = intent_type.version;
            records[index].intent.field_count = intent_type.field_count;
            records[index].intent.fields[0U] = {CARD_GAME_HOST_INTENT_U32, index};
            ++index;
        }
        if (populated.host_intents.initialize(records.data(), INTENTS) != FT_ERR_SUCCESS)
            return (1);
        populated.next_host_intent_sequence = INTENTS + 1U;
        if (source.apply_snapshot(populated) != FT_ERR_SUCCESS)
            return (1);
    }
    if (unchanged && source.get_snapshot(&baseline) != FT_ERR_SUCCESS)
        return (1);
    if (source.create_delta(baseline, &delta) != FT_ERR_SUCCESS)
        return (1);
    if (unchanged && replica.apply_snapshot(baseline) != FT_ERR_SUCCESS)
        return (1);

    double elapsed = 0.0;
    uint64_t observed_hash = 0U;
    std::vector<uint8_t> encoded;
    if (wire_restore && source.serialize_snapshot(encoded) != FT_ERR_SUCCESS)
        return (1);
    index = 0U;
    while (index < ITERATIONS)
    {
        if (apply && replica.apply_snapshot(baseline) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        int32_t status;
        if (wire_restore)
            status = replica.restore_snapshot(encoded.data(), encoded.size());
        else if (wire)
            status = source.serialize_snapshot(encoded);
        else if (hash)
            status = source.get_state_hash(&observed_hash);
        else if (apply || unchanged)
            status = replica.apply_delta(delta);
        else
            status = source.create_delta(baseline, &delta);
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        if (status != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    if (wire && replica.restore_snapshot(encoded.data(), encoded.size()) != FT_ERR_SUCCESS)
        return (1);
    if (!apply && !unchanged && !wire && !wire_restore
        && (replica.apply_snapshot(baseline) != FT_ERR_SUCCESS
            || replica.apply_delta(delta) != FT_ERR_SUCCESS))
        return (1);
    uint64_t source_hash;
    uint64_t replica_hash;
    if (source.get_state_hash(&source_hash) != FT_ERR_SUCCESS
        || replica.get_state_hash(&replica_hash) != FT_ERR_SUCCESS
        || source_hash != replica_hash || delta.event_count != event_count
        || (hash && observed_hash != source_hash))
        return (1);
    uint64_t checksum = replica_hash;
    if (wire)
    {
        checksum = UINT64_C(14695981039346656037);
        std::size_t byte_index = 0U;
        while (byte_index < encoded.size())
        {
            checksum ^= encoded[byte_index];
            checksum *= UINT64_C(1099511628211);
            ++byte_index;
        }
    }
    std::printf("delta checksum %" PRIu64 " %.6f us/operation\n", checksum, elapsed / ITERATIONS);
    return (0);
}
