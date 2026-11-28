#include "card_game_ordered_zone.hpp"
#include "card_game_rng.hpp"

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno_internal.hpp"

static_assert(FT_CARD_GAME_MAX_CARDS > 0U
    && FT_CARD_GAME_MAX_CARDS <= UINT8_MAX + 1U
    && (FT_CARD_GAME_MAX_CARDS & (FT_CARD_GAME_MAX_CARDS - 1U)) == 0U,
    "ordered zone storage must have a power-of-two length that fits an 8-bit index");

struct card_game_shuffle_limits
{
    uint64_t values[FT_CARD_GAME_MAX_CARDS];
};

static constexpr card_game_shuffle_limits card_game_make_shuffle_limits() noexcept
{
    card_game_shuffle_limits limits{};
    uint32_t index = 1U;
    while (index < FT_CARD_GAME_MAX_CARDS)
    {
        const uint64_t range = static_cast<uint64_t>(index) + 1U;
        limits.values[index] = (UINT64_MAX / range) * range;
        ++index;
    }
    return (limits);
}

static constexpr card_game_shuffle_limits SHUFFLE_LIMITS = card_game_make_shuffle_limits();

card_game_ordered_zone::card_game_ordered_zone() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _capacity(0U),
      _allow_duplicates(FT_FALSE), _head(0U), _count(0U), _cards()
{
    return ;
}

card_game_ordered_zone::~card_game_ordered_zone() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_ordered_zone::initialize(uint32_t capacity,
    ft_bool allow_duplicates) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "card_game_ordered_zone::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (capacity == 0U || capacity > FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_capacity = capacity;
    if (allow_duplicates == FT_FALSE)
        this->_allow_duplicates = FT_FALSE;
    else
        this->_allow_duplicates = FT_TRUE;
    this->_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    this->_capacity = 0U;
    this->_allow_duplicates = FT_FALSE;
    this->_head = 0U;
    this->_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::move(card_game_ordered_zone &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_capacity = other._capacity;
    this->_allow_duplicates = other._allow_duplicates;
    this->_head = other._head;
    this->_count = other._count;
    const uint32_t contiguous_capacity = FT_CARD_GAME_MAX_CARDS - this->_head;
    uint32_t contiguous_count = this->_count;
    if (contiguous_count > contiguous_capacity)
    {
        contiguous_count = contiguous_capacity;
        ft_memcpy(this->_cards, other._cards,
            (this->_count - contiguous_count) * sizeof(this->_cards[0]));
    }
    ft_memcpy(this->_cards + this->_head, other._cards + this->_head,
        contiguous_count * sizeof(this->_cards[0]));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::clear() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    this->_head = 0U;
    this->_count = 0U;
    return (FT_ERR_SUCCESS);
}

void card_game_ordered_zone::copy_entries_to(card_game_zone_entry *entries) const noexcept
{
    const uint32_t contiguous_capacity = FT_CARD_GAME_MAX_CARDS - this->_head;
    uint32_t contiguous_count = this->_count;
    if (contiguous_count > contiguous_capacity)
        contiguous_count = contiguous_capacity;
    ft_memcpy(entries, this->_cards + this->_head,
        contiguous_count * sizeof(this->_cards[0]));
    if (contiguous_count < this->_count)
        ft_memcpy(entries + contiguous_count, this->_cards,
            (this->_count - contiguous_count) * sizeof(this->_cards[0]));
    return ;
}

int32_t card_game_ordered_zone::replace_entries(const card_game_zone_entry *entries,
    uint32_t count) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || count > this->_capacity)
        return (FT_ERR_INVALID_STATE);
    if (count != 0U)
        ft_memcpy(this->_cards, entries, count * sizeof(this->_cards[0]));
    this->_head = 0U;
    this->_count = count;
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_ordered_zone::storage_index(uint32_t index) const noexcept
{
    return ((this->_head + index) & (FT_CARD_GAME_MAX_CARDS - 1U));
}

void card_game_ordered_zone::shift_entries_right(uint32_t index) noexcept
{
    const uint32_t start = this->storage_index(index);
    const uint32_t end = this->storage_index(this->_count);
    if (start < end)
    {
        ft_memmove(this->_cards + start + 1U, this->_cards + start,
            (end - start) * sizeof(this->_cards[0]));
        return ;
    }
    constexpr uint32_t LAST_SLOT = FT_CARD_GAME_MAX_CARDS - 1U;
    ft_memmove(this->_cards + 1U, this->_cards, end * sizeof(this->_cards[0]));
    this->_cards[0] = this->_cards[LAST_SLOT];
    ft_memmove(this->_cards + start + 1U, this->_cards + start,
        (LAST_SLOT - start) * sizeof(this->_cards[0]));
    return ;
}

void card_game_ordered_zone::shift_entries_left(uint32_t index) noexcept
{
    const uint32_t start = this->storage_index(index);
    const uint32_t last = this->storage_index(this->_count - 1U);
    if (start < last)
    {
        ft_memmove(this->_cards + start, this->_cards + start + 1U,
            (last - start) * sizeof(this->_cards[0]));
        return ;
    }
    constexpr uint32_t LAST_SLOT = FT_CARD_GAME_MAX_CARDS - 1U;
    ft_memmove(this->_cards + start, this->_cards + start + 1U,
        (LAST_SLOT - start) * sizeof(this->_cards[0]));
    this->_cards[LAST_SLOT] = this->_cards[0];
    ft_memmove(this->_cards, this->_cards + 1U, last * sizeof(this->_cards[0]));
    return ;
}

template <uint32_t card_game_zone_entry::*IDENTIFIER>
static const card_game_zone_entry *card_game_find_identifier_in_range(
    const card_game_zone_entry *start, const card_game_zone_entry *end,
    uint32_t identifier) noexcept
{
    while (start != end)
    {
        if (start->*IDENTIFIER == identifier)
            return (start);
        ++start;
    }
    return (ft_nullptr);
}

template <uint32_t card_game_zone_entry::*IDENTIFIER>
static int32_t card_game_find_identifier(const card_game_zone_entry *cards,
    uint32_t head, uint32_t count, uint32_t identifier, uint32_t *index) noexcept
{
    const uint32_t contiguous_capacity = FT_CARD_GAME_MAX_CARDS - head;
    uint32_t contiguous_count = count;
    if (contiguous_count > contiguous_capacity)
        contiguous_count = contiguous_capacity;
    const card_game_zone_entry *start = cards + head;
    const card_game_zone_entry *found = card_game_find_identifier_in_range<IDENTIFIER>(
        start, start + contiguous_count, identifier);
    if (found != ft_nullptr)
    {
        if (index != ft_nullptr)
            *index = static_cast<uint32_t>(found - start);
        return (FT_ERR_SUCCESS);
    }
    if (contiguous_count == count)
        return (FT_ERR_NOT_FOUND);
    found = card_game_find_identifier_in_range<IDENTIFIER>(cards,
        cards + (count - contiguous_count), identifier);
    if (found == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    if (index != ft_nullptr)
        *index = contiguous_count + static_cast<uint32_t>(found - cards);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::find_card(uint32_t card_instance_id,
    uint32_t *index) const noexcept
{
    return (card_game_find_identifier<&card_game_zone_entry::instance_id>(
        this->_cards, this->_head, this->_count, card_instance_id, index));
}

int32_t card_game_ordered_zone::find_card_definition(uint32_t card_id,
    uint32_t *index) const noexcept
{
    return (card_game_find_identifier<&card_game_zone_entry::card_id>(
        this->_cards, this->_head, this->_count, card_id, index));
}

int32_t card_game_ordered_zone::append_at(uint32_t index,
    uint32_t card_instance_id) noexcept
{
    card_game_zone_entry entry;

    entry.instance_id = card_instance_id;
    entry.card_id = card_instance_id;
    return (this->insert_entry_at(index, entry));
}

int32_t card_game_ordered_zone::push_top(uint32_t card_instance_id) noexcept
{
    return (this->insert_at(0U, card_instance_id));
}

int32_t card_game_ordered_zone::push_bottom(uint32_t card_instance_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->append_at(this->_count, card_instance_id));
}

int32_t card_game_ordered_zone::insert_at(uint32_t index,
    uint32_t card_instance_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (index > this->_count)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->append_at(index, card_instance_id));
}

int32_t card_game_ordered_zone::insert_entry_at(uint32_t index,
    const card_game_zone_entry &entry) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (index > this->_count || entry.instance_id == 0U
        || entry.card_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_count >= this->_capacity)
        return (FT_ERR_FULL);
    if (this->_allow_duplicates == FT_FALSE
        && this->find_card(entry.instance_id, ft_nullptr) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    const card_game_zone_entry inserted_entry = entry;
    if (index == 0U && this->_count != 0U)
        this->_head = static_cast<uint8_t>(this->storage_index(FT_CARD_GAME_MAX_CARDS - 1U));
    else if (index < this->_count)
        this->shift_entries_right(index);
    this->_cards[this->storage_index(index)] = inserted_entry;
    this->_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::push_top_entry(
    const card_game_zone_entry &entry) noexcept
{
    return (this->insert_entry_at(0U, entry));
}

int32_t card_game_ordered_zone::push_bottom_entry(
    const card_game_zone_entry &entry) noexcept
{
    return (this->insert_entry_at(this->_count, entry));
}

int32_t card_game_ordered_zone::peek_top(uint32_t *card_instance_id) const noexcept
{
    return (this->get(0U, card_instance_id));
}

int32_t card_game_ordered_zone::peek_bottom(
    uint32_t *card_instance_id) const noexcept
{
    if (this->_count == 0U)
        return (FT_ERR_EMPTY);
    return (this->get(this->_count - 1U, card_instance_id));
}

int32_t card_game_ordered_zone::pop_top(uint32_t *card_instance_id) noexcept
{
    card_game_zone_entry entry;
    int32_t pop_error;

    pop_error = this->pop_top_entry(&entry);
    if (pop_error != FT_ERR_SUCCESS)
        return (pop_error);
    if (card_instance_id != ft_nullptr)
        *card_instance_id = entry.instance_id;
    return (pop_error);
}

int32_t card_game_ordered_zone::pop_bottom(
    uint32_t *card_instance_id) noexcept
{
    card_game_zone_entry entry;
    int32_t pop_error;

    pop_error = this->pop_bottom_entry(&entry);
    if (pop_error != FT_ERR_SUCCESS)
        return (pop_error);
    if (card_instance_id != ft_nullptr)
        *card_instance_id = entry.instance_id;
    return (pop_error);
}

int32_t card_game_ordered_zone::remove_instance(uint32_t card_instance_id) noexcept
{
    return (this->remove_entry(card_instance_id, ft_nullptr));
}

int32_t card_game_ordered_zone::get(uint32_t index,
    uint32_t *card_instance_id) const noexcept
{
    if (card_instance_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (index >= this->_count)
        return (FT_ERR_EMPTY);
    *card_instance_id = this->_cards[this->storage_index(index)].instance_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::get_entry(uint32_t index,
    card_game_zone_entry *entry) const noexcept
{
    if (entry == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (index >= this->_count)
        return (FT_ERR_EMPTY);
    *entry = this->_cards[this->storage_index(index)];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::pop_top_entry(
    card_game_zone_entry *entry) noexcept
{
    const int32_t get_error = this->get_entry(0U, entry);
    if (get_error != FT_ERR_SUCCESS)
        return (get_error);
    this->_head = static_cast<uint8_t>(this->storage_index(1U));
    this->_count -= 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::pop_bottom_entry(
    card_game_zone_entry *entry) noexcept
{
    int32_t get_error;

    get_error = this->get_entry(this->_count - 1U, entry);
    if (get_error != FT_ERR_SUCCESS)
        return (get_error);
    this->_count -= 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::remove_entry(uint32_t instance_id,
    card_game_zone_entry *entry) noexcept
{
    uint32_t index;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->find_card(instance_id, &index) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    if (entry != ft_nullptr)
        *entry = this->_cards[this->storage_index(index)];
    this->erase_at(index);
    return (FT_ERR_SUCCESS);
}

void card_game_ordered_zone::erase_at(uint32_t index) noexcept
{
    if (index == 0U)
        this->_head = static_cast<uint8_t>(this->storage_index(1U));
    else if (index + 1U < this->_count)
        this->shift_entries_left(index);
    this->_count -= 1U;
    return ;
}

ft_bool card_game_ordered_zone::contains_instance(uint32_t instance_id) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_FALSE);
    if (this->find_card(instance_id, ft_nullptr) == FT_ERR_SUCCESS)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t card_game_ordered_zone::shuffle(uint64_t *random_state) noexcept
{
    uint32_t index;
    uint32_t swap_index;
    card_game_zone_entry temporary;
    uint64_t random_value;
    uint64_t range;
    uint64_t limit;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (random_state == ft_nullptr || *random_state == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = this->_count;
    while (index > 1U)
    {
        index -= 1U;
        range = static_cast<uint64_t>(index) + 1U;
        limit = SHUFFLE_LIMITS.values[index];
        do
        {
            random_value = card_game_random_next(random_state);
        }
        while (random_value >= limit);
        swap_index = static_cast<uint32_t>(random_value % range);
        const uint32_t storage_position = this->storage_index(index);
        const uint32_t swap_position = this->storage_index(swap_index);
        temporary = this->_cards[storage_position];
        this->_cards[storage_position] = this->_cards[swap_position];
        this->_cards[swap_position] = temporary;
    }
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_ordered_zone::size() const noexcept
{
    return (this->_count);
}

uint32_t card_game_ordered_zone::capacity() const noexcept
{
    return (this->_capacity);
}
