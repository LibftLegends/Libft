#ifndef CARD_GAME_RESOURCES_HPP
# define CARD_GAME_RESOURCES_HPP

# include <cstdint>
# include "card_game_limits.hpp"
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_CARD_GAME_MAX_RESOURCE_POOLS = 64U;
static const uint32_t FT_CARD_GAME_MAX_RESOURCE_UNITS = 256U;
static const uint32_t FT_CARD_GAME_MAX_ALLOWANCES = 128U;
static const uint32_t FT_CARD_GAME_MAX_PAYMENT_UNITS = 64U;
static const uint32_t FT_CARD_GAME_MAX_COST_COMPONENTS = 8U;
static const uint32_t FT_CARD_GAME_MAX_COST_ALTERNATIVES = 4U;

struct card_game_resource_pool
{
    uint32_t pool_id;
    uint32_t owner_id;
    uint32_t resource_type_id;
    uint32_t maximum_amount;
    uint32_t current_amount;
    uint32_t locked_amount;
    uint32_t temporary_amount;
};

struct card_game_resource_unit
{
    uint32_t unit_id;
    uint32_t owner_id;
    uint32_t resource_type_id;
    uint32_t amount;
    uint32_t tags;
    uint64_t expiry_epoch;
    uint64_t unlock_epoch;
    ft_bool temporary;
    ft_bool locked;
    uint32_t locked_amount;
};

struct card_game_resource_requirement
{
    uint32_t resource_type_id;
    uint32_t amount;
    uint32_t required_tags;
    uint32_t forbidden_tags;
};

struct card_game_payment_unit
{
    uint32_t unit_id;
    uint32_t amount;
};

struct card_game_payment_plan
{
    uint32_t count;
    uint32_t total_amount;
    card_game_payment_unit units[FT_CARD_GAME_MAX_PAYMENT_UNITS];
};

struct card_game_resource_snapshot
{
    uint32_t pool_count;
    uint32_t unit_count;
    uint32_t next_unit_id;
    card_game_resource_pool *pools;
    card_game_resource_unit *units;
};

struct card_game_cost
{
    uint32_t component_count;
    card_game_resource_requirement components[FT_CARD_GAME_MAX_COST_COMPONENTS];
    uint32_t alternative_count;
    uint32_t alternative_component_counts[FT_CARD_GAME_MAX_COST_ALTERNATIVES];
    card_game_resource_requirement alternatives[
        FT_CARD_GAME_MAX_COST_ALTERNATIVES][FT_CARD_GAME_MAX_COST_COMPONENTS];
};

struct card_game_cost_plan
{
    uint32_t selected_alternative;
    uint32_t component_count;
    card_game_payment_plan components[FT_CARD_GAME_MAX_COST_COMPONENTS];
    card_game_payment_plan combined;
};

class card_game_resource_ledger
{
    private:
        uint8_t _initialised_state;
        uint32_t _pool_count;
        uint32_t _unit_count;
        uint32_t _next_unit_id;
        card_game_resource_pool _pools[FT_CARD_GAME_MAX_RESOURCE_POOLS];
        card_game_resource_unit _units[FT_CARD_GAME_MAX_RESOURCE_UNITS];

        card_game_resource_ledger(const card_game_resource_ledger &other) = delete;
        card_game_resource_ledger(card_game_resource_ledger &&other) = delete;
        card_game_resource_ledger &operator=(
            const card_game_resource_ledger &other) = delete;
        card_game_resource_ledger &operator=(
            card_game_resource_ledger &&other) = delete;

        int32_t find_pool(uint32_t owner_id, uint32_t resource_type_id,
            uint32_t *index) const noexcept;
        int32_t find_unit(uint32_t unit_id, uint32_t *index) const noexcept;
        ft_bool unit_matches(const card_game_resource_unit &unit,
            const card_game_resource_requirement &requirement) const noexcept;
        int32_t rebuild_pool(uint32_t pool_index) noexcept;
        int32_t create_payment_plan_excluding(uint32_t owner_id,
            const card_game_resource_requirement &requirement,
            const card_game_payment_unit *reserved_units,
            uint32_t reserved_count,
            card_game_payment_plan *plan) const noexcept;

    public:
        card_game_resource_ledger() noexcept;
        ~card_game_resource_ledger() noexcept;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_resource_ledger &other) noexcept;
        static int32_t release_snapshot(
            card_game_resource_snapshot *snapshot) noexcept;
        int32_t get_snapshot(
            card_game_resource_snapshot *snapshot) const noexcept;
        static int32_t clone_snapshot(
            const card_game_resource_snapshot &source,
            card_game_resource_snapshot *destination) noexcept;
        int32_t apply_snapshot(
            const card_game_resource_snapshot &snapshot) noexcept;
        static ft_bool snapshots_equal(
            const card_game_resource_snapshot &first,
            const card_game_resource_snapshot &second) noexcept;
        int32_t register_pool(uint32_t owner_id, uint32_t resource_type_id,
            uint32_t maximum_amount, uint32_t *pool_id) noexcept;
        int32_t get_pool(uint32_t owner_id, uint32_t resource_type_id,
            card_game_resource_pool *pool) const noexcept;
        int32_t get_unit(uint32_t unit_id,
            card_game_resource_unit *unit) const noexcept;
        int32_t set_maximum(uint32_t owner_id, uint32_t resource_type_id,
            uint32_t maximum_amount) noexcept;
        int32_t add_units(uint32_t owner_id, uint32_t resource_type_id,
            uint32_t amount, uint32_t tags, uint64_t expiry_epoch,
            ft_bool temporary, uint32_t *unit_id) noexcept;
        int32_t lock_units(uint32_t owner_id, uint32_t resource_type_id,
            uint32_t amount) noexcept;
        int32_t lock_units_until(uint32_t owner_id, uint32_t resource_type_id,
            uint32_t amount, uint64_t unlock_epoch) noexcept;
        int32_t refresh(uint32_t epoch) noexcept;
        int32_t create_payment_plan(uint32_t owner_id,
            const card_game_resource_requirement &requirement,
            card_game_payment_plan *plan) const noexcept;
        int32_t spend(const card_game_payment_plan &plan) noexcept;
        int32_t create_cost_plan(uint32_t owner_id, const card_game_cost &cost,
            uint32_t variable_amount, card_game_cost_plan *plan) const noexcept;
        int32_t spend_cost(const card_game_cost_plan &plan) noexcept;
        uint32_t pool_count() const noexcept;
        uint32_t unit_count() const noexcept;
};

typedef ft_bool (*card_game_allowance_predicate)(uint32_t action_id,
    uint32_t action_tags, void *user_data) noexcept;

static const uint32_t FT_CARD_GAME_MAX_ALLOWANCE_PREDICATES = 64U;

struct card_game_allowance_predicate_record
{
    uint32_t predicate_id;
    card_game_allowance_predicate predicate;
    void *user_data;
};

struct card_game_action_allowance
{
    uint32_t allowance_id;
    uint32_t owner_id;
    uint32_t action_id;
    uint32_t action_tags;
    uint32_t remaining_uses;
    uint32_t maximum_uses;
    uint64_t expiry_epoch;
    uint32_t source_instance;
    uint32_t source_effect_id;
    uint32_t predicate_id;
    uint32_t predicate_context_id;
};

struct card_game_allowance_snapshot
{
    uint32_t count;
    uint32_t next_id;
    card_game_action_allowance *allowances;
};

class card_game_allowance_ledger
{
    private:
        uint8_t _initialised_state;
        uint32_t _count;
        uint32_t _next_id;
        card_game_action_allowance _allowances[FT_CARD_GAME_MAX_ALLOWANCES];
        uint32_t _predicate_count;
        card_game_allowance_predicate_record _predicates[
            FT_CARD_GAME_MAX_ALLOWANCE_PREDICATES];

        card_game_allowance_ledger(const card_game_allowance_ledger &other) = delete;
        card_game_allowance_ledger(card_game_allowance_ledger &&other) = delete;
        card_game_allowance_ledger &operator=(
            const card_game_allowance_ledger &other) = delete;
        card_game_allowance_ledger &operator=(
            card_game_allowance_ledger &&other) = delete;

        int32_t find(uint32_t allowance_id, uint32_t *index) const noexcept;
        int32_t find_predicate(uint32_t predicate_id,
            uint32_t *index) const noexcept;
        ft_bool matches(const card_game_action_allowance &allowance,
            uint32_t owner_id, uint32_t action_id, uint32_t action_tags,
            uint64_t epoch) const noexcept;

    public:
        card_game_allowance_ledger() noexcept;
        ~card_game_allowance_ledger() noexcept;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_allowance_ledger &other) noexcept;
        static int32_t release_snapshot(
            card_game_allowance_snapshot *snapshot) noexcept;
        int32_t get_snapshot(
            card_game_allowance_snapshot *snapshot) const noexcept;
        static int32_t clone_snapshot(
            const card_game_allowance_snapshot &source,
            card_game_allowance_snapshot *destination) noexcept;
        int32_t apply_snapshot(
            const card_game_allowance_snapshot &snapshot) noexcept;
        static ft_bool snapshots_equal(
            const card_game_allowance_snapshot &first,
            const card_game_allowance_snapshot &second) noexcept;
        int32_t register_predicate(uint32_t predicate_id,
            card_game_allowance_predicate predicate, void *user_data) noexcept;
        int32_t grant(uint32_t owner_id, uint32_t action_id,
            uint32_t action_tags, uint32_t uses, uint64_t expiry_epoch,
            uint32_t source_instance, uint32_t source_effect_id,
            uint32_t predicate_id, uint32_t predicate_context_id,
            uint32_t *allowance_id) noexcept;
        int32_t get(uint32_t allowance_id,
            card_game_action_allowance *allowance) const noexcept;
        int32_t count_eligible(uint32_t owner_id, uint32_t action_id,
            uint32_t action_tags, uint64_t epoch, uint32_t *count) const noexcept;
        int32_t consume(uint32_t allowance_id, uint32_t owner_id,
            uint32_t action_id, uint32_t action_tags, uint64_t epoch) noexcept;
        int32_t consume_first(uint32_t owner_id, uint32_t action_id,
            uint32_t action_tags, uint64_t epoch,
            uint32_t *allowance_id) noexcept;
        int32_t reset_epoch(uint64_t epoch) noexcept;
        uint32_t size() const noexcept;
};

#endif
