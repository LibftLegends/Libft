#include "game_recipe_book.hpp"

#define GAME_RECIPE_BOOK_MAGIC 0x47425242U
#define GAME_RECIPE_BOOK_HEADER_SIZE 12U
#define GAME_RECIPE_BOOK_ENTRY_SIZE 12U
#define GAME_RECIPE_BOOK_CHECKSUM_SIZE 4U

static uint32_t game_recipe_book_checksum(const uint8_t *data,
    ft_size_t size) noexcept
{
    ft_size_t index;
    uint32_t checksum;

    checksum = 2166136261U;
    index = 0U;
    while (index < size)
    {
        checksum ^= static_cast<uint32_t>(data[index]);
        checksum *= 16777619U;
        index += 1U;
    }
    return (checksum);
}

game_recipe_book::game_recipe_book() noexcept
    : _catalogue(0U), _unlock_events(0U),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

game_recipe_book::~game_recipe_book() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_recipe_book::initialize(
    const ft_vector<uint32_t> &recipe_ids) noexcept
{
    return (this->initialize(recipe_ids, ft_nullptr, 0U));
}

int32_t game_recipe_book::initialize(
    const ft_vector<uint32_t> &recipe_ids,
    const game_recipe_unlock_record *initial_unlocks,
    uint32_t initial_unlock_count) noexcept
{
    ft_size_t index;
    uint32_t unlock_index;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    if (recipe_ids.is_initialised() != FT_CLASS_STATE_INITIALISED
        || (initial_unlock_count != 0U && initial_unlocks == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->_catalogue.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->_unlock_events.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_catalogue.destroy();
        (void)this->_unlock_events.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    index = 0U;
    while (index < recipe_ids.size())
    {
        if (recipe_ids[index] == 0U
            || this->_catalogue.find(recipe_ids[index])
                != this->_catalogue.end())
        {
            error_code = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        this->_catalogue.insert(recipe_ids[index], 1U);
        error_code = this->_catalogue.get_error();
        if (error_code != FT_ERR_SUCCESS)
            break ;
        index += 1U;
    }
    unlock_index = 0U;
    while (error_code == FT_ERR_SUCCESS
        && unlock_index < initial_unlock_count)
    {
        const game_recipe_unlock_record &unlock = initial_unlocks[unlock_index];
        uint32_t prior_index;

        if (unlock.recipe_id == 0U || unlock.source_event_id == 0U
            || this->_catalogue.find(unlock.recipe_id)
                == this->_catalogue.end())
        {
            error_code = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        prior_index = 0U;
        while (prior_index < unlock_index)
        {
            if (initial_unlocks[prior_index].source_event_id
                == unlock.source_event_id)
            {
                error_code = FT_ERR_INVALID_ARGUMENT;
                break ;
            }
            prior_index += 1U;
        }
        if (error_code == FT_ERR_SUCCESS)
            error_code = this->_unlock_events.push_back(unlock);
        unlock_index += 1U;
    }
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_catalogue.destroy();
        (void)this->_unlock_events.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_book::destroy() noexcept
{
    int32_t first_error;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    first_error = FT_ERR_SUCCESS;
    error_code = this->_catalogue.destroy();
    if (error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    error_code = this->_unlock_events.destroy();
    if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

int32_t game_recipe_book::unlock_recipe_with_result(uint32_t recipe_id,
    uint64_t source_event_id, game_recipe_unlock_result *result_out) noexcept
{
    ft_size_t index;
    ft_bool was_unlocked;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || result_out == ft_nullptr || recipe_id == 0U
        || source_event_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result_out->event_newly_recorded = FT_FALSE;
    result_out->recipe_newly_known = FT_FALSE;
    if (this->_catalogue.find(recipe_id) == this->_catalogue.end())
        return (FT_ERR_NOT_FOUND);
    index = 0U;
    was_unlocked = FT_FALSE;
    while (index < this->_unlock_events.size())
    {
        const game_recipe_unlock_record &record = this->_unlock_events[index];

        if (record.source_event_id == source_event_id)
        {
            if (record.recipe_id != recipe_id)
                return (FT_ERR_INVALID_STATE);
            return (FT_ERR_SUCCESS);
        }
        if (record.recipe_id == recipe_id)
            was_unlocked = FT_TRUE;
        index += 1U;
    }
    game_recipe_unlock_record record;

    record.source_event_id = source_event_id;
    record.recipe_id = recipe_id;
    error_code = this->_unlock_events.push_back(record);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    result_out->event_newly_recorded = FT_TRUE;
    if (was_unlocked == FT_FALSE)
        result_out->recipe_newly_known = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_book::unlock_recipe(uint32_t recipe_id,
    uint64_t source_event_id, ft_bool *newly_unlocked) noexcept
{
    game_recipe_unlock_result unlock_result = {};
    int32_t error_code;

    if (newly_unlocked == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *newly_unlocked = FT_FALSE;
    error_code = this->unlock_recipe_with_result(recipe_id, source_event_id,
        &unlock_result);
    if (error_code == FT_ERR_SUCCESS)
        *newly_unlocked = unlock_result.recipe_newly_known;
    return (error_code);
}

int32_t game_recipe_book::is_recipe_known(uint32_t recipe_id,
    ft_bool *known) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || known == ft_nullptr || recipe_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    *known = FT_FALSE;
    if (this->_catalogue.find(recipe_id) == this->_catalogue.end())
        return (FT_ERR_NOT_FOUND);
    ft_size_t index;

    index = 0U;
    while (index < this->_unlock_events.size())
    {
        if (this->_unlock_events[index].recipe_id == recipe_id)
        {
            *known = FT_TRUE;
            break ;
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_book::quarantined_unlock_count(uint32_t *count) const noexcept
{
    ft_size_t index;
    uint32_t quarantined_count;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    quarantined_count = 0U;
    index = 0U;
    while (index < this->_unlock_events.size())
    {
        if (this->_catalogue.find(this->_unlock_events[index].recipe_id)
            == this->_catalogue.end())
            quarantined_count += 1U;
        index += 1U;
    }
    *count = quarantined_count;
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_book::export_book(ft_byte_buffer &output) const noexcept
{
    ft_size_t index;
    uint32_t checksum;
    int32_t error_code;
    ft_byte_buffer staged_output;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (output.is_initialised() != FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = staged_output.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = staged_output.append_u32_le(GAME_RECIPE_BOOK_MAGIC);
    if (error_code == FT_ERR_SUCCESS)
        error_code = staged_output.append_u32_le(
            GAME_RECIPE_BOOK_FORMAT_VERSION);
    if (error_code == FT_ERR_SUCCESS)
        if (this->_unlock_events.size() > UINT32_MAX)
            return (FT_ERR_FULL);
    if (error_code == FT_ERR_SUCCESS)
        error_code = staged_output.append_u32_le(static_cast<uint32_t>(
            this->_unlock_events.size()));
    index = 0U;
    while (error_code == FT_ERR_SUCCESS
        && index < this->_unlock_events.size())
    {
        error_code = staged_output.append_u64_le(
            this->_unlock_events[index].source_event_id);
        if (error_code == FT_ERR_SUCCESS)
            error_code = staged_output.append_u32_le(
                this->_unlock_events[index].recipe_id);
        index += 1U;
    }
    if (error_code == FT_ERR_SUCCESS)
        checksum = game_recipe_book_checksum(staged_output.data(),
            staged_output.size());
    if (error_code == FT_ERR_SUCCESS)
        error_code = staged_output.append_u32_le(checksum);
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append_buffer(staged_output);
    (void)staged_output.destroy();
    return (error_code);
}

int32_t game_recipe_book::import_book(ft_byte_buffer &input) noexcept
{
    ft_vector<game_recipe_unlock_record> staged_events;
    const uint8_t *payload_data;
    ft_size_t original_position;
    ft_size_t expected_size;
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t expected_checksum;
    uint32_t actual_checksum;
    uint32_t recipe_id;
    uint64_t source_event_id;
    uint32_t index;
    ft_size_t prior_index;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (input.is_initialised() != FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    original_position = input.read_position();
    error_code = input.read_u32_le(&magic);
    if (error_code == FT_ERR_SUCCESS)
        error_code = input.read_u32_le(&version);
    if (error_code == FT_ERR_SUCCESS)
        error_code = input.read_u32_le(&entry_count);
    if (error_code != FT_ERR_SUCCESS || magic != GAME_RECIPE_BOOK_MAGIC
        || version != GAME_RECIPE_BOOK_FORMAT_VERSION)
    {
        (void)input.set_read_position(original_position);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (input.size() - original_position
        < GAME_RECIPE_BOOK_HEADER_SIZE + GAME_RECIPE_BOOK_CHECKSUM_SIZE)
    {
        (void)input.set_read_position(original_position);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (static_cast<ft_size_t>(entry_count)
        > (input.size() - original_position
            - GAME_RECIPE_BOOK_HEADER_SIZE
            - GAME_RECIPE_BOOK_CHECKSUM_SIZE) / GAME_RECIPE_BOOK_ENTRY_SIZE)
    {
        (void)input.set_read_position(original_position);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    expected_size = GAME_RECIPE_BOOK_HEADER_SIZE
        + static_cast<ft_size_t>(entry_count) * GAME_RECIPE_BOOK_ENTRY_SIZE
        + GAME_RECIPE_BOOK_CHECKSUM_SIZE;
    if (expected_size > input.size() - original_position)
    {
        (void)input.set_read_position(original_position);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (expected_size != input.size() - original_position)
    {
        (void)input.set_read_position(original_position);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    error_code = input.view(original_position,
        expected_size - GAME_RECIPE_BOOK_CHECKSUM_SIZE, &payload_data);
    if (error_code == FT_ERR_SUCCESS)
        actual_checksum = game_recipe_book_checksum(payload_data,
            expected_size - GAME_RECIPE_BOOK_CHECKSUM_SIZE);
    if (error_code == FT_ERR_SUCCESS)
        error_code = input.set_read_position(original_position
            + expected_size - GAME_RECIPE_BOOK_CHECKSUM_SIZE);
    if (error_code == FT_ERR_SUCCESS)
        error_code = input.read_u32_le(&expected_checksum);
    if (error_code != FT_ERR_SUCCESS || expected_checksum != actual_checksum)
    {
        (void)input.set_read_position(original_position);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    error_code = staged_events.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)input.set_read_position(original_position);
        return (error_code);
    }
    error_code = input.set_read_position(original_position
        + GAME_RECIPE_BOOK_HEADER_SIZE);
    index = 0U;
    while (error_code == FT_ERR_SUCCESS && index < entry_count)
    {
        error_code = input.read_u64_le(&source_event_id);
        if (error_code == FT_ERR_SUCCESS)
            error_code = input.read_u32_le(&recipe_id);
        if (error_code != FT_ERR_SUCCESS)
            break ;
        if (source_event_id == 0U || recipe_id == 0U)
        {
            error_code = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        prior_index = 0U;
        while (prior_index < staged_events.size())
        {
            if (staged_events[prior_index].source_event_id == source_event_id)
            {
                error_code = FT_ERR_INVALID_ARGUMENT;
                break ;
            }
            prior_index += 1U;
        }
        if (error_code != FT_ERR_SUCCESS)
            break ;
        game_recipe_unlock_record record;

        record.source_event_id = source_event_id;
        record.recipe_id = recipe_id;
        error_code = staged_events.push_back(record);
        index += 1U;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = input.set_read_position(original_position + expected_size);
    if (error_code == FT_ERR_SUCCESS)
    {
        this->_unlock_events.reserve(staged_events.size());
        error_code = this->_unlock_events.get_error();
        if (error_code == FT_ERR_SUCCESS)
        {
            this->_unlock_events.clear();
            prior_index = 0U;
            while (prior_index < staged_events.size())
            {
                error_code = this->_unlock_events.push_back(
                    staged_events[prior_index]);
                if (error_code != FT_ERR_SUCCESS)
                    break ;
                prior_index += 1U;
            }
        }
    }
    (void)staged_events.destroy();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)input.set_read_position(original_position);
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}
