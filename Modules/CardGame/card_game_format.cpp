#include "card_game_format.hpp"
#include "../Crypto/crypto_primitives.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"

static void card_game_format_hash_u32(crypto_sha256 &hash,
    uint32_t value) noexcept
{
    uint8_t bytes[4];

    bytes[0] = static_cast<uint8_t>(value & 255U);
    bytes[1] = static_cast<uint8_t>((value >> 8U) & 255U);
    bytes[2] = static_cast<uint8_t>((value >> 16U) & 255U);
    bytes[3] = static_cast<uint8_t>((value >> 24U) & 255U);
    (void)hash.update(bytes, sizeof(bytes));
    return ;
}

static void card_game_format_hash_bytes(crypto_sha256 &hash,
    const void *data, ft_size_t size) noexcept
{
    (void)hash.update(data, size);
    return ;
}

static void card_game_format_sort_card_rules(
    card_game_format_config *config) noexcept
{
    uint32_t index;
    uint32_t previous_index;
    card_game_format_card_rule value;

    index = 1U;
    while (index < config->legal_card_count)
    {
        value = config->legal_cards[index];
        previous_index = index;
        while (previous_index > 0U
            && config->legal_cards[previous_index - 1U].card_id
                > value.card_id)
        {
            config->legal_cards[previous_index]
                = config->legal_cards[previous_index - 1U];
            previous_index -= 1U;
        }
        config->legal_cards[previous_index] = value;
        index += 1U;
    }
    return ;
}

static void card_game_format_sort_bans(
    card_game_format_config *config) noexcept
{
    uint32_t index;
    uint32_t previous_index;
    card_game_format_ban_entry value;

    index = 1U;
    while (index < config->ban_count)
    {
        value = config->bans[index];
        previous_index = index;
        while (previous_index > 0U
            && config->bans[previous_index - 1U].card_id > value.card_id)
        {
            config->bans[previous_index] = config->bans[previous_index - 1U];
            previous_index -= 1U;
        }
        config->bans[previous_index] = value;
        index += 1U;
    }
    return ;
}

static void card_game_format_sort_exceptions(
    card_game_format_config *config) noexcept
{
    uint32_t index;
    uint32_t previous_index;
    card_game_format_exception value;

    index = 1U;
    while (index < config->exception_count)
    {
        value = config->exceptions[index];
        previous_index = index;
        while (previous_index > 0U
            && (config->exceptions[previous_index - 1U].card_id
                    > value.card_id
                || (config->exceptions[previous_index - 1U].card_id
                        == value.card_id
                    && config->exceptions[previous_index - 1U].rule_id
                        > value.rule_id)))
        {
            config->exceptions[previous_index]
                = config->exceptions[previous_index - 1U];
            previous_index -= 1U;
        }
        config->exceptions[previous_index] = value;
        index += 1U;
    }
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
    uint32_t inner_index;

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
            || config.legal_cards[index].copy_limit > config.global_copy_limit)
            return (FT_ERR_INVALID_ARGUMENT);
        inner_index = index + 1U;
        while (inner_index < config.legal_card_count)
        {
            if (config.legal_cards[index].card_id
                    == config.legal_cards[inner_index].card_id)
                return (FT_ERR_INVALID_ARGUMENT);
            inner_index += 1U;
        }
        index += 1U;
    }
    index = 0U;
    while (index < config.ban_count)
    {
        if (config.bans[index].card_id == 0U
            || config.bans[index].copy_limit > config.global_copy_limit)
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
    card_game_format_hash_u32(context, FT_CARD_GAME_FORMAT_SCHEMA_VERSION);
    card_game_format_hash_u32(context, config.format_id);
    card_game_format_hash_u32(context, config.revision);
    card_game_format_hash_u32(context, config.profile_id);
    card_game_format_hash_u32(context, config.corpus_version);
    card_game_format_hash_u32(context, config.global_copy_limit);
    card_game_format_hash_u32(context, config.minimum_main_cards);
    card_game_format_hash_u32(context, config.maximum_main_cards);
    card_game_format_hash_u32(context, config.minimum_extra_cards);
    card_game_format_hash_u32(context, config.maximum_extra_cards);
    card_game_format_hash_u32(context, config.minimum_side_cards);
    card_game_format_hash_u32(context, config.maximum_side_cards);
    index = 0U;
    while (index < config.legal_card_count)
    {
        card_game_format_hash_bytes(context, &config.legal_cards[index],
            sizeof(card_game_format_card_rule));
        index += 1U;
    }
    index = 0U;
    while (index < config.ban_count)
    {
        card_game_format_hash_bytes(context, &config.bans[index],
            sizeof(card_game_format_ban_entry));
        index += 1U;
    }
    index = 0U;
    while (index < config.exception_count)
    {
        card_game_format_hash_bytes(context, &config.exceptions[index],
            sizeof(card_game_format_exception));
        index += 1U;
    }
    result = context.final(hash->bytes);
    (void)context.destroy();
    return (result);
}

int32_t card_game_format::find_rule(uint32_t card_id,
    card_game_format_card_rule *rule) const noexcept
{
    uint32_t index;

    if (rule == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_config->legal_card_count)
    {
        if (this->_config->legal_cards[index].card_id == card_id)
        {
            *rule = this->_config->legal_cards[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
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
    this->_config = static_cast<card_game_format_config *>(cma_malloc(
        sizeof(card_game_format_config)));
    if (this->_config == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    *this->_config = config;
    card_game_format_sort_card_rules(this->_config);
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
    uint32_t index;
    int32_t result;

    if (copy_limit == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    result = this->find_rule(card_id, &rule);
    if (result != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    *copy_limit = rule.copy_limit;
    index = 0U;
    while (index < this->_config->ban_count)
    {
        if (this->_config->bans[index].card_id == card_id)
        {
            *copy_limit = this->_config->bans[index].copy_limit;
            if (*copy_limit == 0U)
                return (FT_ERR_PERMISSION_DENIED);
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    if (rule.policy == CARD_GAME_FORMAT_CARD_BANNED)
        return (FT_ERR_PERMISSION_DENIED);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_format::get_exception(uint32_t card_id, uint32_t rule_id,
    card_game_format_exception *exception) const noexcept
{
    uint32_t index;

    if (exception == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    index = 0U;
    while (index < this->_config->exception_count)
    {
        if (this->_config->exceptions[index].card_id == card_id
            && this->_config->exceptions[index].rule_id == rule_id)
        {
            *exception = this->_config->exceptions[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
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
                diagnostic->permitted_count = copy_limit;
                return (result);
            }
            if (deck.zones[zone_index].entries[entry_index].quantity
                > copy_limit)
            {
                diagnostic->error_code = FT_ERR_OUT_OF_RANGE;
                diagnostic->card_id = deck.zones[zone_index]
                    .entries[entry_index].definition_id;
                diagnostic->zone_id = deck.zones[zone_index].zone_id;
                diagnostic->observed_count = deck.zones[zone_index]
                    .entries[entry_index].quantity;
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
