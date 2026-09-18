#ifndef CARD_GAME_ZONE_STORE_HPP
# define CARD_GAME_ZONE_STORE_HPP

# include <cstdint>
# include "card_game_ordered_zone.hpp"
# include "card_game_limits.hpp"
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

struct card_game_zone_store_definition
{
    uint32_t zone_id;
    uint32_t capacity;
    uint32_t allowed_card_type_mask;
    ft_bool owner_scoped;
};

struct card_game_zone_store_snapshot
{
    uint32_t definition_count;
    card_game_zone_store_definition definitions[FT_CARD_GAME_MAX_ZONES];
    uint32_t counts[FT_CARD_GAME_MAX_PLAYERS][FT_CARD_GAME_MAX_ZONES];
    uint32_t offsets[FT_CARD_GAME_MAX_PLAYERS][FT_CARD_GAME_MAX_ZONES];
    uint32_t entry_count;
    uint32_t entry_capacity;
    card_game_zone_entry *entries;
};

class card_game_zone_store
{
    private:
        uint8_t _initialised_state;
        uint32_t _definition_count;
        card_game_zone_store_definition _definitions[FT_CARD_GAME_MAX_ZONES];
        card_game_ordered_zone _zones[FT_CARD_GAME_MAX_PLAYERS]
            [FT_CARD_GAME_MAX_ZONES];

        card_game_zone_store(const card_game_zone_store &other) = delete;
        card_game_zone_store(card_game_zone_store &&other) = delete;
        card_game_zone_store &operator=(
            const card_game_zone_store &other) = delete;
        card_game_zone_store &operator=(
            card_game_zone_store &&other) = delete;

        int32_t find_definition(uint32_t zone_id,
            uint32_t *index) const noexcept;
        int32_t resolve_zone(uint32_t player_id, uint32_t zone_id,
            uint32_t *definition_index, uint32_t *zone_player) const noexcept;
        ft_bool contains_any(uint32_t player_id,
            uint32_t instance_id) const noexcept;
        int32_t validate_entry(const card_game_zone_store_definition &definition,
            const card_game_zone_entry &entry, uint32_t card_type_id) const noexcept;

    public:
        card_game_zone_store() noexcept;
        ~card_game_zone_store() noexcept;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_zone_store &other) noexcept;
        static int32_t release_snapshot(
            card_game_zone_store_snapshot *snapshot) noexcept;
        int32_t get_snapshot(
            card_game_zone_store_snapshot *snapshot) const noexcept;
        static int32_t clone_snapshot(
            const card_game_zone_store_snapshot &source,
            card_game_zone_store_snapshot *destination) noexcept;
        int32_t apply_snapshot(
            const card_game_zone_store_snapshot &snapshot) noexcept;
        static ft_bool snapshots_equal(
            const card_game_zone_store_snapshot &first,
            const card_game_zone_store_snapshot &second) noexcept;
        int32_t register_zone(
            const card_game_zone_store_definition &definition) noexcept;
        int32_t get_zone(uint32_t zone_id,
            card_game_zone_store_definition *definition) const noexcept;
        int32_t insert_top(uint32_t player_id, uint32_t zone_id,
            const card_game_zone_entry &entry, uint32_t card_type_id) noexcept;
        int32_t insert_bottom(uint32_t player_id, uint32_t zone_id,
            const card_game_zone_entry &entry, uint32_t card_type_id) noexcept;
        int32_t insert_at(uint32_t player_id, uint32_t zone_id, uint32_t index,
            const card_game_zone_entry &entry, uint32_t card_type_id) noexcept;
        int32_t peek_top(uint32_t player_id, uint32_t zone_id,
            card_game_zone_entry *entry) const noexcept;
        int32_t pop_top(uint32_t player_id, uint32_t zone_id,
            card_game_zone_entry *entry) noexcept;
        int32_t peek_bottom(uint32_t player_id, uint32_t zone_id,
            card_game_zone_entry *entry) const noexcept;
        int32_t pop_bottom(uint32_t player_id, uint32_t zone_id,
            card_game_zone_entry *entry) noexcept;
        int32_t remove(uint32_t player_id, uint32_t zone_id,
            uint32_t instance_id, card_game_zone_entry *entry) noexcept;
        int32_t move_instance(uint32_t player_id, uint32_t source_zone_id,
            uint32_t destination_zone_id, uint32_t instance_id,
            uint32_t card_type_id) noexcept;
        int32_t inspect(uint32_t player_id, uint32_t zone_id, uint32_t index,
            card_game_zone_entry *entry) const noexcept;
        ft_bool contains(uint32_t player_id, uint32_t zone_id,
            uint32_t instance_id) const noexcept;
        int32_t shuffle(uint32_t player_id, uint32_t zone_id,
            uint64_t *random_state) noexcept;
        uint32_t size(uint32_t player_id, uint32_t zone_id) const noexcept;
};

#endif
