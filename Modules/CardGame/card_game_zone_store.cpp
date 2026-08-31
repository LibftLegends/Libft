#include "card_game_zone_store.hpp"
#include "../CMA/CMA.hpp"

card_game_zone_store::card_game_zone_store() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _definition_count(0U),
      _definitions(), _zones()
{
    return ;
}

card_game_zone_store::~card_game_zone_store() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_zone_store::release_snapshot(
    card_game_zone_store_snapshot *snapshot) noexcept
{
    if (snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot->entries != ft_nullptr)
        cma_free(snapshot->entries);
    ft_bzero(snapshot, sizeof(*snapshot));
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::get_snapshot(
    card_game_zone_store_snapshot *snapshot) const noexcept
{
    uint32_t player_id;
    uint32_t zone_index;
    uint32_t entry_index;
    uint32_t offset;
    card_game_zone_entry *entries;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    entries = ft_nullptr;
    if (this->release_snapshot(snapshot) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    snapshot->definition_count = this->_definition_count;
    ft_memcpy(snapshot->definitions, this->_definitions,
        sizeof(snapshot->definitions));
    offset = 0U;
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_index = 0U;
        while (zone_index < FT_CARD_GAME_MAX_ZONES)
        {
            snapshot->offsets[player_id][zone_index] = offset;
            snapshot->counts[player_id][zone_index] = 0U;
            if (zone_index < this->_definition_count)
            {
                snapshot->counts[player_id][zone_index] =
                    this->_zones[player_id][zone_index].size();
                offset += snapshot->counts[player_id][zone_index];
            }
            zone_index += 1U;
        }
        player_id += 1U;
    }
    snapshot->entry_count = offset;
    snapshot->entry_capacity = offset;
    if (offset != 0U)
    {
        entries = static_cast<card_game_zone_entry *>(cma_malloc(
            static_cast<ft_size_t>(offset) * sizeof(card_game_zone_entry)));
        if (entries == ft_nullptr)
        {
            this->release_snapshot(snapshot);
            return (FT_ERR_NO_MEMORY);
        }
    }
    snapshot->entries = entries;
    entry_index = 0U;
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_index = 0U;
        while (zone_index < this->_definition_count)
        {
            uint32_t local_index;

            local_index = 0U;
            while (local_index < snapshot->counts[player_id][zone_index])
            {
                if (this->_zones[player_id][zone_index].get_entry(local_index,
                        &snapshot->entries[entry_index]) != FT_ERR_SUCCESS)
                {
                    this->release_snapshot(snapshot);
                    return (FT_ERR_INVALID_STATE);
                }
                entry_index += 1U;
                local_index += 1U;
            }
            zone_index += 1U;
        }
        player_id += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::clone_snapshot(
    const card_game_zone_store_snapshot &source,
    card_game_zone_store_snapshot *destination) noexcept
{
    if (destination == ft_nullptr || (source.entry_count != 0U
            && source.entries == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (card_game_zone_store::release_snapshot(destination)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    destination->definition_count = source.definition_count;
    ft_memcpy(destination->definitions, source.definitions,
        sizeof(destination->definitions));
    ft_memcpy(destination->counts, source.counts,
        sizeof(destination->counts));
    ft_memcpy(destination->offsets, source.offsets,
        sizeof(destination->offsets));
    destination->entry_count = source.entry_count;
    destination->entry_capacity = source.entry_count;
    if (source.entry_count != 0U)
    {
        destination->entries = static_cast<card_game_zone_entry *>(cma_malloc(
            static_cast<ft_size_t>(source.entry_count)
                * sizeof(card_game_zone_entry)));
        if (destination->entries == ft_nullptr)
        {
            card_game_zone_store::release_snapshot(destination);
            return (FT_ERR_NO_MEMORY);
        }
        ft_memcpy(destination->entries, source.entries,
            static_cast<ft_size_t>(source.entry_count)
                * sizeof(card_game_zone_entry));
    }
    return (FT_ERR_SUCCESS);
}

ft_bool card_game_zone_store::snapshots_equal(
    const card_game_zone_store_snapshot &first,
    const card_game_zone_store_snapshot &second) noexcept
{
    if (first.definition_count != second.definition_count
        || first.entry_count != second.entry_count
        || ft_memcmp(first.definitions, second.definitions,
            sizeof(first.definitions)) != 0
        || ft_memcmp(first.counts, second.counts, sizeof(first.counts)) != 0
        || ft_memcmp(first.offsets, second.offsets, sizeof(first.offsets)) != 0)
        return (FT_FALSE);
    if (first.entry_count == 0U)
        return (FT_TRUE);
    if (first.entries == ft_nullptr || second.entries == ft_nullptr)
        return (FT_FALSE);
    if (ft_memcmp(first.entries, second.entries,
            static_cast<ft_size_t>(first.entry_count)
                * sizeof(card_game_zone_entry)) == 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t card_game_zone_store::apply_snapshot(
    const card_game_zone_store_snapshot &snapshot) noexcept
{
    uint32_t player_id;
    uint32_t zone_index;
    uint32_t entry_index;
    uint32_t expected_offset;
    card_game_zone_entry entry;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot.definition_count != this->_definition_count
        || (snapshot.entry_count != 0U && snapshot.entries == ft_nullptr)
        || snapshot.entry_count > FT_CARD_GAME_MAX_PLAYERS
            * FT_CARD_GAME_MAX_ZONES * FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (ft_memcmp(snapshot.definitions, this->_definitions,
            sizeof(this->_definitions)) != 0)
        return (FT_ERR_INVALID_STATE);
    expected_offset = 0U;
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_index = 0U;
        while (zone_index < FT_CARD_GAME_MAX_ZONES)
        {
            uint32_t zone_capacity;

            zone_capacity = 0U;
            if (zone_index < this->_definition_count)
                zone_capacity = this->_definitions[zone_index].capacity;
            if (snapshot.offsets[player_id][zone_index] != expected_offset
                || snapshot.counts[player_id][zone_index]
                    > zone_capacity)
                return (FT_ERR_INVALID_ARGUMENT);
            expected_offset += snapshot.counts[player_id][zone_index];
            zone_index += 1U;
        }
        player_id += 1U;
    }
    if (expected_offset != snapshot.entry_count)
        return (FT_ERR_INVALID_ARGUMENT);
    entry_index = 0U;
    while (entry_index < snapshot.entry_count)
    {
        entry = snapshot.entries[entry_index];
        if (entry.instance_id == 0U || entry.card_id == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        if (entry_index > 0U)
        {
            uint32_t previous_index;

            previous_index = 0U;
            while (previous_index < entry_index)
            {
                if (snapshot.entries[previous_index].instance_id
                    == entry.instance_id)
                    return (FT_ERR_INVALID_ARGUMENT);
                previous_index += 1U;
            }
        }
        entry_index += 1U;
    }
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_index = 0U;
        while (zone_index < this->_definition_count)
        {
            if (this->_zones[player_id][zone_index].clear()
                != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            entry_index = snapshot.offsets[player_id][zone_index];
            while (entry_index < snapshot.offsets[player_id][zone_index]
                + snapshot.counts[player_id][zone_index])
            {
                if (this->_zones[player_id][zone_index].push_bottom_entry(
                        snapshot.entries[entry_index]) != FT_ERR_SUCCESS)
                    return (FT_ERR_INVALID_STATE);
                entry_index += 1U;
            }
            zone_index += 1U;
        }
        player_id += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::initialize() noexcept
{
    uint32_t player_id;
    uint32_t zone_index;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_index = 0U;
        while (zone_index < FT_CARD_GAME_MAX_ZONES)
        {
            if (this->_zones[player_id][zone_index].initialize(
                    FT_CARD_GAME_MAX_CARDS, FT_TRUE) != FT_ERR_SUCCESS)
            {
                while (zone_index > 0U)
                {
                    zone_index -= 1U;
                    (void)this->_zones[player_id][zone_index].destroy();
                }
                while (player_id > 0U)
                {
                    player_id -= 1U;
                    zone_index = 0U;
                    while (zone_index < FT_CARD_GAME_MAX_ZONES)
                    {
                        (void)this->_zones[player_id][zone_index].destroy();
                        zone_index += 1U;
                    }
                }
                return (FT_ERR_INTERNAL);
            }
            zone_index += 1U;
        }
        player_id += 1U;
    }
    this->_definition_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::destroy() noexcept
{
    uint32_t player_id;
    uint32_t zone_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_index = 0U;
        while (zone_index < FT_CARD_GAME_MAX_ZONES)
        {
            (void)this->_zones[player_id][zone_index].destroy();
            zone_index += 1U;
        }
        player_id += 1U;
    }
    this->_definition_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::move(card_game_zone_store &other) noexcept
{
    uint32_t player_id;
    uint32_t zone_index;
    int32_t result;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    result = this->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_definition_count = other._definition_count;
    ft_memcpy(this->_definitions, other._definitions,
        sizeof(this->_definitions));
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_index = 0U;
        while (zone_index < FT_CARD_GAME_MAX_ZONES)
        {
            result = this->_zones[player_id][zone_index].move(
                other._zones[player_id][zone_index]);
            if (result != FT_ERR_SUCCESS)
                return (result);
            zone_index += 1U;
        }
        player_id += 1U;
    }
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::find_definition(uint32_t zone_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || zone_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_definition_count)
    {
        if (this->_definitions[current_index].zone_id == zone_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_zone_store::resolve_zone(uint32_t player_id,
    uint32_t zone_id, uint32_t *definition_index,
    uint32_t *zone_player) const noexcept
{
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || definition_index == ft_nullptr || zone_player == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find_definition(zone_id, definition_index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->_definitions[*definition_index].owner_scoped != FT_FALSE)
        *zone_player = player_id;
    else
        *zone_player = 0U;
    return (FT_ERR_SUCCESS);
}

ft_bool card_game_zone_store::contains_any(uint32_t player_id,
    uint32_t instance_id) const noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;

    if (instance_id == 0U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_FALSE);
    definition_index = 0U;
    while (definition_index < this->_definition_count)
    {
        zone_player = 0U;
        if (this->_definitions[definition_index].owner_scoped != FT_FALSE)
            zone_player = player_id;
        if (this->_zones[zone_player][definition_index].contains_instance(
                instance_id) != FT_FALSE)
            return (FT_TRUE);
        definition_index += 1U;
    }
    return (FT_FALSE);
}

int32_t card_game_zone_store::validate_entry(
    const card_game_zone_store_definition &definition,
    const card_game_zone_entry &entry, uint32_t card_type_id) const noexcept
{
    if (entry.instance_id == 0U || entry.card_id == 0U
        || card_type_id >= 32U
        || (definition.allowed_card_type_mask & (1U << card_type_id)) == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::register_zone(
    const card_game_zone_store_definition &definition) noexcept
{
    uint32_t player_id;
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || definition.zone_id == 0U || definition.capacity == 0U
        || definition.capacity > FT_CARD_GAME_MAX_CARDS
        || definition.allowed_card_type_mask == 0U
        || (definition.owner_scoped != FT_FALSE
            && definition.owner_scoped != FT_TRUE))
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find_definition(definition.zone_id, &index);
    if (result == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    if (this->_definition_count >= FT_CARD_GAME_MAX_ZONES)
        return (FT_ERR_FULL);
    index = this->_definition_count;
    this->_definitions[index] = definition;
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        (void)this->_zones[player_id][index].destroy();
        result = this->_zones[player_id][index].initialize(
            definition.capacity, FT_FALSE);
        if (result != FT_ERR_SUCCESS)
            return (result);
        player_id += 1U;
    }
    this->_definition_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::get_zone(uint32_t zone_id,
    card_game_zone_store_definition *definition) const noexcept
{
    uint32_t index;
    int32_t result;

    if (definition == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find_definition(zone_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *definition = this->_definitions[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::insert_top(uint32_t player_id,
    uint32_t zone_id, const card_game_zone_entry &entry,
    uint32_t card_type_id) noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;
    int32_t result;

    result = this->resolve_zone(player_id, zone_id, &definition_index,
        &zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->validate_entry(this->_definitions[definition_index], entry,
        card_type_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->contains_any(player_id, entry.instance_id) != FT_FALSE)
        return (FT_ERR_ALREADY_EXISTS);
    return (this->_zones[zone_player][definition_index].push_top_entry(entry));
}

int32_t card_game_zone_store::insert_bottom(uint32_t player_id,
    uint32_t zone_id, const card_game_zone_entry &entry,
    uint32_t card_type_id) noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;
    int32_t result;

    result = this->resolve_zone(player_id, zone_id, &definition_index,
        &zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->validate_entry(this->_definitions[definition_index], entry,
        card_type_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->contains_any(player_id, entry.instance_id) != FT_FALSE)
        return (FT_ERR_ALREADY_EXISTS);
    return (this->_zones[zone_player][definition_index].push_bottom_entry(
        entry));
}

int32_t card_game_zone_store::insert_at(uint32_t player_id, uint32_t zone_id,
    uint32_t index, const card_game_zone_entry &entry,
    uint32_t card_type_id) noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;
    int32_t result;

    result = this->resolve_zone(player_id, zone_id, &definition_index,
        &zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->validate_entry(this->_definitions[definition_index], entry,
        card_type_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->contains_any(player_id, entry.instance_id) != FT_FALSE)
        return (FT_ERR_ALREADY_EXISTS);
    return (this->_zones[zone_player][definition_index].insert_entry_at(index,
        entry));
}

int32_t card_game_zone_store::peek_top(uint32_t player_id, uint32_t zone_id,
    card_game_zone_entry *entry) const noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;
    int32_t result;

    if (entry == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->resolve_zone(player_id, zone_id, &definition_index,
        &zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (this->_zones[zone_player][definition_index].get_entry(0U, entry));
}

int32_t card_game_zone_store::pop_top(uint32_t player_id, uint32_t zone_id,
    card_game_zone_entry *entry) noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;
    int32_t result;

    if (entry == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->resolve_zone(player_id, zone_id, &definition_index,
        &zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (this->_zones[zone_player][definition_index].pop_top_entry(entry));
}

int32_t card_game_zone_store::peek_bottom(uint32_t player_id,
    uint32_t zone_id, card_game_zone_entry *entry) const noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;
    int32_t result;

    if (entry == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->resolve_zone(player_id, zone_id, &definition_index,
        &zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->_zones[zone_player][definition_index].size() == 0U)
        return (FT_ERR_EMPTY);
    return (this->_zones[zone_player][definition_index].get_entry(
        this->_zones[zone_player][definition_index].size() - 1U, entry));
}

int32_t card_game_zone_store::pop_bottom(uint32_t player_id,
    uint32_t zone_id, card_game_zone_entry *entry) noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;
    int32_t result;

    if (entry == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->resolve_zone(player_id, zone_id, &definition_index,
        &zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (this->_zones[zone_player][definition_index].pop_bottom_entry(
        entry));
}

int32_t card_game_zone_store::remove(uint32_t player_id, uint32_t zone_id,
    uint32_t instance_id, card_game_zone_entry *entry) noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;
    int32_t result;

    if (instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->resolve_zone(player_id, zone_id, &definition_index,
        &zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (this->_zones[zone_player][definition_index].remove_entry(
        instance_id, entry));
}

int32_t card_game_zone_store::move_instance(uint32_t player_id,
    uint32_t source_zone_id, uint32_t destination_zone_id,
    uint32_t instance_id, uint32_t card_type_id) noexcept
{
    uint32_t source_definition_index;
    uint32_t source_zone_player;
    uint32_t destination_definition_index;
    uint32_t destination_zone_player;
    uint32_t source_index;
    card_game_zone_entry entry;
    int32_t result;

    if (instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->resolve_zone(player_id, source_zone_id,
        &source_definition_index, &source_zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->resolve_zone(player_id, destination_zone_id,
        &destination_definition_index, &destination_zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (source_definition_index == destination_definition_index
        && source_zone_player == destination_zone_player)
        return (FT_ERR_INVALID_ARGUMENT);
    source_index = 0U;
    while (source_index < this->_zones[source_zone_player]
        [source_definition_index].size())
    {
        result = this->_zones[source_zone_player][source_definition_index]
            .get_entry(source_index, &entry);
        if (result != FT_ERR_SUCCESS)
            return (result);
        if (entry.instance_id == instance_id)
            break ;
        source_index += 1U;
    }
    if (source_index >= this->_zones[source_zone_player]
        [source_definition_index].size())
        return (FT_ERR_NOT_FOUND);
    result = this->validate_entry(
        this->_definitions[destination_definition_index], entry, card_type_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_zones[destination_zone_player][destination_definition_index]
        .push_bottom_entry(entry);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_zones[source_zone_player][source_definition_index]
        .remove_entry(instance_id, ft_nullptr);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_zones[destination_zone_player][destination_definition_index]
            .remove_entry(instance_id, ft_nullptr);
        return (result);
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::inspect(uint32_t player_id, uint32_t zone_id,
    uint32_t index, card_game_zone_entry *entry) const noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;
    int32_t result;

    if (entry == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->resolve_zone(player_id, zone_id, &definition_index,
        &zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (this->_zones[zone_player][definition_index].get_entry(index,
        entry));
}

ft_bool card_game_zone_store::contains(uint32_t player_id, uint32_t zone_id,
    uint32_t instance_id) const noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;

    if (this->resolve_zone(player_id, zone_id, &definition_index,
            &zone_player) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (this->_zones[zone_player][definition_index].contains_instance(
        instance_id));
}

int32_t card_game_zone_store::shuffle(uint32_t player_id, uint32_t zone_id,
    uint64_t *random_state) noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;
    int32_t result;

    result = this->resolve_zone(player_id, zone_id, &definition_index,
        &zone_player);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (this->_zones[zone_player][definition_index].shuffle(random_state));
}

uint32_t card_game_zone_store::size(uint32_t player_id,
    uint32_t zone_id) const noexcept
{
    uint32_t definition_index;
    uint32_t zone_player;

    if (this->resolve_zone(player_id, zone_id, &definition_index,
            &zone_player) != FT_ERR_SUCCESS)
        return (0U);
    return (this->_zones[zone_player][definition_index].size());
}
