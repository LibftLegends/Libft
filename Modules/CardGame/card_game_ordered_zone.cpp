#include "card_game_ordered_zone.hpp"

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno_internal.hpp"

static uint64_t card_game_zone_random_next(uint64_t *state) noexcept
{
    uint64_t value;

    value = *state;
    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    *state = value;
    return (value * 2685821657736338717ULL);
}

card_game_ordered_zone::card_game_ordered_zone() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _capacity(0U),
      _allow_duplicates(FT_FALSE), _count(0U), _cards()
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
    this->_count = other._count;
    ft_memcpy(this->_cards, other._cards, sizeof(this->_cards));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::clear() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    this->_count = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::find_card(uint32_t card_instance_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    current_index = 0U;
    while (current_index < this->_count)
    {
        if (this->_cards[current_index].instance_id == card_instance_id)
        {
            if (index != ft_nullptr)
                *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
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
    uint32_t current_index;

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
    current_index = this->_count;
    while (current_index > index)
    {
        this->_cards[current_index] = this->_cards[current_index - 1U];
        current_index -= 1U;
    }
    this->_cards[index] = entry;
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
    uint32_t index;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->find_card(card_instance_id, &index) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
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
    *card_instance_id = this->_cards[index].instance_id;
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
    *entry = this->_cards[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_ordered_zone::pop_top_entry(
    card_game_zone_entry *entry) noexcept
{
    uint32_t current_index;
    int32_t get_error;

    get_error = this->get_entry(0U, entry);
    if (get_error != FT_ERR_SUCCESS)
        return (get_error);
    current_index = 1U;
    while (current_index < this->_count)
    {
        this->_cards[current_index - 1U] = this->_cards[current_index];
        current_index += 1U;
    }
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
    uint32_t current_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->find_card(instance_id, &index) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    if (entry != ft_nullptr)
        *entry = this->_cards[index];
    current_index = index + 1U;
    while (current_index < this->_count)
    {
        this->_cards[current_index - 1U] = this->_cards[current_index];
        current_index += 1U;
    }
    this->_count -= 1U;
    return (FT_ERR_SUCCESS);
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
        limit = (UINT64_MAX / range) * range;
        do
        {
            random_value = card_game_zone_random_next(random_state);
        }
        while (random_value >= limit);
        swap_index = static_cast<uint32_t>(random_value % range);
        temporary = this->_cards[index];
        this->_cards[index] = this->_cards[swap_index];
        this->_cards[swap_index] = temporary;
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
