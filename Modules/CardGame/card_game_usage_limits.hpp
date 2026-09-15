#ifndef CARD_GAME_USAGE_LIMITS_HPP
# define CARD_GAME_USAGE_LIMITS_HPP

# include <cstdint>
# include "card_game_limits.hpp"
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_CARD_GAME_MAX_USAGE_LIMITS = 4096U;

enum card_game_usage_scope : uint8_t
{
    CARD_GAME_USAGE_ACTION = 0U,
    CARD_GAME_USAGE_CHAIN = 1U,
    CARD_GAME_USAGE_PHASE = 2U,
    CARD_GAME_USAGE_TURN = 3U,
    CARD_GAME_USAGE_ROUND = 4U,
    CARD_GAME_USAGE_MATCH = 5U,
    CARD_GAME_USAGE_CUSTOM = 6U
};

enum card_game_usage_attempt_policy : uint8_t
{
    CARD_GAME_USAGE_ON_ATTEMPT = 0U,
    CARD_GAME_USAGE_ON_ACTIVATION = 1U,
    CARD_GAME_USAGE_ON_RESOLUTION = 2U
};

struct card_game_usage_limit
{
    uint32_t limit_id;
    uint32_t key_id;
    uint32_t subject_id;
    card_game_usage_scope scope;
    uint64_t window_epoch;
    uint32_t maximum_uses;
    uint32_t used_uses;
    card_game_usage_attempt_policy attempt_policy;
    uint32_t source_instance;
};

struct card_game_usage_limit_snapshot
{
    uint32_t count;
    uint32_t next_id;
    uint32_t capacity;
    card_game_usage_limit *limits;
};

class card_game_usage_limit_ledger
{
    private:
        uint8_t _initialised_state;
        uint32_t _count;
        uint32_t _next_id;
        card_game_usage_limit *_limits;

        card_game_usage_limit_ledger(
            const card_game_usage_limit_ledger &other) = delete;
        card_game_usage_limit_ledger(
            card_game_usage_limit_ledger &&other) = delete;
        card_game_usage_limit_ledger &operator=(
            const card_game_usage_limit_ledger &other) = delete;
        card_game_usage_limit_ledger &operator=(
            card_game_usage_limit_ledger &&other) = delete;

        int32_t find(uint32_t limit_id, uint32_t *index) const noexcept;
        int32_t find_matching(uint32_t key_id, uint32_t subject_id,
            card_game_usage_scope scope, uint64_t window_epoch,
            uint32_t *index) const noexcept;

    public:
        card_game_usage_limit_ledger() noexcept;
        ~card_game_usage_limit_ledger() noexcept;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_usage_limit_ledger &other) noexcept;
        static int32_t release_snapshot(
            card_game_usage_limit_snapshot *snapshot) noexcept;
        int32_t get_snapshot(
            card_game_usage_limit_snapshot *snapshot) const noexcept;
        static int32_t clone_snapshot(
            const card_game_usage_limit_snapshot &source,
            card_game_usage_limit_snapshot *destination) noexcept;
        int32_t apply_snapshot(
            const card_game_usage_limit_snapshot &snapshot) noexcept;
        static ft_bool snapshots_equal(
            const card_game_usage_limit_snapshot &first,
            const card_game_usage_limit_snapshot &second) noexcept;
        int32_t register_limit(uint32_t key_id, uint32_t subject_id,
            card_game_usage_scope scope, uint64_t window_epoch,
            uint32_t maximum_uses,
            card_game_usage_attempt_policy attempt_policy,
            uint32_t source_instance, uint32_t *limit_id) noexcept;
        int32_t get(uint32_t limit_id,
            card_game_usage_limit *limit) const noexcept;
        int32_t can_consume(uint32_t limit_id, uint32_t amount,
            uint64_t current_epoch) const noexcept;
        int32_t consume(uint32_t limit_id, uint32_t amount,
            uint64_t current_epoch) noexcept;
        int32_t reset_epoch(uint64_t current_epoch) noexcept;
        uint32_t size() const noexcept;
};

#endif
