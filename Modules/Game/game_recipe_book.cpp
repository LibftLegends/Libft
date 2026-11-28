#include "game_recipe_book.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"
#include "../Buffer/buffer_endian.hpp"
#include <algorithm>
#include <type_traits>

static constexpr uint32_t GAME_RECIPE_BOOK_MAGIC = 0x47425242U;
static constexpr ft_size_t GAME_RECIPE_BOOK_HEADER_SIZE = 3U * sizeof(uint32_t);
static constexpr ft_size_t GAME_RECIPE_BOOK_ENTRY_SIZE = sizeof(uint64_t) + sizeof(uint32_t);
static constexpr ft_size_t GAME_RECIPE_BOOK_CHECKSUM_SIZE = sizeof(uint32_t);

static constexpr uint64_t RECIPE_BOOK_REVISION_SEED = 14695981039346656037ULL;
static constexpr uint64_t RECIPE_BOOK_REVISION_PRIME = 1099511628211ULL;

static int32_t game_recipe_book_build_event_index(
    const ft_vector<game_recipe_unlock_record> &events,
    ft_vector<game_recipe_unlock_record> &event_index) noexcept
{
    const ft_size_t count = events.size();
    const int32_t error_code = event_index.append(events.begin(), count);
    if (error_code != FT_ERR_SUCCESS || count < 2U)
        return (error_code);
    auto *begin = event_index.begin();
    auto *end = begin + count;
    const auto less = [](const game_recipe_unlock_record &left,
        const game_recipe_unlock_record &right) noexcept
    {
        return (left.source_event_id < right.source_event_id);
    };
    if (!std::is_sorted(begin, end, less))
        std::sort(begin, end, less);
    if (std::adjacent_find(begin, end, [](const game_recipe_unlock_record &left,
            const game_recipe_unlock_record &right) noexcept
        {
            return (left.source_event_id == right.source_event_id);
        }) != end)
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

static uint64_t game_recipe_book_extend_revision(uint64_t revision,
    const game_recipe_unlock_record &record) noexcept
{
    uint64_t event_id = record.source_event_id;
    uint32_t recipe_id = record.recipe_id;
    uint32_t index = 0U;
    while (index < 8U)
    {
        revision = (revision ^ static_cast<uint8_t>(event_id)) * RECIPE_BOOK_REVISION_PRIME;
        event_id >>= 8U;
        ++index;
    }
    index = 0U;
    while (index < 4U)
    {
        revision = (revision ^ static_cast<uint8_t>(recipe_id)) * RECIPE_BOOK_REVISION_PRIME;
        recipe_id >>= 8U;
        ++index;
    }
    return (revision);
}

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
    : _catalogue(0U), _unlock_events(0U), _event_index(0U),
      _revision(RECIPE_BOOK_REVISION_SEED), _quarantined_count(0U),
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
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    if (recipe_ids.is_initialised() != FT_CLASS_STATE_INITIALISED
        || (initial_unlock_count != 0U && initial_unlocks == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->_catalogue.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->_unlock_events.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->_event_index.initialize();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    const ft_size_t recipe_count = recipe_ids.size();
    if (error_code == FT_ERR_SUCCESS)
    {
        this->_catalogue.reserve(recipe_count);
        error_code = this->_catalogue.get_error();
    }
    const uint32_t *ids = recipe_ids.begin();
    ft_size_t index = 0U;
    while (error_code == FT_ERR_SUCCESS && index < recipe_count)
    {
        if (ids[index] == 0U)
        {
            error_code = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        error_code = this->_catalogue.push_back({ids[index], FT_FALSE});
        index += 1U;
    }
    if (error_code == FT_ERR_SUCCESS && recipe_count > 1U)
    {
        auto *begin = this->_catalogue.begin();
        auto *end = begin + recipe_count;
        const auto less = [](const catalogue_entry &left,
            const catalogue_entry &right) noexcept
        {
            return (left.recipe_id < right.recipe_id);
        };
        if (!std::is_sorted(begin, end, less))
            std::sort(begin, end, less);
        if (std::adjacent_find(begin, end, [](const catalogue_entry &left,
                const catalogue_entry &right) noexcept
            {
                return (left.recipe_id == right.recipe_id);
            }) != end)
            error_code = FT_ERR_INVALID_ARGUMENT;
    }
    uint32_t unlock_index = 0U;
    while (error_code == FT_ERR_SUCCESS && unlock_index < initial_unlock_count)
    {
        const game_recipe_unlock_record &unlock = initial_unlocks[unlock_index];
        if (unlock.source_event_id == 0U || this->find_recipe(unlock.recipe_id) == ft_nullptr)
        {
            error_code = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        ++unlock_index;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->_unlock_events.append(initial_unlocks, initial_unlock_count);
    if (error_code == FT_ERR_SUCCESS)
        error_code = game_recipe_book_build_event_index(this->_unlock_events,
            this->_event_index);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (error_code);
    }
    this->rebuild_cached_state();
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
    error_code = this->_event_index.destroy();
    if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

int32_t game_recipe_book::unlock_recipe_with_result(uint32_t recipe_id,
    uint64_t source_event_id, game_recipe_unlock_result *result_out) noexcept
{
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || result_out == ft_nullptr || recipe_id == 0U
        || source_event_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result_out->event_newly_recorded = FT_FALSE;
    result_out->recipe_newly_known = FT_FALSE;
    auto *recipe = this->find_recipe(recipe_id);
    if (recipe == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    const ft_size_t count = this->_event_index.size();
    ft_size_t position = count;
    const auto *begin = this->_event_index.begin();
    if (count != 0U && source_event_id <= begin[count - 1U].source_event_id)
    {
        const auto *end = begin + count;
        const auto *event = std::lower_bound(begin, end, source_event_id,
            [](const game_recipe_unlock_record &entry, uint64_t id) noexcept
            {
                return (entry.source_event_id < id);
            });
        if (event != end && event->source_event_id == source_event_id)
        {
            if (event->recipe_id != recipe_id)
                return (FT_ERR_INVALID_STATE);
            return (FT_ERR_SUCCESS);
        }
        position = static_cast<ft_size_t>(event - begin);
    }
    const game_recipe_unlock_record record = {source_event_id, recipe_id};
    error_code = this->_unlock_events.push_back(record);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->_event_index.push_back(record);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_unlock_events.pop_back();
        return (error_code);
    }
    if (position < count)
    {
        static_assert(std::is_trivially_copyable_v<game_recipe_unlock_record>);
        auto *entries = this->_event_index.begin();
        (void)ft_memmove(entries + position + 1U, entries + position,
            (count - position) * sizeof(*entries));
        entries[position] = record;
    }
    result_out->event_newly_recorded = FT_TRUE;
    if (recipe->known == FT_FALSE)
        result_out->recipe_newly_known = FT_TRUE;
    recipe->known = FT_TRUE;
    this->_revision = game_recipe_book_extend_revision(this->_revision, record);
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
    const auto *recipe = this->find_recipe(recipe_id);
    if (recipe == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    *known = recipe->known;
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_book::get_revision(uint64_t *revision_out) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || revision_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *revision_out = this->_revision;
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_book::quarantined_unlock_count(uint32_t *count) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_quarantined_count;
    return (FT_ERR_SUCCESS);
}

const game_recipe_book::catalogue_entry *game_recipe_book::find_recipe(
    uint32_t recipe_id) const noexcept
{
    const ft_size_t count = this->_catalogue.size();
    if (count == 0U)
        return (ft_nullptr);
    const auto *begin = this->_catalogue.begin();
    const auto *end = begin + count;
    const auto *recipe = std::lower_bound(begin, end, recipe_id,
        [](const catalogue_entry &entry, uint32_t id) noexcept
        {
            return (entry.recipe_id < id);
        });
    if (recipe == end || recipe->recipe_id != recipe_id)
        return (ft_nullptr);
    return (recipe);
}

game_recipe_book::catalogue_entry *game_recipe_book::find_recipe(
    uint32_t recipe_id) noexcept
{
    return (const_cast<catalogue_entry *>(
        static_cast<const game_recipe_book *>(this)->find_recipe(recipe_id)));
}

void game_recipe_book::rebuild_cached_state() noexcept
{
    this->_revision = RECIPE_BOOK_REVISION_SEED;
    this->_quarantined_count = 0U;
    auto *recipe_entry = this->_catalogue.begin();
    const auto *catalogue_end = this->_catalogue.end();
    while (recipe_entry != catalogue_end)
        (recipe_entry++)->known = FT_FALSE;
    const auto *end = this->_unlock_events.end();
    const auto *event = this->_unlock_events.begin();
    while (event != end)
    {
        auto *recipe = this->find_recipe(event->recipe_id);
        if (recipe == ft_nullptr)
            ++this->_quarantined_count;
        else
            recipe->known = FT_TRUE;
        this->_revision = game_recipe_book_extend_revision(this->_revision, *event);
        ++event;
    }
    return ;
}

int32_t game_recipe_book::export_book(ft_byte_buffer &output) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (output.is_initialised() != FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_size_t count = this->_unlock_events.size();
    if (count > UINT32_MAX || count > (FT_SYSTEM_SIZE_MAX - GAME_RECIPE_BOOK_HEADER_SIZE
            - GAME_RECIPE_BOOK_CHECKSUM_SIZE) / GAME_RECIPE_BOOK_ENTRY_SIZE)
        return (FT_ERR_FULL);

    struct wire_source
    {
        const game_recipe_unlock_record *events;
        uint32_t count;
    };
    wire_source source = {this->_unlock_events.begin(), static_cast<uint32_t>(count)};
    const ft_size_t wire_size = GAME_RECIPE_BOOK_HEADER_SIZE
        + count * GAME_RECIPE_BOOK_ENTRY_SIZE + GAME_RECIPE_BOOK_CHECKSUM_SIZE;
    ft_byte_buffer staged;
    const int32_t result = staged.initialize_from_reader(wire_size,
        [](uint8_t *bytes, ft_size_t length, void *context) noexcept -> int32_t
        {
            const auto &book = *static_cast<const wire_source *>(context);
            buffer_store_le(bytes, GAME_RECIPE_BOOK_MAGIC);
            buffer_store_le(bytes + sizeof(uint32_t), GAME_RECIPE_BOOK_FORMAT_VERSION);
            buffer_store_le(bytes + 2U * sizeof(uint32_t), book.count);
            uint8_t *record = bytes + GAME_RECIPE_BOOK_HEADER_SIZE;
            uint32_t index = 0U;
            while (index < book.count)
            {
                buffer_store_le(record, book.events[index].source_event_id);
                buffer_store_le(record + sizeof(uint64_t), book.events[index].recipe_id);
                record += GAME_RECIPE_BOOK_ENTRY_SIZE;
                ++index;
            }
            buffer_store_le(record, game_recipe_book_checksum(bytes, length - GAME_RECIPE_BOOK_CHECKSUM_SIZE));
            return (FT_ERR_SUCCESS);
        }, &source);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (output.append_buffer(staged));
}

int32_t game_recipe_book::import_book(ft_byte_buffer &input) noexcept
{
    ft_vector<game_recipe_unlock_record> staged_events;
    ft_vector<game_recipe_unlock_record> staged_index;
    const uint8_t *payload_data;
    ft_size_t original_position;
    ft_size_t expected_size;
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t index;
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
    error_code = input.view(original_position, expected_size, &payload_data);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)input.set_read_position(original_position);
        return (error_code);
    }
    const ft_size_t checksum_offset = expected_size - GAME_RECIPE_BOOK_CHECKSUM_SIZE;
    if (buffer_load_le<uint32_t>(payload_data + checksum_offset)
        != game_recipe_book_checksum(payload_data, checksum_offset))
    {
        (void)input.set_read_position(original_position);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    error_code = staged_events.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = staged_index.initialize();
    if (error_code == FT_ERR_SUCCESS)
    {
        staged_events.reserve(entry_count);
        error_code = staged_events.get_error();
    }
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)input.set_read_position(original_position);
        return (error_code);
    }
    const uint8_t *record_data = payload_data + GAME_RECIPE_BOOK_HEADER_SIZE;
    index = 0U;
    while (error_code == FT_ERR_SUCCESS && index < entry_count)
    {
        const uint64_t source_event_id = buffer_load_le<uint64_t>(record_data);
        const uint32_t recipe_id = buffer_load_le<uint32_t>(record_data + sizeof(uint64_t));
        if (source_event_id == 0U || recipe_id == 0U)
        {
            error_code = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        error_code = staged_events.push_back({source_event_id, recipe_id});
        index += 1U;
        record_data += GAME_RECIPE_BOOK_ENTRY_SIZE;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = game_recipe_book_build_event_index(staged_events, staged_index);
    if (error_code == FT_ERR_SUCCESS)
        error_code = input.set_read_position(original_position + expected_size);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)input.set_read_position(original_position);
        return (error_code);
    }
    // These private vectors have no mutex, and moving initialized vectors of
    // trivial records transfers heap storage or copies inline records without allocation.
    (void)this->_unlock_events.move(staged_events);
    (void)this->_event_index.move(staged_index);
    this->rebuild_cached_state();
    return (FT_ERR_SUCCESS);
}
