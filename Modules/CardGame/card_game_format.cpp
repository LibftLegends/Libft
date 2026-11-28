#include "card_game_format.hpp"
#include "../Crypto/crypto_primitives.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include <algorithm>

struct card_game_format_hash_batch
{
    uint8_t bytes[1024];
    ft_size_t used;
};

template <ft_size_t WORD_COUNT>
static int32_t card_game_format_hash_words(crypto_sha256 &hash,
    card_game_format_hash_batch &batch, const uint32_t (&words)[WORD_COUNT]) noexcept
{
    static_assert(WORD_COUNT * 4U <= sizeof(batch.bytes), "format record exceeds hash buffer");
    if (batch.used > sizeof(batch.bytes) - WORD_COUNT * 4U)
    {
        const int32_t status = hash.update(batch.bytes, batch.used);
        if (status != FT_ERR_SUCCESS)
            return (status);
        batch.used = 0U;
    }
    ft_size_t index = 0U;
    while (index < WORD_COUNT)
    {
        const uint32_t word = words[index];
        uint8_t *bytes = batch.bytes + batch.used;
        bytes[0] = static_cast<uint8_t>(word);
        bytes[1] = static_cast<uint8_t>(word >> 8U);
        bytes[2] = static_cast<uint8_t>(word >> 16U);
        bytes[3] = static_cast<uint8_t>(word >> 24U);
        batch.used += 4U;
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

static void card_game_format_sort_card_rules(
    card_game_format_config *config) noexcept
{
    card_game_format_card_rule *cards = config->legal_cards;
    std::sort(cards, cards + config->legal_card_count,
        [](const card_game_format_card_rule &first, const card_game_format_card_rule &second)
        {
            return (first.card_id < second.card_id);
        });
    return ;
}

static ft_bool card_game_format_try_check_sparse_ids(
    const card_game_format_config &config, ft_bool *duplicate) noexcept
{
    constexpr uint32_t SLOT_BITS = 14U;
    constexpr uint32_t SLOT_COUNT = 1U << SLOT_BITS;
    constexpr uint32_t PROBE_LIMIT = 32U;
    if (config.legal_card_count > SLOT_COUNT / 2U)
        return (FT_FALSE);
    uint32_t slots[SLOT_COUNT]{};
    uint32_t index = 0U;
    while (index < config.legal_card_count)
    {
        const uint32_t id = config.legal_cards[index].card_id;
        const uint32_t hash = (id ^ (id >> 16U)) * 2654435761U;
        uint32_t slot = hash >> (32U - SLOT_BITS);
        uint32_t probes = 0U;
        while (slots[slot] != 0U)
        {
            if (slots[slot] == id)
            {
                *duplicate = FT_TRUE;
                return (FT_TRUE);
            }
            if (++probes == PROBE_LIMIT)
                return (FT_FALSE);
            slot = (slot + 1U) & (SLOT_COUNT - 1U);
        }
        slots[slot] = id;
        ++index;
    }
    *duplicate = FT_FALSE;
    return (FT_TRUE);
}

static ft_bool card_game_format_has_sparse_duplicate_rules(
    const card_game_format_config &config) noexcept
{
    ft_bool duplicate;
    if (card_game_format_try_check_sparse_ids(config, &duplicate) == FT_TRUE)
        return (duplicate);

    constexpr uint32_t BLOCK_SIZE = 8192U;
    uint32_t ids[BLOCK_SIZE];
    uint32_t block_start = 0U;
    while (block_start < config.legal_card_count)
    {
        const uint32_t count = std::min(BLOCK_SIZE, config.legal_card_count - block_start);
        uint32_t index = 0U;
        while (index < count)
        {
            ids[index] = config.legal_cards[block_start + index].card_id;
            ++index;
        }
        std::sort(ids, ids + count);
        if (std::adjacent_find(ids, ids + count) != ids + count)
            return (FT_TRUE);
        block_start += count;
        index = block_start;
        while (index < config.legal_card_count)
        {
            if (std::binary_search(ids, ids + count, config.legal_cards[index].card_id))
                return (FT_TRUE);
            ++index;
        }
    }
    return (FT_FALSE);
}

static ft_bool card_game_format_has_duplicate_rules(
    const card_game_format_config &config) noexcept
{
    if (config.legal_card_count < 2U)
        return (FT_FALSE);
    uint32_t minimum = config.legal_cards[0].card_id;
    uint32_t maximum = minimum;
    uint32_t previous = minimum;
    ft_bool ascending = FT_TRUE;
    ft_bool descending = FT_TRUE;
    uint32_t index = 1U;
    while (index < config.legal_card_count)
    {
        const uint32_t id = config.legal_cards[index].card_id;
        if (id == previous)
            return (FT_TRUE);
        if (id < previous)
            ascending = FT_FALSE;
        else
            descending = FT_FALSE;
        minimum = std::min(minimum, id);
        maximum = std::max(maximum, id);
        previous = id;
        ++index;
    }
    if (ascending == FT_TRUE || descending == FT_TRUE)
        return (FT_FALSE);

    constexpr uint32_t BITMAP_IDS = 65536U;
    if (maximum - minimum >= BITMAP_IDS)
        return (card_game_format_has_sparse_duplicate_rules(config));
    uint64_t seen[BITMAP_IDS / 64U]{};
    index = 0U;
    while (index < config.legal_card_count)
    {
        const uint32_t bit_index = config.legal_cards[index].card_id - minimum;
        const uint64_t mask = uint64_t{1} << (bit_index % 64U);
        uint64_t &word = seen[bit_index / 64U];
        if ((word & mask) != 0U)
            return (FT_TRUE);
        word |= mask;
        ++index;
    }
    return (FT_FALSE);
}

static uint64_t card_game_format_pair_key(uint32_t first, uint32_t second) noexcept
{
    return ((static_cast<uint64_t>(first) << 32U) | second);
}

template <typename Record, ft_size_t CAPACITY, typename Key>
static ft_bool card_game_format_unique_keys(const Record (&records)[CAPACITY],
    uint32_t count, Key key) noexcept
{
    if (count < 2U)
        return (FT_TRUE);
    uint64_t keys[CAPACITY];
    uint32_t index = 0U;
    while (index < count)
    {
        keys[index] = key(records[index]);
        ++index;
    }
    std::sort(keys, keys + count);
    return (std::adjacent_find(keys, keys + count) == keys + count);
}

static void card_game_format_sort_bans(card_game_format_config *config) noexcept
{
    if (config->ban_count < 2U)
        return ;
    uint64_t order[FT_CARD_GAME_FORMAT_MAX_BAN_ENTRIES];
    uint32_t index = 0U;
    while (index < config->ban_count)
    {
        order[index] = card_game_format_pair_key(config->bans[index].card_id, index);
        ++index;
    }
    std::sort(order, order + config->ban_count);
    index = 0U;
    while (index < config->ban_count)
    {
        uint32_t source_index = static_cast<uint32_t>(order[index]);
        if (source_index != index)
        {
            // Move each cycle in place. The original index preserves same-card order.
            const card_game_format_ban_entry first = config->bans[index];
            uint32_t destination_index = index;
            while (source_index != index)
            {
                config->bans[destination_index] = config->bans[source_index];
                order[destination_index] = destination_index;
                destination_index = source_index;
                source_index = static_cast<uint32_t>(order[destination_index]);
            }
            config->bans[destination_index] = first;
            order[destination_index] = destination_index;
        }
        ++index;
    }
    return ;
}

static void card_game_format_sort_exceptions(card_game_format_config *config) noexcept
{
    std::sort(config->exceptions, config->exceptions + config->exception_count,
        [](const card_game_format_exception &first, const card_game_format_exception &second)
        {
            return (first.card_id < second.card_id
                || (first.card_id == second.card_id && first.rule_id < second.rule_id));
        });
    return ;
}

card_game_format::card_game_format() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _config(ft_nullptr),
      _hash()
{
    return ;
}

card_game_format::~card_game_format() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_format::validate_config(
    const card_game_format_config &config) const noexcept
{
    uint32_t index;

    if (config.format_id == 0U || config.revision == 0U
        || config.profile_id == 0U || config.corpus_version == 0U
        || config.global_copy_limit == 0U
        || config.legal_card_count > FT_CARD_GAME_FORMAT_MAX_LEGAL_CARDS
        || config.ban_count > FT_CARD_GAME_FORMAT_MAX_BAN_ENTRIES
        || config.exception_count > FT_CARD_GAME_FORMAT_MAX_EXCEPTIONS
        || config.minimum_main_cards > config.maximum_main_cards
        || config.minimum_extra_cards > config.maximum_extra_cards
        || config.minimum_side_cards > config.maximum_side_cards)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < config.legal_card_count)
    {
        if (config.legal_cards[index].card_id == 0U
            || config.legal_cards[index].copy_limit == 0U
            || config.legal_cards[index].copy_limit > config.global_copy_limit
            || config.legal_cards[index].policy
                > CARD_GAME_FORMAT_CARD_RESTRICTED)
            return (FT_ERR_INVALID_ARGUMENT);
        index += 1U;
    }
    index = 0U;
    while (index < config.ban_count)
    {
        if (config.bans[index].card_id == 0U
            || config.bans[index].copy_limit > config.global_copy_limit
            || config.bans[index].zone_mask == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        index += 1U;
    }
    index = 0U;
    while (index < config.exception_count)
    {
        if (config.exceptions[index].exception_id == 0U
            || config.exceptions[index].card_id == 0U
            || config.exceptions[index].rule_id == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        index += 1U;
    }
    if (card_game_format_has_duplicate_rules(config) == FT_TRUE
        || card_game_format_unique_keys(config.bans, config.ban_count,
            [](const card_game_format_ban_entry &ban)
            {
                return (card_game_format_pair_key(ban.card_id, ban.zone_mask));
            }) == FT_FALSE
        || card_game_format_unique_keys(config.exceptions, config.exception_count,
            [](const card_game_format_exception &exception)
            {
                return (exception.exception_id);
            }) == FT_FALSE
        || card_game_format_unique_keys(config.exceptions, config.exception_count,
            [](const card_game_format_exception &exception)
            {
                return (card_game_format_pair_key(exception.card_id, exception.rule_id));
            }) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_format::calculate_hash(
    const card_game_format_config &config, card_game_format_hash *hash) const noexcept
{
    crypto_sha256 context;
    uint32_t index;
    int32_t result;

    if (hash == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = context.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    card_game_format_hash_batch batch;
    batch.used = 0U;
    result = card_game_format_hash_words(context, batch, {
        FT_CARD_GAME_FORMAT_SCHEMA_VERSION, config.format_id, config.revision,
        config.profile_id, config.corpus_version, config.global_copy_limit,
        config.minimum_main_cards, config.maximum_main_cards,
        config.minimum_extra_cards, config.maximum_extra_cards,
        config.minimum_side_cards, config.maximum_side_cards,
        config.legal_card_count, config.ban_count, config.exception_count
    });
    index = 0U;
    while (result == FT_ERR_SUCCESS && index < config.legal_card_count)
    {
        const card_game_format_card_rule &rule = config.legal_cards[index];
        result = card_game_format_hash_words(context, batch, {
            rule.card_id, rule.copy_limit, rule.limit_group_id, static_cast<uint32_t>(rule.policy)
        });
        ++index;
    }
    index = 0U;
    while (result == FT_ERR_SUCCESS && index < config.ban_count)
    {
        const card_game_format_ban_entry &ban = config.bans[index];
        result = card_game_format_hash_words(context, batch, {
            ban.card_id, ban.copy_limit, ban.zone_mask, ban.source_revision
        });
        ++index;
    }
    index = 0U;
    while (result == FT_ERR_SUCCESS && index < config.exception_count)
    {
        const card_game_format_exception &exception = config.exceptions[index];
        result = card_game_format_hash_words(context, batch, {
            exception.exception_id, exception.card_id, exception.rule_id,
            exception.replacement_program_id, exception.priority
        });
        ++index;
    }
    if (result == FT_ERR_SUCCESS)
        result = context.update(batch.bytes, batch.used);
    if (result == FT_ERR_SUCCESS)
        result = context.final(hash->bytes);
    {
        int32_t destroy_error = context.destroy();

        if (result == FT_ERR_SUCCESS)
            result = destroy_error;
    }
    return (result);
}

int32_t card_game_format::find_rule(uint32_t card_id,
    card_game_format_card_rule *rule) const noexcept
{
    if (rule == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const card_game_format_card_rule *start = this->_config->legal_cards;
    const card_game_format_card_rule *end = start + this->_config->legal_card_count;
    const card_game_format_card_rule *found = std::lower_bound(start, end, card_id,
        [](const card_game_format_card_rule &entry, uint32_t key)
        {
            return (entry.card_id < key);
        });
    if (found == end || found->card_id != card_id)
        return (FT_ERR_NOT_FOUND);
    *rule = *found;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_format::initialize(
    const card_game_format_config &config) noexcept
{
    int32_t result;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    result = this->validate_config(config);
    if (result != FT_ERR_SUCCESS)
        return (result);
    card_game_format_config *candidate = static_cast<card_game_format_config *>(cma_malloc(
        sizeof(card_game_format_config)));
    if (candidate == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    *candidate = config;
    card_game_format_sort_card_rules(candidate);
    this->_config = candidate;
    card_game_format_sort_bans(this->_config);
    card_game_format_sort_exceptions(this->_config);
    result = this->calculate_hash(*this->_config, &this->_hash);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (result);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_format::destroy() noexcept
{
    if (this->_config != ft_nullptr)
        cma_free(this->_config);
    this->_config = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_format::move(card_game_format &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_config = other._config;
    this->_hash = other._hash;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._config = ft_nullptr;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t card_game_format::get_config(
    card_game_format_config *config) const noexcept
{
    if (config == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    *config = *this->_config;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_format::get_hash(card_game_format_hash *hash) const noexcept
{
    if (hash == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    *hash = this->_hash;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_format::is_card_legal(uint32_t card_id,
    uint32_t *copy_limit) const noexcept
{
    card_game_format_card_rule rule;
    int32_t result;

    if (copy_limit == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    result = this->find_rule(card_id, &rule);
    if (result != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    *copy_limit = rule.copy_limit;
    const card_game_format_ban_entry *start = this->_config->bans;
    const card_game_format_ban_entry *end = start + this->_config->ban_count;
    const card_game_format_ban_entry *ban = std::lower_bound(start, end, card_id,
        [](const card_game_format_ban_entry &entry, uint32_t key)
        {
            return (entry.card_id < key);
        });
    if (ban != end && ban->card_id == card_id)
    {
        *copy_limit = ban->copy_limit;
        if (*copy_limit == 0U)
            return (FT_ERR_PERMISSION_DENIED);
        return (FT_ERR_SUCCESS);
    }
    if (rule.policy == CARD_GAME_FORMAT_CARD_BANNED)
        return (FT_ERR_PERMISSION_DENIED);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_format::get_exception(uint32_t card_id, uint32_t rule_id,
    card_game_format_exception *exception) const noexcept
{
    if (exception == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    const card_game_format_exception *start = this->_config->exceptions;
    const card_game_format_exception *end = start + this->_config->exception_count;
    const card_game_format_exception *found = std::lower_bound(start, end, rule_id,
        [card_id](const card_game_format_exception &entry, uint32_t key)
        {
            return (entry.card_id < card_id
                || (entry.card_id == card_id && entry.rule_id < key));
        });
    if (found == end || found->card_id != card_id || found->rule_id != rule_id)
        return (FT_ERR_NOT_FOUND);
    *exception = *found;
    return (FT_ERR_SUCCESS);
}

struct card_game_format_deck_total
{
    uint32_t card_id;
    uint64_t quantity;
};

static uint32_t card_game_format_merge_totals(card_game_format_deck_total *totals,
    uint32_t count) noexcept
{
    std::sort(totals, totals + count,
        [](const card_game_format_deck_total &first, const card_game_format_deck_total &second)
        {
            return (first.card_id < second.card_id);
        });
    uint32_t write_index = 0U;
    uint32_t entry_index = 0U;
    while (entry_index < count)
    {
        const uint32_t card_id = totals[entry_index].card_id;
        uint64_t quantity = totals[entry_index].quantity;
        ++entry_index;
        while (entry_index < count && totals[entry_index].card_id == card_id)
        {
            quantity += totals[entry_index].quantity;
            ++entry_index;
        }
        totals[write_index] = {card_id, quantity};
        ++write_index;
    }
    return (write_index);
}

int32_t card_game_format::validate_deck(const card_game_deck &deck,
    card_game_format_diagnostic *diagnostic) const noexcept
{
    uint32_t zone_index;
    uint32_t entry_index;
    uint32_t copy_limit;
    uint32_t minimum_cards;
    uint32_t maximum_cards;
    uint32_t total_cards;
    uint32_t previous_index;
    int32_t result;

    if (diagnostic == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    diagnostic->error_code = FT_ERR_SUCCESS;
    diagnostic->card_id = 0U;
    diagnostic->zone_id = 0U;
    diagnostic->observed_count = 0U;
    diagnostic->permitted_count = 0U;
    diagnostic->deciding_rule_id = 0U;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (deck.zone_count > FT_CARD_GAME_DECK_CODE_MAX_ZONES
        || deck.format_id != this->_config->format_id
        || deck.profile_id != this->_config->profile_id
        || deck.corpus_version != this->_config->corpus_version)
    {
        diagnostic->error_code = FT_ERR_PERMISSION_DENIED;
        if (deck.zone_count > FT_CARD_GAME_DECK_CODE_MAX_ZONES)
            diagnostic->error_code = FT_ERR_INVALID_ARGUMENT;
        return (diagnostic->error_code);
    }
    card_game_format_deck_total totals[FT_CARD_GAME_DECK_CODE_MAX_ZONES
        * FT_CARD_GAME_DECK_CODE_MAX_ENTRIES];
    uint32_t total_entry_count = 0U;
    zone_index = 0U;
    while (zone_index < deck.zone_count)
    {
        if (deck.zones[zone_index].entry_count
            > FT_CARD_GAME_DECK_CODE_MAX_ENTRIES)
        {
            diagnostic->error_code = FT_ERR_INVALID_ARGUMENT;
            diagnostic->zone_id = deck.zones[zone_index].zone_id;
            return (diagnostic->error_code);
        }
        previous_index = 0U;
        while (previous_index < zone_index)
        {
            if (deck.zones[previous_index].zone_id
                == deck.zones[zone_index].zone_id)
            {
                diagnostic->error_code = FT_ERR_INVALID_ARGUMENT;
                diagnostic->zone_id = deck.zones[zone_index].zone_id;
                return (diagnostic->error_code);
            }
            previous_index += 1U;
        }
        entry_index = 0U;
        while (entry_index < deck.zones[zone_index].entry_count)
        {
            if (deck.zones[zone_index].entries[entry_index].quantity == 0U)
            {
                diagnostic->error_code = FT_ERR_INVALID_ARGUMENT;
                diagnostic->card_id = deck.zones[zone_index]
                    .entries[entry_index].definition_id;
                diagnostic->zone_id = deck.zones[zone_index].zone_id;
                return (diagnostic->error_code);
            }
            previous_index = 0U;
            while (previous_index < entry_index)
            {
                if (deck.zones[zone_index].entries[previous_index]
                        .definition_id
                    == deck.zones[zone_index].entries[entry_index]
                        .definition_id)
                {
                    diagnostic->error_code = FT_ERR_INVALID_ARGUMENT;
                    diagnostic->card_id = deck.zones[zone_index]
                        .entries[entry_index].definition_id;
                    diagnostic->zone_id = deck.zones[zone_index].zone_id;
                    return (diagnostic->error_code);
                }
                previous_index += 1U;
            }
            const card_game_deck_entry &entry = deck.zones[zone_index].entries[entry_index];
            totals[total_entry_count] = {entry.definition_id, entry.quantity};
            ++total_entry_count;
            entry_index += 1U;
        }
        zone_index += 1U;
    }
    const uint32_t distinct_count = card_game_format_merge_totals(totals, total_entry_count);
    const card_game_format_deck_total *totals_start = totals;
    const card_game_format_deck_total *totals_end = totals_start + distinct_count;
    zone_index = 0U;
    while (zone_index < deck.zone_count)
    {
        total_cards = 0U;
        entry_index = 0U;
        while (entry_index < deck.zones[zone_index].entry_count)
        {
            result = this->is_card_legal(
                deck.zones[zone_index].entries[entry_index].definition_id,
                &copy_limit);
            if (result != FT_ERR_SUCCESS)
            {
                diagnostic->error_code = result;
                diagnostic->card_id = deck.zones[zone_index]
                    .entries[entry_index].definition_id;
                diagnostic->zone_id = deck.zones[zone_index].zone_id;
                diagnostic->observed_count = deck.zones[zone_index]
                    .entries[entry_index].quantity;
                diagnostic->permitted_count = 0U;
                return (result);
            }
            const uint32_t card_id = deck.zones[zone_index].entries[entry_index].definition_id;
            const card_game_format_deck_total *total = std::lower_bound(
                totals_start, totals_end, card_id,
                [](const card_game_format_deck_total &entry, uint32_t key)
                {
                    return (entry.card_id < key);
                });
            const uint64_t deck_copy_count = total->quantity;
            if (deck_copy_count > copy_limit)
            {
                diagnostic->error_code = FT_ERR_OUT_OF_RANGE;
                diagnostic->card_id = deck.zones[zone_index]
                    .entries[entry_index].definition_id;
                diagnostic->zone_id = deck.zones[zone_index].zone_id;
                diagnostic->observed_count = UINT32_MAX;
                if (deck_copy_count <= UINT32_MAX)
                    diagnostic->observed_count = static_cast<uint32_t>(deck_copy_count);
                diagnostic->permitted_count = copy_limit;
                return (FT_ERR_OUT_OF_RANGE);
            }
            if (total_cards > UINT32_MAX - deck.zones[zone_index]
                    .entries[entry_index].quantity)
                return (FT_ERR_OUT_OF_RANGE);
            total_cards += deck.zones[zone_index].entries[entry_index].quantity;
            entry_index += 1U;
        }
        minimum_cards = 0U;
        maximum_cards = UINT32_MAX;
        if (deck.zones[zone_index].zone_id == CARD_GAME_DECK_ZONE_MAIN)
        {
            minimum_cards = this->_config->minimum_main_cards;
            maximum_cards = this->_config->maximum_main_cards;
        }
        else if (deck.zones[zone_index].zone_id == CARD_GAME_DECK_ZONE_EXTRA)
        {
            minimum_cards = this->_config->minimum_extra_cards;
            maximum_cards = this->_config->maximum_extra_cards;
        }
        else if (deck.zones[zone_index].zone_id == CARD_GAME_DECK_ZONE_SIDE)
        {
            minimum_cards = this->_config->minimum_side_cards;
            maximum_cards = this->_config->maximum_side_cards;
        }
        if (total_cards < minimum_cards || total_cards > maximum_cards)
        {
            diagnostic->error_code = FT_ERR_OUT_OF_RANGE;
            diagnostic->zone_id = deck.zones[zone_index].zone_id;
            diagnostic->observed_count = total_cards;
            diagnostic->permitted_count = maximum_cards;
            return (FT_ERR_OUT_OF_RANGE);
        }
        zone_index += 1U;
    }
    return (FT_ERR_SUCCESS);
}
