#include "card_game_resources.hpp"
#include "../CMA/CMA.hpp"

int32_t card_game_allowance_ledger::release_snapshot(
    card_game_allowance_snapshot *snapshot) noexcept
{
    if (snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot->allowances != ft_nullptr)
        cma_free(snapshot->allowances);
    ft_bzero(snapshot, sizeof(*snapshot));
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::get_snapshot(
    card_game_allowance_snapshot *snapshot) const noexcept
{
    card_game_action_allowance *allowances;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (card_game_allowance_ledger::release_snapshot(snapshot)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    allowances = ft_nullptr;
    if (this->_count != 0U)
    {
        allowances = static_cast<card_game_action_allowance *>(cma_malloc(
            static_cast<ft_size_t>(this->_count)
                * sizeof(card_game_action_allowance)));
        if (allowances == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ft_memcpy(allowances, this->_allowances,
            static_cast<ft_size_t>(this->_count)
                * sizeof(card_game_action_allowance));
    }
    snapshot->count = this->_count;
    snapshot->next_id = this->_next_id;
    snapshot->allowances = allowances;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::clone_snapshot(
    const card_game_allowance_snapshot &source,
    card_game_allowance_snapshot *destination) noexcept
{
    if (destination == ft_nullptr
        || source.count > FT_CARD_GAME_MAX_ALLOWANCES
        || source.next_id == 0U
        || (source.count != 0U && source.allowances == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (card_game_allowance_ledger::release_snapshot(destination)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    destination->count = source.count;
    destination->next_id = source.next_id;
    if (source.count != 0U)
    {
        destination->allowances =
            static_cast<card_game_action_allowance *>(cma_malloc(
                static_cast<ft_size_t>(source.count)
                    * sizeof(card_game_action_allowance)));
        if (destination->allowances == ft_nullptr)
        {
            (void)card_game_allowance_ledger::release_snapshot(destination);
            return (FT_ERR_NO_MEMORY);
        }
        ft_memcpy(destination->allowances, source.allowances,
            static_cast<ft_size_t>(source.count)
                * sizeof(card_game_action_allowance));
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::apply_snapshot(
    const card_game_allowance_snapshot &snapshot) noexcept
{
    uint32_t index;
    uint32_t compare_index;
    uint32_t predicate_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot.count > FT_CARD_GAME_MAX_ALLOWANCES
        || snapshot.next_id == 0U
        || (snapshot.count != 0U && snapshot.allowances == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < snapshot.count)
    {
        if (snapshot.allowances[index].allowance_id == 0U
            || snapshot.allowances[index].maximum_uses == 0U
            || snapshot.allowances[index].remaining_uses
                > snapshot.allowances[index].maximum_uses)
            return (FT_ERR_INVALID_ARGUMENT);
        if (snapshot.allowances[index].predicate_id != 0U
            && this->find_predicate(snapshot.allowances[index].predicate_id,
                &predicate_index) != FT_ERR_SUCCESS)
            return (FT_ERR_NOT_FOUND);
        compare_index = index + 1U;
        while (compare_index < snapshot.count)
        {
            if (snapshot.allowances[index].allowance_id
                == snapshot.allowances[compare_index].allowance_id)
                return (FT_ERR_INVALID_ARGUMENT);
            compare_index += 1U;
        }
        index += 1U;
    }
    ft_memcpy(this->_allowances, snapshot.allowances,
        static_cast<ft_size_t>(snapshot.count)
            * sizeof(card_game_action_allowance));
    this->_count = snapshot.count;
    this->_next_id = snapshot.next_id;
    return (FT_ERR_SUCCESS);
}

ft_bool card_game_allowance_ledger::snapshots_equal(
    const card_game_allowance_snapshot &first,
    const card_game_allowance_snapshot &second) noexcept
{
    if (first.count != second.count || first.next_id != second.next_id
        || (first.count != 0U
            && (first.allowances == ft_nullptr
                || second.allowances == ft_nullptr)))
        return (FT_FALSE);
    if (first.count != 0U
        && ft_memcmp(first.allowances, second.allowances,
            static_cast<ft_size_t>(first.count)
                * sizeof(card_game_action_allowance)) != 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

card_game_allowance_ledger::card_game_allowance_ledger() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _count(0U),
      _next_id(1U), _allowances(), _predicate_count(0U), _predicates()
{
    return ;
}

card_game_allowance_ledger::~card_game_allowance_ledger() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_allowance_ledger::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_count = 0U;
    this->_next_id = 1U;
    this->_predicate_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::destroy() noexcept
{
    this->_count = 0U;
    this->_next_id = 1U;
    this->_predicate_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::move(
    card_game_allowance_ledger &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_count = other._count;
    this->_next_id = other._next_id;
    this->_predicate_count = other._predicate_count;
    ft_memcpy(this->_allowances, other._allowances,
        sizeof(this->_allowances));
    ft_memcpy(this->_predicates, other._predicates,
        sizeof(this->_predicates));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (other.destroy());
}

int32_t card_game_allowance_ledger::find(uint32_t allowance_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || allowance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_count)
    {
        if (this->_allowances[current_index].allowance_id == allowance_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_allowance_ledger::find_predicate(uint32_t predicate_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || predicate_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_predicate_count)
    {
        if (this->_predicates[current_index].predicate_id == predicate_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

ft_bool card_game_allowance_ledger::matches(
    const card_game_action_allowance &allowance, uint32_t owner_id,
    uint32_t action_id, uint32_t action_tags, uint64_t epoch) const noexcept
{
    if (allowance.owner_id != owner_id || allowance.action_id != action_id
        || allowance.remaining_uses == 0U
        || (allowance.action_tags & action_tags) != allowance.action_tags)
        return (FT_FALSE);
    if (allowance.expiry_epoch != 0U && allowance.expiry_epoch <= epoch)
        return (FT_FALSE);
    if (allowance.predicate_id != 0U)
    {
        uint32_t predicate_index;

        if (this->find_predicate(allowance.predicate_id, &predicate_index)
            != FT_ERR_SUCCESS)
            return (FT_FALSE);
        if (this->_predicates[predicate_index].predicate(action_id, action_tags,
            this->_predicates[predicate_index].user_data) == FT_FALSE)
            return (FT_FALSE);
    }
    return (FT_TRUE);
}

int32_t card_game_allowance_ledger::register_predicate(
    uint32_t predicate_id, card_game_allowance_predicate predicate,
    void *user_data) noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || predicate_id == 0U || predicate == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_predicate(predicate_id, &index) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    if (this->_predicate_count >= FT_CARD_GAME_MAX_ALLOWANCE_PREDICATES)
        return (FT_ERR_FULL);
    index = this->_predicate_count;
    this->_predicates[index].predicate_id = predicate_id;
    this->_predicates[index].predicate = predicate;
    this->_predicates[index].user_data = user_data;
    this->_predicate_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::grant(uint32_t owner_id,
    uint32_t action_id, uint32_t action_tags, uint32_t uses,
    uint64_t expiry_epoch, uint32_t source_instance, uint32_t source_effect_id,
    uint32_t predicate_id, uint32_t predicate_context_id,
    uint32_t *allowance_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || allowance_id == ft_nullptr || action_id == 0U || uses == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_count >= FT_CARD_GAME_MAX_ALLOWANCES)
        return (FT_ERR_FULL);
    if (predicate_id != 0U)
    {
        uint32_t predicate_index;

        if (this->find_predicate(predicate_id, &predicate_index)
            != FT_ERR_SUCCESS)
            return (FT_ERR_NOT_FOUND);
    }
    this->_allowances[this->_count].allowance_id = this->_next_id;
    this->_allowances[this->_count].owner_id = owner_id;
    this->_allowances[this->_count].action_id = action_id;
    this->_allowances[this->_count].action_tags = action_tags;
    this->_allowances[this->_count].remaining_uses = uses;
    this->_allowances[this->_count].maximum_uses = uses;
    this->_allowances[this->_count].expiry_epoch = expiry_epoch;
    this->_allowances[this->_count].source_instance = source_instance;
    this->_allowances[this->_count].source_effect_id = source_effect_id;
    this->_allowances[this->_count].predicate_id = predicate_id;
    this->_allowances[this->_count].predicate_context_id = predicate_context_id;
    *allowance_id = this->_next_id;
    this->_next_id += 1U;
    this->_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::get(uint32_t allowance_id,
    card_game_action_allowance *allowance) const noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || allowance == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find(allowance_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *allowance = this->_allowances[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::count_eligible(uint32_t owner_id,
    uint32_t action_id, uint32_t action_tags, uint64_t epoch,
    uint32_t *count) const noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = 0U;
    index = 0U;
    while (index < this->_count)
    {
        if (this->matches(this->_allowances[index], owner_id, action_id,
            action_tags, epoch) != FT_FALSE)
            *count += 1U;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::consume(uint32_t allowance_id,
    uint32_t owner_id, uint32_t action_id, uint32_t action_tags,
    uint64_t epoch) noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    result = this->find(allowance_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->matches(this->_allowances[index], owner_id, action_id,
        action_tags, epoch) == FT_FALSE)
        return (FT_ERR_PERMISSION_DENIED);
    this->_allowances[index].remaining_uses -= 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::consume_first(uint32_t owner_id,
    uint32_t action_id, uint32_t action_tags, uint64_t epoch,
    uint32_t *allowance_id) noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || allowance_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_count)
    {
        if (this->matches(this->_allowances[index], owner_id, action_id,
            action_tags, epoch) != FT_FALSE)
        {
            this->_allowances[index].remaining_uses -= 1U;
            *allowance_id = this->_allowances[index].allowance_id;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_PERMISSION_DENIED);
}

int32_t card_game_allowance_ledger::reset_epoch(uint64_t epoch) noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    index = 0U;
    while (index < this->_count)
    {
        if (this->_allowances[index].expiry_epoch != 0U
            && this->_allowances[index].expiry_epoch <= epoch)
        {
            this->_allowances[index] = this->_allowances[this->_count - 1U];
            this->_count -= 1U;
            continue ;
        }
        this->_allowances[index].remaining_uses =
            this->_allowances[index].maximum_uses;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_allowance_ledger::size() const noexcept
{
    return (this->_count);
}

