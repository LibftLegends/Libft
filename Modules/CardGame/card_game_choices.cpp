#include "card_game_choices.hpp"
#include "../CMA/CMA.hpp"

static void card_game_choices_write_u32(uint8_t *output, uint32_t *offset,
    uint32_t value) noexcept
{
    output[*offset] = static_cast<uint8_t>(value & 255U);
    output[*offset + 1U] = static_cast<uint8_t>((value >> 8U) & 255U);
    output[*offset + 2U] = static_cast<uint8_t>((value >> 16U) & 255U);
    output[*offset + 3U] = static_cast<uint8_t>((value >> 24U) & 255U);
    *offset += 4U;
    return ;
}

static void card_game_choices_write_u64(uint8_t *output, uint32_t *offset,
    uint64_t value) noexcept
{
    uint32_t index;

    index = 0U;
    while (index < 8U)
    {
        output[*offset + index] = static_cast<uint8_t>(value & 255U);
        value >>= 8U;
        index += 1U;
    }
    *offset += 8U;
    return ;
}

static int32_t card_game_choices_read_u32(const uint8_t *input,
    uint32_t input_size, uint32_t *offset, uint32_t *value) noexcept
{
    if (input == ft_nullptr || offset == ft_nullptr || value == ft_nullptr
        || *offset > input_size || input_size - *offset < 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    *value = static_cast<uint32_t>(input[*offset])
        | (static_cast<uint32_t>(input[*offset + 1U]) << 8U)
        | (static_cast<uint32_t>(input[*offset + 2U]) << 16U)
        | (static_cast<uint32_t>(input[*offset + 3U]) << 24U);
    *offset += 4U;
    return (FT_ERR_SUCCESS);
}

static int32_t card_game_choices_read_u64(const uint8_t *input,
    uint32_t input_size, uint32_t *offset, uint64_t *value) noexcept
{
    uint32_t index;
    uint64_t result;

    if (input == ft_nullptr || offset == ft_nullptr || value == ft_nullptr
        || *offset > input_size || input_size - *offset < 8U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = 0U;
    index = 0U;
    while (index < 8U)
    {
        result |= static_cast<uint64_t>(input[*offset + index])
            << (index * 8U);
        index += 1U;
    }
    *offset += 8U;
    *value = result;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::release_snapshot(
    card_game_choice_snapshot *snapshot) noexcept
{
    if (snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot->choices != ft_nullptr)
        cma_free(snapshot->choices);
    ft_bzero(snapshot, sizeof(*snapshot));
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::get_snapshot(
    card_game_choice_snapshot *snapshot) const noexcept
{
    card_game_choice *choices;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (card_game_choice_ledger::release_snapshot(snapshot)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    choices = ft_nullptr;
    if (this->_count != 0U)
    {
        choices = static_cast<card_game_choice *>(cma_malloc(
            static_cast<ft_size_t>(this->_count) * sizeof(card_game_choice)));
        if (choices == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ft_memcpy(choices, this->_choices,
            static_cast<ft_size_t>(this->_count) * sizeof(card_game_choice));
    }
    snapshot->count = this->_count;
    snapshot->next_id = this->_next_id;
    snapshot->choices = choices;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::clone_snapshot(
    const card_game_choice_snapshot &source,
    card_game_choice_snapshot *destination) noexcept
{
    if (destination == ft_nullptr || source.count > FT_CARD_GAME_MAX_CHOICES
        || source.next_id == 0U
        || (source.count != 0U && source.choices == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (card_game_choice_ledger::release_snapshot(destination)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    destination->count = source.count;
    destination->next_id = source.next_id;
    if (source.count != 0U)
    {
        destination->choices = static_cast<card_game_choice *>(cma_malloc(
            static_cast<ft_size_t>(source.count) * sizeof(card_game_choice)));
        if (destination->choices == ft_nullptr)
        {
            (void)card_game_choice_ledger::release_snapshot(destination);
            return (FT_ERR_NO_MEMORY);
        }
        ft_memcpy(destination->choices, source.choices,
            static_cast<ft_size_t>(source.count) * sizeof(card_game_choice));
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::apply_snapshot(
    const card_game_choice_snapshot &snapshot) noexcept
{
    uint32_t index;
    uint32_t compare_index;
    uint32_t option_index;
    uint32_t option_compare_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot.count > FT_CARD_GAME_MAX_CHOICES
        || snapshot.next_id == 0U
        || (snapshot.count != 0U && snapshot.choices == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < snapshot.count)
    {
        if (snapshot.choices[index].choice_id == 0U
            || snapshot.choices[index].kind < CARD_GAME_CHOICE_TARGET
            || snapshot.choices[index].kind > CARD_GAME_CHOICE_MULLIGAN
            || snapshot.choices[index].option_count
                > FT_CARD_GAME_MAX_CHOICE_OPTIONS
            || snapshot.choices[index].resolved > FT_TRUE)
            return (FT_ERR_INVALID_ARGUMENT);
        option_index = 0U;
        while (option_index < snapshot.choices[index].option_count)
        {
            if (snapshot.choices[index].options[option_index].option_id == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            option_compare_index = option_index + 1U;
            while (option_compare_index
                < snapshot.choices[index].option_count)
            {
                if (snapshot.choices[index].options[option_index].option_id
                    == snapshot.choices[index]
                        .options[option_compare_index].option_id)
                    return (FT_ERR_INVALID_ARGUMENT);
                option_compare_index += 1U;
            }
            option_index += 1U;
        }
        if (snapshot.choices[index].resolved != FT_FALSE
            && snapshot.choices[index].selected_option_id == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        if (snapshot.choices[index].selected_option_id != 0U
            && this->find_option(snapshot.choices[index],
                snapshot.choices[index].selected_option_id) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        compare_index = index + 1U;
        while (compare_index < snapshot.count)
        {
            if (snapshot.choices[index].choice_id
                == snapshot.choices[compare_index].choice_id)
                return (FT_ERR_INVALID_ARGUMENT);
            compare_index += 1U;
        }
        index += 1U;
    }
    if (snapshot.count != 0U)
        ft_memcpy(this->_choices, snapshot.choices,
            static_cast<ft_size_t>(snapshot.count) * sizeof(card_game_choice));
    this->_count = snapshot.count;
    this->_next_id = snapshot.next_id;
    return (FT_ERR_SUCCESS);
}

ft_bool card_game_choice_ledger::snapshots_equal(
    const card_game_choice_snapshot &first,
    const card_game_choice_snapshot &second) noexcept
{
    if (first.count != second.count || first.next_id != second.next_id
        || (first.count != 0U
            && (first.choices == ft_nullptr || second.choices == ft_nullptr)))
        return (FT_FALSE);
    if (first.count != 0U
        && ft_memcmp(first.choices, second.choices,
            static_cast<ft_size_t>(first.count) * sizeof(card_game_choice)) != 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

card_game_choice_ledger::card_game_choice_ledger() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _count(0U),
      _next_id(1U), _choices()
{
    return ;
}

card_game_choice_ledger::~card_game_choice_ledger() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_choice_ledger::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_count = 0U;
    this->_next_id = 1U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::destroy() noexcept
{
    this->_count = 0U;
    this->_next_id = 1U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::move(card_game_choice_ledger &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_count = other._count;
    this->_next_id = other._next_id;
    ft_memcpy(this->_choices, other._choices, sizeof(this->_choices));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::find(uint32_t choice_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || choice_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_count)
    {
        if (this->_choices[current_index].choice_id == choice_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_choice_ledger::find_option(
    const card_game_choice &choice, uint32_t option_id) const noexcept
{
    uint32_t index;

    if (option_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < choice.option_count)
    {
        if (choice.options[index].option_id == option_id)
            return (FT_ERR_SUCCESS);
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_choice_ledger::open(uint32_t player_id,
    card_game_choice_kind kind, uint64_t deadline_epoch,
    uint32_t default_option_id, uint32_t *choice_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || choice_id == ft_nullptr
        || kind < CARD_GAME_CHOICE_TARGET || kind > CARD_GAME_CHOICE_MULLIGAN)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_count >= FT_CARD_GAME_MAX_CHOICES)
        return (FT_ERR_FULL);
    if (this->_next_id == 0U || this->_next_id == UINT32_MAX)
        return (FT_ERR_FULL);
    this->_choices[this->_count].choice_id = this->_next_id;
    this->_choices[this->_count].player_id = player_id;
    this->_choices[this->_count].kind = kind;
    this->_choices[this->_count].option_count = 0U;
    this->_choices[this->_count].deadline_epoch = deadline_epoch;
    this->_choices[this->_count].default_option_id = default_option_id;
    this->_choices[this->_count].selected_option_id = 0U;
    this->_choices[this->_count].resolved = FT_FALSE;
    *choice_id = this->_next_id;
    this->_next_id += 1U;
    this->_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::add_option(uint32_t choice_id,
    const card_game_choice_option &option) noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || option.option_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find(choice_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->_choices[index].resolved
        || this->_choices[index].option_count >= FT_CARD_GAME_MAX_CHOICE_OPTIONS)
        return (FT_ERR_FULL);
    if (this->find_option(this->_choices[index], option.option_id)
        == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    this->_choices[index].options[this->_choices[index].option_count] = option;
    this->_choices[index].option_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::choose(uint32_t choice_id, uint32_t player_id,
    uint32_t option_id, uint64_t epoch) noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    result = this->find(choice_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->_choices[index].player_id != player_id
        || this->_choices[index].resolved)
        return (FT_ERR_PERMISSION_DENIED);
    if (this->_choices[index].deadline_epoch != 0U
        && epoch >= this->_choices[index].deadline_epoch)
        return (FT_ERR_TIMEOUT);
    if (this->find_option(this->_choices[index], option_id) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_choices[index].selected_option_id = option_id;
    this->_choices[index].resolved = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::resolve_expired(uint64_t epoch) noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    index = 0U;
    while (index < this->_count)
    {
        if (this->_choices[index].resolved == FT_FALSE
            && this->_choices[index].deadline_epoch != 0U
            && epoch >= this->_choices[index].deadline_epoch)
        {
            if (this->_choices[index].default_option_id == 0U
                || this->find_option(this->_choices[index],
                    this->_choices[index].default_option_id) != FT_ERR_SUCCESS)
                return (FT_ERR_TIMEOUT);
        }
        index += 1U;
    }
    index = 0U;
    while (index < this->_count)
    {
        if (this->_choices[index].resolved == FT_FALSE
            && this->_choices[index].deadline_epoch != 0U
            && epoch >= this->_choices[index].deadline_epoch)
        {
            this->_choices[index].selected_option_id =
                this->_choices[index].default_option_id;
            this->_choices[index].resolved = FT_TRUE;
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::get(uint32_t choice_id,
    card_game_choice *choice) const noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || choice == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find(choice_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *choice = this->_choices[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::serialize(uint8_t *output,
    uint32_t output_capacity, uint32_t *output_size) const noexcept
{
    uint32_t required_size;
    uint32_t choice_index;
    uint32_t option_index;
    uint32_t offset;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || output == ft_nullptr || output_size == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    required_size = 12U + this->_count * FT_CARD_GAME_CHOICE_RECORD_BYTES;
    if (output_capacity < required_size)
        return (FT_ERR_OUT_OF_RANGE);
    offset = 0U;
    card_game_choices_write_u32(output, &offset,
        FT_CARD_GAME_CHOICES_SERIAL_MAGIC);
    card_game_choices_write_u32(output, &offset,
        FT_CARD_GAME_CHOICES_SERIAL_VERSION);
    card_game_choices_write_u32(output, &offset, this->_count);
    choice_index = 0U;
    while (choice_index < this->_count)
    {
        const card_game_choice &choice = this->_choices[choice_index];

        ft_bzero(output + offset, FT_CARD_GAME_CHOICE_RECORD_BYTES);
        card_game_choices_write_u32(output, &offset, choice.choice_id);
        card_game_choices_write_u32(output, &offset, choice.player_id);
        card_game_choices_write_u32(output, &offset,
            static_cast<uint32_t>(choice.kind));
        card_game_choices_write_u32(output, &offset, choice.option_count);
        card_game_choices_write_u64(output, &offset, choice.deadline_epoch);
        card_game_choices_write_u32(output, &offset,
            choice.default_option_id);
        card_game_choices_write_u32(output, &offset,
            choice.selected_option_id);
        card_game_choices_write_u32(output, &offset,
            static_cast<uint32_t>(choice.resolved));
        option_index = 0U;
        while (option_index < choice.option_count)
        {
            card_game_choices_write_u32(output, &offset,
                choice.options[option_index].option_id);
            card_game_choices_write_u32(output, &offset,
                choice.options[option_index].value_a);
            card_game_choices_write_u32(output, &offset,
                choice.options[option_index].value_b);
            option_index += 1U;
        }
        offset += (FT_CARD_GAME_MAX_CHOICE_OPTIONS - choice.option_count)
            * 12U;
        choice_index += 1U;
    }
    *output_size = offset;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::deserialize(const uint8_t *input,
    uint32_t input_size) noexcept
{
    card_game_choice_ledger candidate;
    card_game_choice choice;
    uint32_t offset;
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t choice_index;
    uint32_t option_index;
    uint32_t value;
    int32_t result;

    if (input == ft_nullptr || input_size < 12U)
        return (FT_ERR_INVALID_ARGUMENT);
    offset = 0U;
    result = card_game_choices_read_u32(input, input_size, &offset, &magic);
    if (result != FT_ERR_SUCCESS || magic != FT_CARD_GAME_CHOICES_SERIAL_MAGIC)
        return (FT_ERR_INVALID_ARGUMENT);
    result = card_game_choices_read_u32(input, input_size, &offset, &version);
    if (result != FT_ERR_SUCCESS
        || version != FT_CARD_GAME_CHOICES_SERIAL_VERSION)
        return (FT_ERR_INVALID_ARGUMENT);
    result = card_game_choices_read_u32(input, input_size, &offset, &count);
    if (result != FT_ERR_SUCCESS || count > FT_CARD_GAME_MAX_CHOICES
        || input_size != 12U + count * FT_CARD_GAME_CHOICE_RECORD_BYTES)
        return (FT_ERR_INVALID_ARGUMENT);
    result = candidate.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    choice_index = 0U;
    while (choice_index < count)
    {
        ft_bzero(&choice, sizeof(choice));
        result = card_game_choices_read_u32(input, input_size, &offset,
            &choice.choice_id);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = card_game_choices_read_u32(input, input_size, &offset,
            &choice.player_id);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = card_game_choices_read_u32(input, input_size, &offset,
            &value);
        if (result != FT_ERR_SUCCESS || value < CARD_GAME_CHOICE_TARGET
            || value > CARD_GAME_CHOICE_MULLIGAN)
            return (FT_ERR_INVALID_ARGUMENT);
        choice.kind = static_cast<card_game_choice_kind>(value);
        result = card_game_choices_read_u32(input, input_size, &offset,
            &choice.option_count);
        if (result != FT_ERR_SUCCESS
            || choice.option_count > FT_CARD_GAME_MAX_CHOICE_OPTIONS)
            return (FT_ERR_INVALID_ARGUMENT);
        result = card_game_choices_read_u64(input, input_size, &offset,
            &choice.deadline_epoch);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = card_game_choices_read_u32(input, input_size, &offset,
            &choice.default_option_id);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = card_game_choices_read_u32(input, input_size, &offset,
            &choice.selected_option_id);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = card_game_choices_read_u32(input, input_size, &offset,
            &value);
        if (result != FT_ERR_SUCCESS || (value != FT_FALSE && value != FT_TRUE))
            return (FT_ERR_INVALID_ARGUMENT);
        choice.resolved = static_cast<ft_bool>(value);
        option_index = 0U;
        while (option_index < FT_CARD_GAME_MAX_CHOICE_OPTIONS)
        {
            result = card_game_choices_read_u32(input, input_size, &offset,
                &choice.options[option_index].option_id);
            if (result != FT_ERR_SUCCESS)
                return (result);
            result = card_game_choices_read_u32(input, input_size, &offset,
                &choice.options[option_index].value_a);
            if (result != FT_ERR_SUCCESS)
                return (result);
            result = card_game_choices_read_u32(input, input_size, &offset,
                &choice.options[option_index].value_b);
            if (result != FT_ERR_SUCCESS)
                return (result);
            option_index += 1U;
        }
        if (choice.choice_id == 0U || (choice.resolved == FT_FALSE
            && choice.selected_option_id != 0U)
            || (choice.resolved != FT_FALSE
                && choice.selected_option_id == 0U))
            return (FT_ERR_INVALID_ARGUMENT);
        option_index = 0U;
        while (option_index < choice.option_count)
        {
            if (choice.options[option_index].option_id == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            option_index += 1U;
        }
        if (choice.default_option_id != 0U
            && candidate.find_option(choice, choice.default_option_id)
                != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        if (choice.selected_option_id != 0U
            && candidate.find_option(choice, choice.selected_option_id)
                != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        option_index = 0U;
        while (option_index < choice.option_count)
        {
            uint32_t duplicate_index;

            duplicate_index = 0U;
            while (duplicate_index < option_index)
            {
                if (choice.options[duplicate_index].option_id
                    == choice.options[option_index].option_id)
                    return (FT_ERR_INVALID_ARGUMENT);
                duplicate_index += 1U;
            }
            option_index += 1U;
        }
        candidate._choices[choice_index] = choice;
        if (choice.choice_id >= candidate._next_id)
        {
            if (choice.choice_id == UINT32_MAX)
                candidate._next_id = UINT32_MAX;
            else
                candidate._next_id = choice.choice_id + 1U;
        }
        choice_index += 1U;
    }
    candidate._count = count;
    choice_index = 0U;
    while (choice_index < count)
    {
        option_index = 0U;
        while (option_index < choice_index)
        {
            if (candidate._choices[option_index].choice_id
                == candidate._choices[choice_index].choice_id)
                return (FT_ERR_INVALID_ARGUMENT);
            option_index += 1U;
        }
        choice_index += 1U;
    }
    this->_count = candidate._count;
    this->_next_id = candidate._next_id;
    ft_memcpy(this->_choices, candidate._choices, sizeof(this->_choices));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)candidate.destroy();
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_choice_ledger::size() const noexcept
{
    return (this->_count);
}
