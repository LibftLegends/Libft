#include "card_game_collection.hpp"

static uint64_t card_game_collection_random(uint64_t *random_state) noexcept
{
    uint64_t value;

    value = *random_state;
    value ^= value << 13U;
    value ^= value >> 7U;
    value ^= value << 17U;
    *random_state = value;
    return (value);
}

card_game_collection_engine::card_game_collection_engine() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _set_entry_count(0U),
      _product_count(0U), _set_entries(), _products(), _quantities()
{
    return ;
}

card_game_collection_engine::~card_game_collection_engine() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_collection_engine::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_set_entry_count = 0U;
    this->_product_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_collection_engine::destroy() noexcept
{
    this->_set_entry_count = 0U;
    this->_product_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_collection_engine::find_set_entry(uint32_t printing_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || printing_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_set_entry_count)
    {
        if (this->_set_entries[current_index].printing_id == printing_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_collection_engine::find_product(uint32_t product_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || product_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_product_count)
    {
        if (this->_products[current_index].product_id == product_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_collection_engine::register_set_entry(
    const card_game_set_entry &entry) noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || entry.printing_id == 0U || entry.definition_id == 0U
        || entry.set_id == 0U || entry.rarity_mask == 0U
        || entry.selection_weight == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_set_entry(entry.printing_id, &index) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    if (this->_set_entry_count >= FT_CARD_GAME_MAX_SET_ENTRIES)
        return (FT_ERR_FULL);
    this->_set_entries[this->_set_entry_count] = entry;
    this->_quantities[this->_set_entry_count] = 0U;
    this->_set_entry_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_collection_engine::register_product(
    const card_game_product_definition &product) noexcept
{
    uint32_t index;
    uint32_t slot_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || product.product_id == 0U || product.slot_count == 0U
        || product.slot_count > FT_CARD_GAME_MAX_PRODUCT_SLOTS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_product(product.product_id, &index) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    slot_index = 0U;
    while (slot_index < product.slot_count)
    {
        if (product.slots[slot_index].set_id == 0U
            || product.slots[slot_index].rarity_mask == 0U
            || product.slots[slot_index].card_count == 0U
            || product.slots[slot_index].selection_weight == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        slot_index += 1U;
    }
    if (this->_product_count >= FT_CARD_GAME_MAX_PRODUCTS)
        return (FT_ERR_FULL);
    this->_products[this->_product_count] = product;
    this->_product_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_collection_engine::get_set_entry(uint32_t printing_id,
    card_game_set_entry *entry) const noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || entry == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find_set_entry(printing_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *entry = this->_set_entries[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_collection_engine::get_product(uint32_t product_id,
    card_game_product_definition *product) const noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || product == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find_product(product_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *product = this->_products[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_collection_engine::open_product(uint32_t product_id,
    uint64_t *random_state, card_game_pack_result *result) noexcept
{
    uint32_t product_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || random_state == ft_nullptr || result == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const int32_t status = this->find_product(product_id, &product_index);
    if (status != FT_ERR_SUCCESS)
        return (status);

    static_assert(FT_CARD_GAME_MAX_PACK_ENTRIES <= UINT8_MAX,
        "pack increments must fit in a byte");
    card_game_pack_result candidate{};
    candidate.product_id = product_id;
    uint32_t selected_indexes[FT_CARD_GAME_MAX_PACK_ENTRIES];
    uint64_t weights[FT_CARD_GAME_MAX_SET_ENTRIES];
    uint32_t eligible_indexes[FT_CARD_GAME_MAX_SET_ENTRIES];
    const card_game_product_definition &product = this->_products[product_index];
    uint32_t slot_index = 0U;
    while (slot_index < product.slot_count)
    {
        const card_game_product_slot &slot = product.slots[slot_index];
        uint32_t eligible_count = 0U;
        uint64_t total_weight = 0U;
        uint32_t printing_index = 0U;
        while (printing_index < this->_set_entry_count)
        {
            const card_game_set_entry &entry = this->_set_entries[printing_index];
            if (entry.set_id == slot.set_id
                && (entry.rarity_mask & slot.rarity_mask) != 0U
                && (slot.treatment_id == 0U || slot.treatment_id == entry.treatment_id)
                && (entry.forced_rarity_mask == 0U
                    || (entry.forced_rarity_mask & slot.rarity_mask) != 0U))
            {
                const uint64_t weight = static_cast<uint64_t>(entry.selection_weight)
                    * slot.selection_weight;
                total_weight += weight;
                weights[eligible_count] = weight;
                eligible_indexes[eligible_count] = printing_index;
                ++eligible_count;
            }
            ++printing_index;
        }
        uint32_t draw_index = 0U;
        while (draw_index < slot.card_count)
        {
            if (candidate.entry_count >= FT_CARD_GAME_MAX_PACK_ENTRIES)
                return (FT_ERR_FULL);
            if (*random_state == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            if (total_weight == 0U)
                return (FT_ERR_NOT_FOUND);
            const uint64_t selected_weight = card_game_collection_random(random_state)
                % total_weight;
            uint64_t running_weight = 0U;
            uint32_t selected = 0U;
            while (selected < eligible_count)
            {
                running_weight += weights[selected];
                if (selected_weight < running_weight)
                    break ;
                ++selected;
            }
            if (selected == eligible_count)
                return (FT_ERR_INTERNAL);
            const uint32_t index = eligible_indexes[selected];
            candidate.printing_ids[candidate.entry_count] = this->_set_entries[index].printing_id;
            selected_indexes[candidate.entry_count] = index;
            ++candidate.entry_count;
            if (slot.allow_duplicates == FT_FALSE)
            {
                total_weight -= weights[selected];
                weights[selected] = 0U;
            }
            ++draw_index;
        }
        ++slot_index;
    }

    uint8_t increments[FT_CARD_GAME_MAX_SET_ENTRIES]{};
    uint32_t card_index = 0U;
    while (card_index < candidate.entry_count)
    {
        const uint32_t index = selected_indexes[card_index];
        ++increments[index];
        if (this->_quantities[index] > UINT32_MAX - increments[index])
            return (FT_ERR_FULL);
        ++card_index;
    }
    card_index = 0U;
    while (card_index < candidate.entry_count)
    {
        ++this->_quantities[selected_indexes[card_index]];
        ++card_index;
    }
    *result = candidate;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_collection_engine::get_collection(uint32_t printing_id,
    uint32_t *quantity) const noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || quantity == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_set_entry(printing_id, &index) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    *quantity = this->_quantities[index];
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_collection_engine::set_entry_count() const noexcept
{
    return (this->_set_entry_count);
}

uint32_t card_game_collection_engine::product_count() const noexcept
{
    return (this->_product_count);
}
