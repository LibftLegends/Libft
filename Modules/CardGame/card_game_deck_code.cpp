#include "card_game_deck_code.hpp"
#include "../Encoding/encoding.hpp"
#include "../Crypto/crypto_primitives.hpp"
#include "../CMA/cma.hpp"

static int32_t deck_code_write_u32(uint8_t *output, uint32_t capacity,
    uint32_t *offset, uint32_t value) noexcept
{
    while (value >= 128U)
    {
        if (*offset >= capacity)
            return (FT_ERR_FULL);
        output[*offset] = static_cast<uint8_t>((value & 127U) | 128U);
        *offset += 1U;
        value >>= 7U;
    }
    if (*offset >= capacity)
        return (FT_ERR_FULL);
    output[*offset] = static_cast<uint8_t>(value);
    *offset += 1U;
    return (FT_ERR_SUCCESS);
}

static int32_t deck_code_read_u32(const uint8_t *input, uint32_t input_size,
    uint32_t *offset, uint32_t *value) noexcept
{
    uint32_t shift;
    uint32_t result;
    uint32_t index;
    uint8_t raw_byte;

    if (input == ft_nullptr || offset == ft_nullptr || value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = 0U;
    shift = 0U;
    index = 0U;
    while (index < 5U)
    {
        if (*offset >= input_size)
            return (FT_ERR_INVALID_ARGUMENT);
        raw_byte = input[*offset];
        *offset += 1U;
        if (index == 4U && (raw_byte & 240U) != 0U)
            return (FT_ERR_OUT_OF_RANGE);
        result |= (raw_byte & 127U) << shift;
        if ((raw_byte & 128U) == 0U)
        {
            if (index > 0U && raw_byte == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            *value = result;
            return (FT_ERR_SUCCESS);
        }
        shift += 7U;
        index += 1U;
    }
    return (FT_ERR_OUT_OF_RANGE);
}

static uint32_t deck_code_crc32c(const uint8_t *input,
    uint32_t input_size) noexcept
{
    uint32_t crc;
    uint32_t byte_index;
    uint32_t bit_index;

    crc = 0xFFFFFFFFU;
    byte_index = 0U;
    while (byte_index < input_size)
    {
        crc ^= input[byte_index];
        bit_index = 0U;
        while (bit_index < 8U)
        {
            if ((crc & 1U) != 0U)
                crc = (crc >> 1U) ^ 0x82F63B78U;
            else
                crc >>= 1U;
            bit_index += 1U;
        }
        byte_index += 1U;
    }
    return (crc ^ 0xFFFFFFFFU);
}

static void deck_code_sort_zone(card_game_deck_zone *zone) noexcept
{
    uint32_t index;
    uint32_t next_index;
    card_game_deck_entry value;

    index = 1U;
    while (index < zone->entry_count)
    {
        value = zone->entries[index];
        next_index = index;
        while (next_index > 0U
            && (zone->entries[next_index - 1U].definition_id > value.definition_id
                || (zone->entries[next_index - 1U].definition_id
                    == value.definition_id
                    && zone->entries[next_index - 1U].printing_id
                        > value.printing_id)))
        {
            zone->entries[next_index] = zone->entries[next_index - 1U];
            next_index -= 1U;
        }
        zone->entries[next_index] = value;
        index += 1U;
    }
}

static int32_t card_game_deck_build_binary(const card_game_deck &source,
    uint8_t *output, uint32_t *output_size, ft_bool include_checksum) noexcept
{
    card_game_deck canonical;
    uint32_t zone_index;
    uint32_t entry_index;
    uint32_t offset;
    uint32_t checksum;
    uint32_t total_cards;

    if (output == ft_nullptr || output_size == ft_nullptr
        || source.zone_count > FT_CARD_GAME_DECK_CODE_MAX_ZONES
        || (source.flags & ~CARD_GAME_DECK_FLAG_PRINTINGS) != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    canonical = source;
    total_cards = 0U;
    zone_index = 0U;
    while (zone_index < canonical.zone_count)
    {
        if (canonical.zones[zone_index].zone_id == 0U
            || canonical.zones[zone_index].entry_count
                > FT_CARD_GAME_DECK_CODE_MAX_ENTRIES)
            return (FT_ERR_INVALID_ARGUMENT);
        entry_index = 0U;
        while (entry_index < canonical.zones[zone_index].entry_count)
        {
            if (canonical.zones[zone_index].entries[entry_index].definition_id
                    == 0U
                || canonical.zones[zone_index].entries[entry_index].quantity
                    == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            if (canonical.zones[zone_index].entries[entry_index].quantity
                    > FT_CARD_GAME_DECK_CODE_MAX_TOTAL_CARDS
                || total_cards > FT_CARD_GAME_DECK_CODE_MAX_TOTAL_CARDS
                - canonical.zones[zone_index].entries[entry_index].quantity)
                return (FT_ERR_OUT_OF_RANGE);
            total_cards += canonical.zones[zone_index]
                .entries[entry_index].quantity;
            entry_index += 1U;
        }
        if (total_cards > FT_CARD_GAME_DECK_CODE_MAX_TOTAL_CARDS)
            return (FT_ERR_OUT_OF_RANGE);
        deck_code_sort_zone(&canonical.zones[zone_index]);
        entry_index = 1U;
        while (entry_index < canonical.zones[zone_index].entry_count)
        {
            if (canonical.zones[zone_index].entries[entry_index - 1U]
                    .definition_id
                == canonical.zones[zone_index].entries[entry_index]
                    .definition_id
                && ((canonical.flags & CARD_GAME_DECK_FLAG_PRINTINGS) == 0U
                    || canonical.zones[zone_index].entries[entry_index - 1U]
                        .printing_id
                    == canonical.zones[zone_index].entries[entry_index]
                        .printing_id))
                return (FT_ERR_INVALID_ARGUMENT);
            entry_index += 1U;
        }
        zone_index += 1U;
    }
    offset = 0U;
    if (deck_code_write_u32(output, FT_CARD_GAME_DECK_CODE_MAX_BYTES, &offset,
        FT_CARD_GAME_DECK_CODE_VERSION) != FT_ERR_SUCCESS
        || deck_code_write_u32(output, FT_CARD_GAME_DECK_CODE_MAX_BYTES,
            &offset, canonical.profile_id) != FT_ERR_SUCCESS
        || deck_code_write_u32(output, FT_CARD_GAME_DECK_CODE_MAX_BYTES,
            &offset, canonical.format_id) != FT_ERR_SUCCESS
        || deck_code_write_u32(output, FT_CARD_GAME_DECK_CODE_MAX_BYTES,
            &offset, canonical.corpus_version) != FT_ERR_SUCCESS
        || deck_code_write_u32(output, FT_CARD_GAME_DECK_CODE_MAX_BYTES,
            &offset, canonical.flags) != FT_ERR_SUCCESS
        || deck_code_write_u32(output, FT_CARD_GAME_DECK_CODE_MAX_BYTES,
            &offset, canonical.zone_count) != FT_ERR_SUCCESS)
        return (FT_ERR_FULL);
    zone_index = 0U;
    while (zone_index < canonical.zone_count)
    {
        if (deck_code_write_u32(output, FT_CARD_GAME_DECK_CODE_MAX_BYTES,
                &offset, canonical.zones[zone_index].zone_id)
                != FT_ERR_SUCCESS
            || deck_code_write_u32(output, FT_CARD_GAME_DECK_CODE_MAX_BYTES,
                &offset, canonical.zones[zone_index].entry_count)
                != FT_ERR_SUCCESS)
            return (FT_ERR_FULL);
        entry_index = 0U;
        while (entry_index < canonical.zones[zone_index].entry_count)
        {
            if (deck_code_write_u32(output, FT_CARD_GAME_DECK_CODE_MAX_BYTES,
                    &offset, canonical.zones[zone_index]
                        .entries[entry_index].definition_id)
                    != FT_ERR_SUCCESS)
                return (FT_ERR_FULL);
            if ((canonical.flags & CARD_GAME_DECK_FLAG_PRINTINGS) != 0U)
            {
                if (deck_code_write_u32(output,
                        FT_CARD_GAME_DECK_CODE_MAX_BYTES, &offset,
                        canonical.zones[zone_index]
                            .entries[entry_index].printing_id)
                        != FT_ERR_SUCCESS)
                    return (FT_ERR_FULL);
            }
            if (deck_code_write_u32(output, FT_CARD_GAME_DECK_CODE_MAX_BYTES,
                    &offset, canonical.zones[zone_index]
                        .entries[entry_index].quantity)
                    != FT_ERR_SUCCESS)
                return (FT_ERR_FULL);
            entry_index += 1U;
        }
        zone_index += 1U;
    }
    if (include_checksum != FT_FALSE)
    {
        checksum = deck_code_crc32c(output, offset);
        output[offset] = static_cast<uint8_t>(checksum & 255U);
        output[offset + 1U] = static_cast<uint8_t>((checksum >> 8U) & 255U);
        output[offset + 2U] = static_cast<uint8_t>((checksum >> 16U) & 255U);
        output[offset + 3U] = static_cast<uint8_t>((checksum >> 24U) & 255U);
        offset += 4U;
    }
    *output_size = offset;
    return (FT_ERR_SUCCESS);
}

static int32_t card_game_deck_parse_binary(const uint8_t *input,
    uint32_t input_size, card_game_deck *deck) noexcept
{
    card_game_deck candidate;
    uint32_t offset;
    uint32_t zone_index;
    uint32_t entry_index;
    uint32_t value;
    uint32_t expected_checksum;
    uint32_t actual_checksum;
    uint32_t total_cards;
    int32_t result;

    if (input == ft_nullptr || deck == ft_nullptr || input_size < 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    actual_checksum = static_cast<uint32_t>(input[input_size - 4U])
        | (static_cast<uint32_t>(input[input_size - 3U]) << 8U)
        | (static_cast<uint32_t>(input[input_size - 2U]) << 16U)
        | (static_cast<uint32_t>(input[input_size - 1U]) << 24U);
    expected_checksum = deck_code_crc32c(input, input_size - 4U);
    if (actual_checksum != expected_checksum)
        return (FT_ERR_INVALID_ARGUMENT);
    offset = 0U;
    result = deck_code_read_u32(input, input_size - 4U, &offset, &value);
    if (result != FT_ERR_SUCCESS || value != FT_CARD_GAME_DECK_CODE_VERSION)
        return (FT_ERR_INVALID_ARGUMENT);
    candidate.profile_id = 0U;
    candidate.format_id = 0U;
    candidate.corpus_version = 0U;
    candidate.flags = 0U;
    candidate.zone_count = 0U;
    total_cards = 0U;
    result = deck_code_read_u32(input, input_size - 4U, &offset,
        &candidate.profile_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = deck_code_read_u32(input, input_size - 4U, &offset,
        &candidate.format_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = deck_code_read_u32(input, input_size - 4U, &offset,
        &candidate.corpus_version);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = deck_code_read_u32(input, input_size - 4U, &offset,
        &candidate.flags);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = deck_code_read_u32(input, input_size - 4U, &offset,
        &candidate.zone_count);
    if (result != FT_ERR_SUCCESS
        || candidate.zone_count > FT_CARD_GAME_DECK_CODE_MAX_ZONES
        || (candidate.flags & ~CARD_GAME_DECK_FLAG_PRINTINGS) != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    zone_index = 0U;
    while (zone_index < candidate.zone_count)
    {
        result = deck_code_read_u32(input, input_size - 4U, &offset,
            &candidate.zones[zone_index].zone_id);
        if (result != FT_ERR_SUCCESS || candidate.zones[zone_index].zone_id == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        {
            uint32_t previous_zone;

            previous_zone = 0U;
            while (previous_zone < zone_index)
            {
                if (candidate.zones[previous_zone].zone_id
                    == candidate.zones[zone_index].zone_id)
                    return (FT_ERR_INVALID_ARGUMENT);
                previous_zone += 1U;
            }
        }
        result = deck_code_read_u32(input, input_size - 4U, &offset,
            &candidate.zones[zone_index].entry_count);
        if (result != FT_ERR_SUCCESS || candidate.zones[zone_index].entry_count
            > FT_CARD_GAME_DECK_CODE_MAX_ENTRIES)
            return (FT_ERR_INVALID_ARGUMENT);
        entry_index = 0U;
        while (entry_index < candidate.zones[zone_index].entry_count)
        {
            card_game_deck_entry *entry =
                &candidate.zones[zone_index].entries[entry_index];
            result = deck_code_read_u32(input, input_size - 4U, &offset,
                &entry->definition_id);
            if (result != FT_ERR_SUCCESS || entry->definition_id == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            entry->printing_id = 0U;
            if ((candidate.flags & CARD_GAME_DECK_FLAG_PRINTINGS) != 0U)
            {
                result = deck_code_read_u32(input, input_size - 4U, &offset,
                    &entry->printing_id);
                if (result != FT_ERR_SUCCESS)
                    return (result);
            }
            result = deck_code_read_u32(input, input_size - 4U, &offset,
                &entry->quantity);
            if (result != FT_ERR_SUCCESS || entry->quantity == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            if (entry->quantity > FT_CARD_GAME_DECK_CODE_MAX_TOTAL_CARDS
                || total_cards > FT_CARD_GAME_DECK_CODE_MAX_TOTAL_CARDS
                - entry->quantity)
                return (FT_ERR_OUT_OF_RANGE);
            total_cards += entry->quantity;
            if (entry_index != 0U
                && (candidate.zones[zone_index]
                        .entries[entry_index - 1U].definition_id
                    > entry->definition_id
                    || (candidate.zones[zone_index]
                            .entries[entry_index - 1U].definition_id
                        == entry->definition_id
                        && candidate.zones[zone_index]
                            .entries[entry_index - 1U].printing_id
                            >= entry->printing_id)))
                return (FT_ERR_INVALID_ARGUMENT);
            entry_index += 1U;
        }
        zone_index += 1U;
    }
    if (offset != input_size - 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    *deck = candidate;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_deck_encode(const card_game_deck &deck,
    ft_string *output) noexcept
{
    uint8_t binary[FT_CARD_GAME_DECK_CODE_MAX_BYTES];
    uint32_t binary_size;
    char *encoded;
    ft_string temporary;
    int32_t result;

    if (output == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = card_game_deck_build_binary(deck, binary, &binary_size, FT_TRUE);
    if (result != FT_ERR_SUCCESS)
        return (result);
    encoded = encoding_base64url_encode(binary, binary_size, FT_FALSE);
    if (encoded == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    result = temporary.initialize(encoded);
    cma_free(encoded);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (output->move(temporary));
}

int32_t card_game_deck_decode(const ft_string &input,
    card_game_deck *deck) noexcept
{
    uint8_t *binary;
    ft_size_t binary_size;
    card_game_deck candidate;
    int32_t result;

    if (deck == ft_nullptr || input.is_initialised() == FT_FALSE
        || input.size() > FT_CARD_GAME_DECK_CODE_MAX_TEXT_BYTES)
        return (FT_ERR_INVALID_ARGUMENT);
    binary_size = 0U;
    binary = encoding_base64url_decode(input.c_str(), input.size(),
        &binary_size);
    if (binary == ft_nullptr || binary_size > FT_CARD_GAME_DECK_CODE_MAX_BYTES)
    {
        if (binary != ft_nullptr)
            cma_free(binary);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    result = card_game_deck_parse_binary(binary,
        static_cast<uint32_t>(binary_size), &candidate);
    cma_free(binary);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *deck = candidate;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_deck_hash(const card_game_deck &deck,
    card_game_deck_hash_value *hash) noexcept
{
    uint8_t binary[FT_CARD_GAME_DECK_CODE_MAX_BYTES];
    uint32_t binary_size;
    int32_t result;

    if (hash == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = card_game_deck_build_binary(deck, binary, &binary_size, FT_FALSE);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (crypto_sha256_hash(binary, binary_size, hash->bytes));
}
