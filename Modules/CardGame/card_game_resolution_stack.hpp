#ifndef CARD_GAME_RESOLUTION_STACK_HPP
# define CARD_GAME_RESOLUTION_STACK_HPP

# include <cstdint>
# include "card_game.hpp"

enum card_game_resolution_order : uint8_t
{
    CARD_GAME_RESOLUTION_LIFO = 0U,
    CARD_GAME_RESOLUTION_FIFO = 1U
};

enum card_game_resolution_admission : uint8_t
{
    CARD_GAME_RESOLUTION_CLOSED = 0U,
    CARD_GAME_RESOLUTION_OPEN_CURRENT_BATCH = 1U,
    CARD_GAME_RESOLUTION_OPEN_DEFERRED = 2U
};

struct card_game_resolution_entry
{
    uint64_t entry_id;
    uint32_t effect_id;
    uint32_t priority;
    uint64_t insertion_sequence;
    uint64_t argument_data;
};

class card_game_resolution_stack
{
    private:
        uint8_t _initialised_state;
        uint32_t _capacity;
        card_game_resolution_order _order;
        card_game_resolution_admission _admission;
        ft_bool _resolving;
        uint64_t _next_sequence;
        uint32_t _count;
        uint32_t _deferred_count;
        card_game_resolution_entry _entries[FT_CARD_GAME_MAX_OPERATIONS];
        card_game_resolution_entry _deferred[FT_CARD_GAME_MAX_OPERATIONS];

        card_game_resolution_stack(const card_game_resolution_stack &other) = delete;
        card_game_resolution_stack(card_game_resolution_stack &&other) = delete;
        card_game_resolution_stack &operator=(
            const card_game_resolution_stack &other) = delete;
        card_game_resolution_stack &operator=(
            card_game_resolution_stack &&other) = delete;

        int32_t append_entry(card_game_resolution_entry *entries,
            uint32_t *count, const card_game_resolution_entry &entry) noexcept;
        int32_t remove_entry(uint32_t index,
            card_game_resolution_entry *entry) noexcept;

    public:
        card_game_resolution_stack() noexcept;
        ~card_game_resolution_stack() noexcept;

        int32_t initialize(uint32_t capacity,
            card_game_resolution_order order,
            card_game_resolution_admission admission) noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_resolution_stack &other) noexcept;
        int32_t clear() noexcept;
        int32_t push(uint64_t entry_id, uint32_t effect_id,
            uint32_t priority, uint64_t argument_data) noexcept;
        int32_t begin_resolution() noexcept;
        int32_t end_resolution() noexcept;
        int32_t pop_next(card_game_resolution_entry *entry) noexcept;
        uint32_t size() const noexcept;
        uint32_t deferred_size() const noexcept;
        ft_bool is_resolving() const noexcept;
};

#endif
