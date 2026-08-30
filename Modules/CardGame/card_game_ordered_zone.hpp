#ifndef CARD_GAME_ORDERED_ZONE_HPP
# define CARD_GAME_ORDERED_ZONE_HPP

# include <cstdint>
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"
# include "card_game_limits.hpp"

class card_game_ordered_zone
{
    private:
        uint8_t _initialised_state;
        uint32_t _capacity;
        ft_bool _allow_duplicates;
        uint32_t _count;
        uint32_t _cards[FT_CARD_GAME_MAX_CARDS];

        card_game_ordered_zone(const card_game_ordered_zone &other) = delete;
        card_game_ordered_zone(card_game_ordered_zone &&other) = delete;
        card_game_ordered_zone &operator=(
            const card_game_ordered_zone &other) = delete;
        card_game_ordered_zone &operator=(
            card_game_ordered_zone &&other) = delete;

        int32_t find_card(uint32_t card_instance_id,
            uint32_t *index) const noexcept;
        int32_t append_at(uint32_t index, uint32_t card_instance_id) noexcept;

    public:
        card_game_ordered_zone() noexcept;
        ~card_game_ordered_zone() noexcept;

        int32_t initialize(uint32_t capacity, ft_bool allow_duplicates) noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_ordered_zone &other) noexcept;
        int32_t clear() noexcept;
        int32_t push_top(uint32_t card_instance_id) noexcept;
        int32_t push_bottom(uint32_t card_instance_id) noexcept;
        int32_t insert_at(uint32_t index, uint32_t card_instance_id) noexcept;
        int32_t peek_top(uint32_t *card_instance_id) const noexcept;
        int32_t peek_bottom(uint32_t *card_instance_id) const noexcept;
        int32_t pop_top(uint32_t *card_instance_id) noexcept;
        int32_t pop_bottom(uint32_t *card_instance_id) noexcept;
        int32_t remove_instance(uint32_t card_instance_id) noexcept;
        int32_t get(uint32_t index, uint32_t *card_instance_id) const noexcept;
        int32_t shuffle(uint64_t *random_state) noexcept;
        uint32_t size() const noexcept;
        uint32_t capacity() const noexcept;
};

#endif
