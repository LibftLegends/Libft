#include "card_game_resolution_stack.hpp"

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno_internal.hpp"
#include "../CMA/CMA.hpp"
#include <algorithm>

card_game_resolution_stack::card_game_resolution_stack() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _capacity(0U),
      _order(CARD_GAME_RESOLUTION_LIFO),
      _admission(CARD_GAME_RESOLUTION_CLOSED), _resolving(FT_FALSE),
      _next_sequence(0U), _highest_entry_id(0U), _count(0U), _deferred_count(0U),
      _entries(ft_nullptr), _deferred(ft_nullptr)
{
    return ;
}

card_game_resolution_stack::~card_game_resolution_stack() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_resolution_stack::initialize(uint32_t capacity,
    card_game_resolution_order order,
    card_game_resolution_admission admission) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "card_game_resolution_stack::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (capacity == 0U || capacity > FT_CARD_GAME_MAX_RESOLUTION_ENTRIES
        || (order != CARD_GAME_RESOLUTION_LIFO
            && order != CARD_GAME_RESOLUTION_FIFO)
        || (admission != CARD_GAME_RESOLUTION_CLOSED
            && admission != CARD_GAME_RESOLUTION_OPEN_CURRENT_BATCH
            && admission != CARD_GAME_RESOLUTION_OPEN_DEFERRED))
        return (FT_ERR_INVALID_ARGUMENT);
    this->_entries = static_cast<card_game_resolution_entry *>(cma_malloc(
        static_cast<ft_size_t>(capacity)
            * sizeof(card_game_resolution_entry)));
    if (this->_entries == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (admission == CARD_GAME_RESOLUTION_OPEN_DEFERRED)
    {
        this->_deferred = static_cast<card_game_resolution_entry *>(cma_malloc(
            static_cast<ft_size_t>(capacity) * sizeof(card_game_resolution_entry)));
        if (this->_deferred == ft_nullptr)
        {
            cma_free(this->_entries);
            this->_entries = ft_nullptr;
            return (FT_ERR_NO_MEMORY);
        }
    }
    this->_capacity = capacity;
    this->_order = order;
    this->_admission = admission;
    this->_resolving = FT_FALSE;
    this->_next_sequence = 0U;
    this->_highest_entry_id = 0U;
    this->_count = 0U;
    this->_deferred_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resolution_stack::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    this->_capacity = 0U;
    this->_highest_entry_id = 0U;
    this->_resolving = FT_FALSE;
    this->_count = 0U;
    this->_deferred_count = 0U;
    if (this->_entries != ft_nullptr)
        cma_free(this->_entries);
    if (this->_deferred != ft_nullptr)
        cma_free(this->_deferred);
    this->_entries = ft_nullptr;
    this->_deferred = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resolution_stack::move(
    card_game_resolution_stack &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_capacity = other._capacity;
    this->_order = other._order;
    this->_admission = other._admission;
    this->_resolving = other._resolving;
    this->_next_sequence = other._next_sequence;
    this->_highest_entry_id = other._highest_entry_id;
    this->_count = other._count;
    this->_deferred_count = other._deferred_count;
    this->_entries = other._entries;
    this->_deferred = other._deferred;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._entries = ft_nullptr;
    other._deferred = ft_nullptr;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resolution_stack::clear() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    this->_count = 0U;
    this->_deferred_count = 0U;
    this->_resolving = FT_FALSE;
    this->_highest_entry_id = 0U;
    return (FT_ERR_SUCCESS);
}

static ft_bool card_game_resolution_lower_priority(
    const card_game_resolution_entry &left, const card_game_resolution_entry &right,
    card_game_resolution_order order) noexcept
{
    if (left.priority != right.priority)
        return (left.priority < right.priority);
    if (order == CARD_GAME_RESOLUTION_LIFO)
        return (left.insertion_sequence < right.insertion_sequence);
    return (left.insertion_sequence > right.insertion_sequence);
}

ft_bool card_game_resolution_stack::contains_entry(uint64_t entry_id) const noexcept
{
    if (entry_id > this->_highest_entry_id)
        return (FT_FALSE);
    uint32_t index;

    index = 0U;
    while (index < this->_count)
    {
        if (this->_entries[index].entry_id == entry_id)
            return (FT_TRUE);
        index += 1U;
    }
    index = 0U;
    while (index < this->_deferred_count)
    {
        if (this->_deferred[index].entry_id == entry_id)
            return (FT_TRUE);
        index += 1U;
    }
    return (FT_FALSE);
}

int32_t card_game_resolution_stack::push(uint64_t entry_id,
    uint32_t effect_id, uint32_t priority, uint64_t argument_data) noexcept
{
    card_game_resolution_entry entry;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (entry_id == 0U || effect_id == UINT32_MAX)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->contains_entry(entry_id) != FT_FALSE)
        return (FT_ERR_ALREADY_EXISTS);
    if (this->_resolving != FT_FALSE
        && this->_admission == CARD_GAME_RESOLUTION_CLOSED)
        return (FT_ERR_PERMISSION_DENIED);
    entry.entry_id = entry_id;
    entry.effect_id = effect_id;
    entry.priority = priority;
    entry.insertion_sequence = this->_next_sequence;
    entry.argument_data = argument_data;
    if (this->_count + this->_deferred_count >= this->_capacity)
        return (FT_ERR_FULL);
    if (this->_resolving != FT_FALSE
        && this->_admission == CARD_GAME_RESOLUTION_OPEN_DEFERRED)
        this->_deferred[this->_deferred_count++] = entry;
    else
    {
        this->_entries[this->_count++] = entry;
        std::push_heap(this->_entries, this->_entries + this->_count,
            [this](const card_game_resolution_entry &left, const card_game_resolution_entry &right)
            {
                return (card_game_resolution_lower_priority(left, right, this->_order));
            });
    }
    if (entry_id > this->_highest_entry_id)
        this->_highest_entry_id = entry_id;
    ++this->_next_sequence;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resolution_stack::begin_resolution() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_resolving != FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    if (this->_deferred_count != 0U)
    {
        const auto lower_priority = [this](const card_game_resolution_entry &left,
            const card_game_resolution_entry &right)
        {
            return (card_game_resolution_lower_priority(left, right, this->_order));
        };
        ft_memcpy(this->_entries + this->_count, this->_deferred,
            this->_deferred_count * sizeof(this->_deferred[0]));
        if (this->_deferred_count < 8U)
        {
            const uint32_t end_count = this->_count + this->_deferred_count;
            while (this->_count < end_count)
            {
                ++this->_count;
                std::push_heap(this->_entries, this->_entries + this->_count, lower_priority);
            }
        }
        else
        {
            this->_count += this->_deferred_count;
            std::make_heap(this->_entries, this->_entries + this->_count, lower_priority);
        }
    }
    this->_deferred_count = 0U;
    this->_resolving = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resolution_stack::end_resolution() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_resolving == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    this->_resolving = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resolution_stack::pop_next(
    card_game_resolution_entry *entry) noexcept
{
    if (entry == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_count == 0U)
        return (FT_ERR_EMPTY);
    std::pop_heap(this->_entries, this->_entries + this->_count,
        [this](const card_game_resolution_entry &left, const card_game_resolution_entry &right)
        {
            return (card_game_resolution_lower_priority(left, right, this->_order));
        });
    --this->_count;
    *entry = this->_entries[this->_count];
    if (this->_count == 0U && this->_deferred_count == 0U)
        this->_highest_entry_id = 0U;
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_resolution_stack::size() const noexcept
{
    return (this->_count);
}

uint32_t card_game_resolution_stack::deferred_size() const noexcept
{
    return (this->_deferred_count);
}

ft_bool card_game_resolution_stack::is_resolving() const noexcept
{
    return (this->_resolving);
}
