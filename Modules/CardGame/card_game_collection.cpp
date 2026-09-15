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
      _product_count(0U), _collection_count(0U), _set_entries(), _products(),
      _collection()
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
    this->_collection_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_collection_engine::destroy() noexcept
{
    this->_set_entry_count = 0U;
    this->_product_count = 0U;
    this->_collection_count = 0U;
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

int32_t card_game_collection_engine::choose_printing(
    const card_game_product_slot &slot, uint64_t *random_state,
    const uint32_t *excluded_printing_ids, uint32_t excluded_count,
    uint32_t *printing_id) const noexcept
{
    uint32_t index;
    uint32_t eligible_count;
    uint64_t total_weight;
    uint64_t selected_weight;
    uint64_t running_weight;

    if (random_state == ft_nullptr || printing_id == ft_nullptr
        || *random_state == 0U
        || (excluded_count != 0U && excluded_printing_ids == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    eligible_count = 0U;
    total_weight = 0U;
    index = 0U;
    while (index < this->_set_entry_count)
    {
        ft_bool excluded;
        uint32_t excluded_index;

        excluded = FT_FALSE;
        excluded_index = 0U;
        while (excluded_index < excluded_count)
        {
            if (excluded_printing_ids[excluded_index]
                == this->_set_entries[index].printing_id)
                excluded = FT_TRUE;
            excluded_index += 1U;
        }
        if (excluded == FT_FALSE
            && this->_set_entries[index].set_id == slot.set_id
            && (this->_set_entries[index].rarity_mask & slot.rarity_mask) != 0U
            && (slot.treatment_id == 0U
                || slot.treatment_id == this->_set_entries[index].treatment_id)
            && (this->_set_entries[index].forced_rarity_mask == 0U
                || (this->_set_entries[index].forced_rarity_mask
                    & slot.rarity_mask) != 0U))
        {
            total_weight += static_cast<uint64_t>(
                this->_set_entries[index].selection_weight)
                * slot.selection_weight;
            eligible_count += 1U;
        }
        index += 1U;
    }
    if (eligible_count == 0U || total_weight == 0U)
        return (FT_ERR_NOT_FOUND);
    selected_weight = card_game_collection_random(random_state) % total_weight;
    running_weight = 0U;
    index = 0U;
    while (index < this->_set_entry_count)
    {
        ft_bool excluded;
        uint32_t excluded_index;

        excluded = FT_FALSE;
        excluded_index = 0U;
        while (excluded_index < excluded_count)
        {
            if (excluded_printing_ids[excluded_index]
                == this->_set_entries[index].printing_id)
                excluded = FT_TRUE;
            excluded_index += 1U;
        }
        if (excluded == FT_FALSE
            && this->_set_entries[index].set_id == slot.set_id
            && (this->_set_entries[index].rarity_mask & slot.rarity_mask) != 0U
            && (slot.treatment_id == 0U
                || slot.treatment_id == this->_set_entries[index].treatment_id)
            && (this->_set_entries[index].forced_rarity_mask == 0U
                || (this->_set_entries[index].forced_rarity_mask
                    & slot.rarity_mask) != 0U))
        {
            running_weight += static_cast<uint64_t>(
                this->_set_entries[index].selection_weight)
                * slot.selection_weight;
            if (selected_weight < running_weight)
            {
                *printing_id = this->_set_entries[index].printing_id;
                return (FT_ERR_SUCCESS);
            }
        }
        index += 1U;
    }
    return (FT_ERR_INTERNAL);
}

int32_t card_game_collection_engine::add_collection(uint32_t printing_id) noexcept
{
    uint32_t index;

    if (this->find_set_entry(printing_id, &index) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    index = 0U;
    while (index < this->_collection_count)
    {
        if (this->_collection[index].printing_id == printing_id)
        {
            if (this->_collection[index].quantity == UINT32_MAX)
                return (FT_ERR_FULL);
            this->_collection[index].quantity += 1U;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    if (this->_collection_count >= FT_CARD_GAME_MAX_SET_ENTRIES)
        return (FT_ERR_FULL);
    this->_collection[this->_collection_count].printing_id = printing_id;
    this->_collection[this->_collection_count].quantity = 1U;
    this->_collection_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_collection_engine::open_product(uint32_t product_id,
    uint64_t *random_state, card_game_pack_result *result) noexcept
{
    uint32_t product_index;
    uint32_t slot_index;
    uint32_t card_index;
    uint32_t prior_index;
    uint32_t collection_index;
    uint32_t slot_selected[FT_CARD_GAME_MAX_PACK_ENTRIES];
    uint32_t slot_selected_count;
    uint32_t excluded_count;
    ft_bool candidate_duplicate;
    card_game_pack_result candidate;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || random_state == ft_nullptr || result == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->find_product(product_id, &product_index);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    candidate.product_id = product_id;
    candidate.entry_count = 0U;
    slot_index = 0U;
    while (slot_index < this->_products[product_index].slot_count)
    {
        card_index = 0U;
        slot_selected_count = 0U;
        while (card_index < this->_products[product_index].slots[slot_index]
            .card_count)
        {
            if (candidate.entry_count >= FT_CARD_GAME_MAX_PACK_ENTRIES)
                return (FT_ERR_FULL);
            excluded_count = 0U;
            if (this->_products[product_index].slots[slot_index]
                .allow_duplicates == FT_FALSE)
                excluded_count = slot_selected_count;
            error_code = this->choose_printing(
                this->_products[product_index].slots[slot_index], random_state,
                slot_selected, excluded_count,
                &candidate.printing_ids[candidate.entry_count]);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            candidate.entry_count += 1U;
            if (slot_selected_count < FT_CARD_GAME_MAX_PACK_ENTRIES)
            {
                slot_selected[slot_selected_count] =
                    candidate.printing_ids[candidate.entry_count - 1U];
                slot_selected_count += 1U;
            }
            card_index += 1U;
        }
        slot_index += 1U;
    }
    card_index = 0U;
    while (card_index < candidate.entry_count)
    {
        if (this->find_set_entry(candidate.printing_ids[card_index],
            &collection_index) != FT_ERR_SUCCESS)
            return (FT_ERR_INTERNAL);
        collection_index = 0U;
        while (collection_index < this->_collection_count
            && this->_collection[collection_index].printing_id
                != candidate.printing_ids[card_index])
            collection_index += 1U;
        if (collection_index < this->_collection_count)
        {
            if (this->_collection[collection_index].quantity == UINT32_MAX)
                return (FT_ERR_FULL);
        }
        else
        {
            candidate_duplicate = FT_FALSE;
            prior_index = 0U;
            while (prior_index < card_index)
            {
                if (candidate.printing_ids[prior_index]
                    == candidate.printing_ids[card_index])
                    candidate_duplicate = FT_TRUE;
                prior_index += 1U;
            }
            if (candidate_duplicate == FT_FALSE
                && this->_collection_count >= FT_CARD_GAME_MAX_SET_ENTRIES)
                return (FT_ERR_FULL);
        }
        card_index += 1U;
    }
    card_index = 0U;
    while (card_index < candidate.entry_count)
    {
        error_code = this->add_collection(candidate.printing_ids[card_index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        card_index += 1U;
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
    index = 0U;
    while (index < this->_collection_count)
    {
        if (this->_collection[index].printing_id == printing_id)
        {
            *quantity = this->_collection[index].quantity;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    *quantity = 0U;
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
