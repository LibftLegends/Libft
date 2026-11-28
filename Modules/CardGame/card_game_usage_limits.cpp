#include "card_game_usage_limits.hpp"
#include "../CMA/CMA.hpp"
#include <algorithm>

card_game_usage_limit_ledger::card_game_usage_limit_ledger() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _sequential_ids(FT_TRUE), _count(0U),
      _next_id(1U), _limits(ft_nullptr)
{
    return ;
}

card_game_usage_limit_ledger::~card_game_usage_limit_ledger() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_usage_limit_ledger::release_snapshot(
    card_game_usage_limit_snapshot *snapshot) noexcept
{
    if (snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot->limits != ft_nullptr)
        cma_free(snapshot->limits);
    ft_bzero(snapshot, sizeof(*snapshot));
    return (FT_ERR_SUCCESS);
}

int32_t card_game_usage_limit_ledger::get_snapshot(
    card_game_usage_limit_snapshot *snapshot) const noexcept
{
    card_game_usage_limit *limits;
    card_game_usage_limit *old_limits;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    limits = ft_nullptr;
    if (this->_count != 0U)
    {
        limits = static_cast<card_game_usage_limit *>(cma_malloc(
            static_cast<ft_size_t>(this->_count)
                * sizeof(card_game_usage_limit)));
        if (limits == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ft_memcpy(limits, this->_limits,
            static_cast<ft_size_t>(this->_count)
                * sizeof(card_game_usage_limit));
    }
    old_limits = snapshot->limits;
    snapshot->count = this->_count;
    snapshot->next_id = this->_next_id;
    snapshot->capacity = FT_CARD_GAME_MAX_USAGE_LIMITS;
    snapshot->limits = limits;
    if (old_limits != ft_nullptr)
        cma_free(old_limits);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_usage_limit_ledger::clone_snapshot(
    const card_game_usage_limit_snapshot &source,
    card_game_usage_limit_snapshot *destination) noexcept
{
    card_game_usage_limit *limits;
    card_game_usage_limit *old_limits;

    if (destination == ft_nullptr
        || source.count > FT_CARD_GAME_MAX_USAGE_LIMITS
        || (source.count != 0U && source.limits == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (destination == &source)
        return (FT_ERR_SUCCESS);
    limits = ft_nullptr;
    if (source.count != 0U)
    {
        limits = static_cast<card_game_usage_limit *>(cma_malloc(
            static_cast<ft_size_t>(source.count)
                * sizeof(card_game_usage_limit)));
        if (limits == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ft_memcpy(limits, source.limits,
            static_cast<ft_size_t>(source.count)
                * sizeof(card_game_usage_limit));
    }
    old_limits = destination->limits;
    destination->count = source.count;
    destination->next_id = source.next_id;
    destination->capacity = source.capacity;
    destination->limits = limits;
    if (old_limits != ft_nullptr)
        cma_free(old_limits);
    return (FT_ERR_SUCCESS);
}

static uint32_t card_game_usage_duplicate_index(
    const card_game_usage_limit *limits, uint32_t count) noexcept
{
    uint32_t index = 1U;
    while (index < count && limits[index - 1U].limit_id < limits[index].limit_id)
        ++index;
    if (index >= count)
        return (count);

    uint64_t keys[FT_CARD_GAME_MAX_USAGE_LIMITS];
    index = 0U;
    while (index < count)
    {
        keys[index] = (static_cast<uint64_t>(limits[index].limit_id) << 32U) | index;
        ++index;
    }
    std::sort(keys, keys + count);
    uint32_t duplicate = count;
    index = 1U;
    while (index < count)
    {
        if ((keys[index - 1U] >> 32U) == (keys[index] >> 32U))
            duplicate = std::min(duplicate, static_cast<uint32_t>(keys[index]));
        ++index;
    }
    return (duplicate);
}

int32_t card_game_usage_limit_ledger::apply_snapshot(
    const card_game_usage_limit_snapshot &snapshot) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot.count > FT_CARD_GAME_MAX_USAGE_LIMITS
        || snapshot.capacity < snapshot.count
        || snapshot.next_id == 0U
        || (snapshot.count != 0U && snapshot.limits == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    const uint32_t duplicate = card_game_usage_duplicate_index(snapshot.limits, snapshot.count);
    ft_bool sequential = FT_TRUE;
    uint32_t index = 0U;
    while (index < snapshot.count)
    {
        const card_game_usage_limit &limit = snapshot.limits[index];
        if (limit.limit_id == 0U || limit.key_id == 0U || limit.subject_id == 0U
            || limit.maximum_uses == 0U || limit.used_uses > limit.maximum_uses
            || limit.scope > CARD_GAME_USAGE_CUSTOM
            || limit.attempt_policy > CARD_GAME_USAGE_ON_RESOLUTION)
            return (FT_ERR_INVALID_ARGUMENT);
        if (index == duplicate)
            return (FT_ERR_ALREADY_EXISTS);
        if (limit.limit_id != index + 1U)
            sequential = FT_FALSE;
        ++index;
    }
    ft_memmove(this->_limits, snapshot.limits,
        static_cast<ft_size_t>(snapshot.count)
            * sizeof(card_game_usage_limit));
    this->_count = snapshot.count;
    this->_next_id = snapshot.next_id;
    this->_sequential_ids = sequential;
    return (FT_ERR_SUCCESS);
}

ft_bool card_game_usage_limit_ledger::matches_snapshot(
    const card_game_usage_limit_snapshot &snapshot) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_FALSE);
    const card_game_usage_limit_snapshot live{
        this->_count, this->_next_id, FT_CARD_GAME_MAX_USAGE_LIMITS, this->_limits};
    return (snapshots_equal(live, snapshot));
}

ft_bool card_game_usage_limit_ledger::snapshots_equal(
    const card_game_usage_limit_snapshot &first,
    const card_game_usage_limit_snapshot &second) noexcept
{
    if (first.count != second.count || first.next_id != second.next_id)
        return (FT_FALSE);
    if (first.count == 0U)
        return (FT_TRUE);
    if (first.limits == ft_nullptr || second.limits == ft_nullptr)
        return (FT_FALSE);
    if (ft_memcmp(first.limits, second.limits,
            static_cast<ft_size_t>(first.count)
                * sizeof(card_game_usage_limit)) == 0)
        return (FT_TRUE);
    uint32_t index = 0U;
    while (index < first.count)
    {
        const card_game_usage_limit &left = first.limits[index];
        const card_game_usage_limit &right = second.limits[index];
        if (left.limit_id != right.limit_id || left.key_id != right.key_id
            || left.subject_id != right.subject_id || left.scope != right.scope
            || left.window_epoch != right.window_epoch || left.maximum_uses != right.maximum_uses
            || left.used_uses != right.used_uses || left.attempt_policy != right.attempt_policy
            || left.source_instance != right.source_instance)
            return (FT_FALSE);
        ++index;
    }
    return (FT_TRUE);
}

int32_t card_game_usage_limit_ledger::initialize() noexcept
{
    card_game_usage_limit *limits;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    limits = static_cast<card_game_usage_limit *>(cma_malloc(
        static_cast<ft_size_t>(FT_CARD_GAME_MAX_USAGE_LIMITS)
            * sizeof(card_game_usage_limit)));
    if (limits == ft_nullptr)
    {
        this->_limits = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    this->_limits = limits;
    this->_count = 0U;
    this->_next_id = 1U;
    this->_sequential_ids = FT_TRUE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_usage_limit_ledger::destroy() noexcept
{
    if (this->_limits != ft_nullptr)
        cma_free(this->_limits);
    this->_limits = ft_nullptr;
    this->_count = 0U;
    this->_next_id = 1U;
    this->_sequential_ids = FT_TRUE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_usage_limit_ledger::move(
    card_game_usage_limit_ledger &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_limits = other._limits;
    other._limits = ft_nullptr;
    this->_count = other._count;
    this->_next_id = other._next_id;
    this->_sequential_ids = other._sequential_ids;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (other.destroy());
}

int32_t card_game_usage_limit_ledger::find(uint32_t limit_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || limit_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_sequential_ids == FT_TRUE)
    {
        if (limit_id > this->_count)
            return (FT_ERR_NOT_FOUND);
        *index = limit_id - 1U;
        return (FT_ERR_SUCCESS);
    }
    current_index = 0U;
    while (current_index < this->_count)
    {
        if (this->_limits[current_index].limit_id == limit_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_usage_limit_ledger::find_matching(uint32_t key_id,
    uint32_t subject_id, card_game_usage_scope scope, uint64_t window_epoch,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || key_id == 0U || subject_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_count)
    {
        if (this->_limits[current_index].key_id == key_id
            && this->_limits[current_index].subject_id == subject_id
            && this->_limits[current_index].scope == scope
            && this->_limits[current_index].window_epoch == window_epoch)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_usage_limit_ledger::register_limit(uint32_t key_id,
    uint32_t subject_id, card_game_usage_scope scope, uint64_t window_epoch,
    uint32_t maximum_uses, card_game_usage_attempt_policy attempt_policy,
    uint32_t source_instance, uint32_t *limit_id) noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || limit_id == ft_nullptr || key_id == 0U || subject_id == 0U
        || maximum_uses == 0U || scope > CARD_GAME_USAGE_CUSTOM
        || attempt_policy > CARD_GAME_USAGE_ON_RESOLUTION
        || this->_count >= FT_CARD_GAME_MAX_USAGE_LIMITS
        || this->_next_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_matching(key_id, subject_id, scope, window_epoch, &index)
        == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    index = this->_count;
    if (this->_next_id != index + 1U)
        this->_sequential_ids = FT_FALSE;
    this->_limits[index].limit_id = this->_next_id;
    this->_limits[index].key_id = key_id;
    this->_limits[index].subject_id = subject_id;
    this->_limits[index].scope = scope;
    this->_limits[index].window_epoch = window_epoch;
    this->_limits[index].maximum_uses = maximum_uses;
    this->_limits[index].used_uses = 0U;
    this->_limits[index].attempt_policy = attempt_policy;
    this->_limits[index].source_instance = source_instance;
    *limit_id = this->_next_id;
    this->_next_id += 1U;
    this->_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_usage_limit_ledger::get(uint32_t limit_id,
    card_game_usage_limit *limit) const noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || limit == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find(limit_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *limit = this->_limits[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_usage_limit_ledger::find_consumable(uint32_t limit_id,
    uint32_t amount, uint64_t current_epoch, uint32_t *index) const noexcept
{
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || amount == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find(limit_id, index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    const card_game_usage_limit &limit = this->_limits[*index];
    if (limit.scope != CARD_GAME_USAGE_MATCH && limit.window_epoch != current_epoch)
        return (FT_ERR_PERMISSION_DENIED);
    if (amount > limit.maximum_uses - limit.used_uses)
        return (FT_ERR_FULL);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_usage_limit_ledger::can_consume(uint32_t limit_id,
    uint32_t amount, uint64_t current_epoch) const noexcept
{
    uint32_t index;
    return (this->find_consumable(limit_id, amount, current_epoch, &index));
}

int32_t card_game_usage_limit_ledger::consume(uint32_t limit_id,
    uint32_t amount, uint64_t current_epoch) noexcept
{
    int32_t result;
    uint32_t index;

    result = this->find_consumable(limit_id, amount, current_epoch, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_limits[index].used_uses += amount;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_usage_limit_ledger::reset_epoch(uint64_t current_epoch)
    noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    index = 0U;
    while (index < this->_count)
    {
        if (this->_limits[index].scope != CARD_GAME_USAGE_MATCH
            && this->_limits[index].window_epoch != current_epoch)
        {
            this->_limits[index].window_epoch = current_epoch;
            this->_limits[index].used_uses = 0U;
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_usage_limit_ledger::size() const noexcept
{
    return (this->_count);
}
