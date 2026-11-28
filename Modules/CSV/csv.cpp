#include "csv.hpp"

#include <cstdlib>
#include <cstring>
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"

static ft_bool csv_is_space_character(char character)
{
    if (character == ' ')
        return (FT_TRUE);
    if (character == '\t')
        return (FT_TRUE);
    if (character == '\v')
        return (FT_TRUE);
    if (character == '\f')
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool csv_is_valid_delimiter(char delimiter)
{
    if (delimiter == '\0' || delimiter == '"' || delimiter == '\n'
        || delimiter == '\r')
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool csv_field_needs_quotes(const char *field, ft_size_t length,
    char delimiter)
{
    if (length == 0U || csv_is_space_character(field[0]) == FT_TRUE
        || csv_is_space_character(field[length - 1U]) == FT_TRUE)
        return (FT_TRUE);
    ft_size_t index = 0U;
    while (index < length)
    {
        if (field[index] == delimiter || field[index] == '"'
            || field[index] == '\n' || field[index] == '\r')
            return (FT_TRUE);
        index += 1U;
    }
    return (FT_FALSE);
}

static int32_t csv_append_escaped_field(ft_string &output, const char *field,
    ft_size_t length, char delimiter)
{
    if (csv_field_needs_quotes(field, length, delimiter) == FT_FALSE)
        return (output.append(field, length));
    int32_t error_code = output.append('"');
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const char *end = field + length;
    while (field < end)
    {
        const char *quote = static_cast<const char *>(
            ft_memchr(field, '"', static_cast<ft_size_t>(end - field)));
        if (quote == ft_nullptr)
        {
            error_code = output.append(field, static_cast<ft_size_t>(end - field));
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            break ;
        }
        const char *next = quote + 1U;
        error_code = output.append(field, static_cast<ft_size_t>(next - field));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = output.append('"');
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        field = next;
    }
    return (output.append('"'));
}

static ft_bool csv_is_field_boundary(char character, char delimiter,
    ft_bool split_document)
{
    return (character == '\0' || character == delimiter
        || (split_document == FT_TRUE && (character == '\r' || character == '\n')));
}

static int32_t csv_read_field(const char *&cursor, char delimiter,
    ft_bool split_document, ft_string &field)
{
    if (*cursor != '"')
    {
        const char *start = cursor;
        while (csv_is_field_boundary(*cursor, delimiter, split_document) == FT_FALSE)
        {
            if (*cursor == '"')
                return (FT_ERR_INVALID_OPERATION);
            cursor += 1U;
        }
        if (cursor == start)
            return (FT_ERR_SUCCESS);
        return (field.append(start, static_cast<ft_size_t>(cursor - start)));
    }
    cursor += 1U;
    while (FT_TRUE)
    {
        const char *start = cursor;
        while (*cursor != '\0' && *cursor != '"')
            cursor += 1U;
        if (*cursor == '\0')
            return (FT_ERR_INVALID_OPERATION);
        if (cursor[1] == '"')
        {
            int32_t error_code = field.append(start,
                static_cast<ft_size_t>(cursor - start) + 1U);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            cursor += 2U;
            continue ;
        }
        if (cursor != start)
        {
            int32_t error_code = field.append(start,
                static_cast<ft_size_t>(cursor - start));
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        cursor += 1U;
        while (csv_is_field_boundary(*cursor, delimiter, split_document) == FT_FALSE)
        {
            if (csv_is_space_character(*cursor) == FT_FALSE)
                return (FT_ERR_INVALID_OPERATION);
            cursor += 1U;
        }
        return (FT_ERR_SUCCESS);
    }
}

static int32_t csv_finalize_field(ft_vector<ft_string> &fields,
    ft_string &current_field)
{
    int32_t error_code;

    error_code = fields.push_back(current_field);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = current_field.clear();
    if (error_code != FT_ERR_SUCCESS)
    {
        fields.clear();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t csv_finalize_row(ft_vector<ft_size_t> &row_offsets,
    ft_vector<ft_size_t> &row_lengths, ft_size_t row_offset,
    ft_size_t row_length)
{
    int32_t error_code;

    if (row_length == 0)
        return (FT_ERR_SUCCESS);
    error_code = row_offsets.push_back(row_offset);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = row_lengths.push_back(row_length);
    if (error_code != FT_ERR_SUCCESS)
    {
        row_offsets.pop_back();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t csv_parse_int64_field(const char *field, int64_t *value_out);
static int32_t csv_parse_double_field(const char *field, double *value_out);
static int32_t csv_parse_uint64_field(const char *field, uint64_t *value_out);
static int32_t csv_parse_bool_field(const char *field, ft_bool *value_out);

thread_local int32_t ft_csv_document::_last_error = FT_ERR_SUCCESS;

int32_t ft_csv_document::set_error(int32_t error_code) noexcept
{
    ft_csv_document::_last_error = error_code;
    return (error_code);
}

ft_csv_document::ft_csv_document() noexcept
    : _fields()
    , _row_offsets()
    , _row_lengths()
    , _delimiter(',')
    , _has_header(FT_FALSE)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_csv_document::~ft_csv_document() noexcept
{
    int32_t previous_error;

    previous_error = ft_csv_document::_last_error;
    (void)this->destroy();
    (void)ft_csv_document::set_error(previous_error);
    return ;
}

int32_t ft_csv_document::initialize() noexcept
{
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "ft_csv_document::initialize",
            "called while object is already initialised");
        return (ft_csv_document::set_error(FT_ERR_INVALID_STATE));
    }
    error_code = this->_fields.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (ft_csv_document::set_error(error_code));
    error_code = this->_row_offsets.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_fields.destroy();
        return (ft_csv_document::set_error(error_code));
    }
    error_code = this->_row_lengths.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_row_offsets.destroy();
        (void)this->_fields.destroy();
        return (ft_csv_document::set_error(error_code));
    }
    this->_delimiter = ',';
    this->_has_header = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (ft_csv_document::set_error(FT_ERR_SUCCESS));
}

int32_t ft_csv_document::parse_content(const char *content, char delimiter,
    ft_bool has_header) noexcept
{
    if (content == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (csv_is_valid_delimiter(delimiter) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string current_field;
    int32_t error_code = current_field.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const char *cursor = content;
    ft_size_t row_offset = 0U;
    while (*cursor != '\0')
    {
        ft_size_t row_field_count = 0U;
        while (FT_TRUE)
        {
            error_code = csv_read_field(cursor, delimiter, FT_TRUE, current_field);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            error_code = csv_finalize_field(this->_fields, current_field);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            row_field_count += 1U;
            if (*cursor != delimiter)
                break ;
            cursor += 1U;
        }
        error_code = csv_finalize_row(this->_row_offsets, this->_row_lengths,
            row_offset, row_field_count);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        row_offset += row_field_count;
        if (*cursor == '\r' && cursor[1] == '\n')
            cursor += 1U;
        if (*cursor != '\0')
            cursor += 1U;
    }
    this->_delimiter = delimiter;
    this->_has_header = has_header;
    return (FT_ERR_SUCCESS);
}

int32_t ft_csv_document::initialize(const char *content, char delimiter,
    ft_bool has_header) noexcept
{
    int32_t error_code;

    if (csv_is_valid_delimiter(delimiter) == FT_FALSE)
        return (ft_csv_document::set_error(FT_ERR_INVALID_ARGUMENT));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        error_code = this->destroy();
        if (error_code != FT_ERR_SUCCESS)
            return (ft_csv_document::set_error(error_code));
    }
    error_code = this->initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (ft_csv_document::set_error(error_code));
    error_code = this->parse_content(content, delimiter, has_header);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (ft_csv_document::set_error(error_code));
    }
    return (ft_csv_document::set_error(FT_ERR_SUCCESS));
}

int32_t ft_csv_document::initialize(ft_document_source &source, char delimiter,
    ft_bool has_header) noexcept
{
    ft_string content;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        error_code = this->destroy();
        if (error_code != FT_ERR_SUCCESS)
            return (ft_csv_document::set_error(error_code));
    }
    error_code = content.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (ft_csv_document::set_error(error_code));
    error_code = source.read_all(content);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)content.destroy();
        return (ft_csv_document::set_error(error_code));
    }
    error_code = this->initialize(content.c_str(), delimiter, has_header);
    (void)content.destroy();
    return (ft_csv_document::set_error(error_code));
}

int32_t ft_csv_document::initialize_from_file(const char *file_path,
    char delimiter, ft_bool has_header) noexcept
{
    ft_string content;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        error_code = this->destroy();
        if (error_code != FT_ERR_SUCCESS)
            return (ft_csv_document::set_error(error_code));
    }
    if (file_path == ft_nullptr)
        return (ft_csv_document::set_error(FT_ERR_INVALID_POINTER));
    error_code = content.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (ft_csv_document::set_error(error_code));
    error_code = file_read_all(file_path, content);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)content.destroy();
        return (ft_csv_document::set_error(error_code));
    }
    error_code = this->initialize(content.c_str(), delimiter, has_header);
    (void)content.destroy();
    return (ft_csv_document::set_error(error_code));
}

int32_t ft_csv_document::initialize(const ft_csv_document &other) noexcept
{
    int32_t error_code;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_csv_document::initialize(const ft_csv_document &)",
            "source object is uninitialised");
        return (ft_csv_document::set_error(FT_ERR_INVALID_STATE));
    }
    if (this == &other)
        return (ft_csv_document::set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        error_code = this->destroy();
        if (error_code != FT_ERR_SUCCESS)
            return (ft_csv_document::set_error(error_code));
    }
    error_code = this->initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (ft_csv_document::set_error(error_code));
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_delimiter = other._delimiter;
        this->_has_header = other._has_header;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_csv_document::set_error(other._last_error));
    }
    error_code = this->_fields.initialize(other._fields);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (ft_csv_document::set_error(error_code));
    }
    error_code = this->_row_offsets.initialize(other._row_offsets);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (ft_csv_document::set_error(error_code));
    }
    error_code = this->_row_lengths.initialize(other._row_lengths);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (ft_csv_document::set_error(error_code));
    }
    this->_delimiter = other._delimiter;
    this->_has_header = other._has_header;
    return (ft_csv_document::set_error(other._last_error));
}

int32_t ft_csv_document::initialize(ft_csv_document &&other) noexcept
{
    return (this->move(other));
}

int32_t ft_csv_document::destroy() noexcept
{
    int32_t error_code;
    int32_t first_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (ft_csv_document::set_error(FT_ERR_SUCCESS));
    first_error = FT_ERR_SUCCESS;
    error_code = this->_fields.destroy();
    if (error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    error_code = this->_row_offsets.destroy();
    if (error_code != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = error_code;
    error_code = this->_row_lengths.destroy();
    if (error_code != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = error_code;
    this->_delimiter = ',';
    this->_has_header = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (ft_csv_document::set_error(first_error));
}

int32_t ft_csv_document::move(ft_csv_document &other) noexcept
{
    int32_t error_code;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_csv_document::move",
            "source object is uninitialised");
        return (ft_csv_document::set_error(FT_ERR_INVALID_STATE));
    }
    if (this == &other)
        return (ft_csv_document::set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        error_code = this->destroy();
        if (error_code != FT_ERR_SUCCESS)
            return (ft_csv_document::set_error(error_code));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_delimiter = other._delimiter;
        this->_has_header = other._has_header;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_csv_document::set_error(other._last_error));
    }
    error_code = this->_fields.move(other._fields);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_fields.destroy();
        (void)this->_row_offsets.destroy();
        (void)this->_row_lengths.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_csv_document::set_error(error_code));
    }
    error_code = this->_row_offsets.move(other._row_offsets);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_fields.destroy();
        (void)this->_row_offsets.destroy();
        (void)this->_row_lengths.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_csv_document::set_error(error_code));
    }
    error_code = this->_row_lengths.move(other._row_lengths);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_fields.destroy();
        (void)this->_row_offsets.destroy();
        (void)this->_row_lengths.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_csv_document::set_error(error_code));
    }
    this->_delimiter = other._delimiter;
    this->_has_header = other._has_header;
    other._delimiter = ',';
    other._has_header = FT_FALSE;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (ft_csv_document::set_error(FT_ERR_SUCCESS));
}

ft_bool ft_csv_document::find_header_column_index(const char *column_name,
    ft_size_t *column_index_out) const noexcept
{
    ft_size_t column_index;
    const ft_string *header_field;

    if (column_index_out != ft_nullptr)
        *column_index_out = 0;
    if (column_name == ft_nullptr)
        return (FT_FALSE);
    if (this->_has_header == FT_FALSE || this->_row_lengths.empty() == FT_TRUE)
        return (FT_FALSE);
    column_index = 0;
    while (column_index < this->_row_lengths[0])
    {
        header_field = &this->_fields[this->_row_offsets[0] + column_index];
        if (*header_field == column_name)
        {
            if (column_index_out != ft_nullptr)
                *column_index_out = column_index;
            return (FT_TRUE);
        }
        column_index += 1;
    }
    return (FT_FALSE);
}

ft_size_t ft_csv_document::row_count() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::row_count");
    return (this->_row_lengths.size());
}

ft_size_t ft_csv_document::column_count(ft_size_t row_index) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::column_count");
    if (row_index >= this->_row_lengths.size())
    {
        (void)ft_csv_document::set_error(FT_ERR_OUT_OF_RANGE);
        return (0);
    }
    (void)ft_csv_document::set_error(FT_ERR_SUCCESS);
    return (this->_row_lengths[row_index]);
}

ft_bool ft_csv_document::has_header() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::has_header");
    return (this->_has_header);
}

char ft_csv_document::delimiter() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::delimiter");
    return (this->_delimiter);
}

const ft_string *ft_csv_document::get_row(ft_size_t row_index,
    ft_size_t *column_count_out) const noexcept
{
    ft_size_t row_offset;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_row");
    if (column_count_out != ft_nullptr)
        *column_count_out = 0;
    if (row_index >= this->_row_lengths.size())
    {
        (void)ft_csv_document::set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    row_offset = this->_row_offsets[row_index];
    if (row_offset >= this->_fields.size())
    {
        (void)ft_csv_document::set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    if (column_count_out != ft_nullptr)
        *column_count_out = this->_row_lengths[row_index];
    (void)ft_csv_document::set_error(FT_ERR_SUCCESS);
    return (&this->_fields[row_offset]);
}

const ft_string *ft_csv_document::get_field(ft_size_t row_index,
    ft_size_t column_index) const noexcept
{
    ft_size_t row_offset;
    ft_size_t field_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_field");
    if (row_index >= this->_row_lengths.size())
    {
        (void)ft_csv_document::set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    if (column_index >= this->_row_lengths[row_index])
    {
        (void)ft_csv_document::set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    row_offset = this->_row_offsets[row_index];
    field_index = row_offset + column_index;
    if (field_index >= this->_fields.size())
    {
        (void)ft_csv_document::set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    (void)ft_csv_document::set_error(FT_ERR_SUCCESS);
    return (&this->_fields[field_index]);
}

const ft_string *ft_csv_document::get_field_by_name(ft_size_t row_index,
    const char *column_name) const noexcept
{
    ft_size_t column_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_field_by_name");
    if (column_name == ft_nullptr)
    {
        (void)ft_csv_document::set_error(FT_ERR_INVALID_POINTER);
        return (ft_nullptr);
    }
    if (this->find_header_column_index(column_name, &column_index) == FT_FALSE)
    {
        if (this->_has_header == FT_FALSE)
            (void)ft_csv_document::set_error(FT_ERR_INVALID_OPERATION);
        else
            (void)ft_csv_document::set_error(FT_ERR_NOT_FOUND);
        return (ft_nullptr);
    }
    return (this->get_field(row_index, column_index));
}

const ft_string *ft_csv_document::get_header(ft_size_t column_index) const noexcept
{
    if (this->has_header() == FT_FALSE)
        return (ft_nullptr);
    return (this->get_field(0, column_index));
}

int32_t ft_csv_document::get_int64(ft_size_t row_index, ft_size_t column_index,
    int64_t *value_out) const noexcept
{
    const ft_string *field_pointer;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_int64");
    if (value_out == ft_nullptr)
    {
        (void)ft_csv_document::set_error(FT_ERR_INVALID_POINTER);
        return (FT_ERR_INVALID_POINTER);
    }
    field_pointer = this->get_field(row_index, column_index);
    if (field_pointer == ft_nullptr)
        return (ft_csv_document::set_error(ft_csv_document::_last_error));
    error_code = csv_parse_int64_field(field_pointer->c_str(), value_out);
    return (ft_csv_document::set_error(error_code));
}

int32_t ft_csv_document::get_int64(ft_size_t row_index, const char *column_name,
    int64_t *value_out) const noexcept
{
    const ft_string *field_pointer;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_int64");
    if (value_out == ft_nullptr)
    {
        (void)ft_csv_document::set_error(FT_ERR_INVALID_POINTER);
        return (FT_ERR_INVALID_POINTER);
    }
    field_pointer = this->get_field_by_name(row_index, column_name);
    if (field_pointer == ft_nullptr)
        return (ft_csv_document::set_error(ft_csv_document::_last_error));
    error_code = csv_parse_int64_field(field_pointer->c_str(), value_out);
    return (ft_csv_document::set_error(error_code));
}

int32_t ft_csv_document::get_double(ft_size_t row_index, ft_size_t column_index,
    double *value_out) const noexcept
{
    const ft_string *field_pointer;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_double");
    if (value_out == ft_nullptr)
    {
        (void)ft_csv_document::set_error(FT_ERR_INVALID_POINTER);
        return (FT_ERR_INVALID_POINTER);
    }
    field_pointer = this->get_field(row_index, column_index);
    if (field_pointer == ft_nullptr)
        return (ft_csv_document::set_error(ft_csv_document::_last_error));
    error_code = csv_parse_double_field(field_pointer->c_str(), value_out);
    return (ft_csv_document::set_error(error_code));
}

int32_t ft_csv_document::get_double(ft_size_t row_index, const char *column_name,
    double *value_out) const noexcept
{
    const ft_string *field_pointer;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_double");
    if (value_out == ft_nullptr)
    {
        (void)ft_csv_document::set_error(FT_ERR_INVALID_POINTER);
        return (FT_ERR_INVALID_POINTER);
    }
    field_pointer = this->get_field_by_name(row_index, column_name);
    if (field_pointer == ft_nullptr)
        return (ft_csv_document::set_error(ft_csv_document::_last_error));
    error_code = csv_parse_double_field(field_pointer->c_str(), value_out);
    return (ft_csv_document::set_error(error_code));
}

int32_t ft_csv_document::get_uint64(ft_size_t row_index, ft_size_t column_index,
    uint64_t *value_out) const noexcept
{
    const ft_string *field_pointer;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_uint64");
    if (value_out == ft_nullptr)
    {
        (void)ft_csv_document::set_error(FT_ERR_INVALID_POINTER);
        return (FT_ERR_INVALID_POINTER);
    }
    field_pointer = this->get_field(row_index, column_index);
    if (field_pointer == ft_nullptr)
        return (ft_csv_document::set_error(ft_csv_document::_last_error));
    error_code = csv_parse_uint64_field(field_pointer->c_str(), value_out);
    return (ft_csv_document::set_error(error_code));
}

int32_t ft_csv_document::get_uint64(ft_size_t row_index, const char *column_name,
    uint64_t *value_out) const noexcept
{
    const ft_string *field_pointer;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_uint64");
    if (value_out == ft_nullptr)
    {
        (void)ft_csv_document::set_error(FT_ERR_INVALID_POINTER);
        return (FT_ERR_INVALID_POINTER);
    }
    field_pointer = this->get_field_by_name(row_index, column_name);
    if (field_pointer == ft_nullptr)
        return (ft_csv_document::set_error(ft_csv_document::_last_error));
    error_code = csv_parse_uint64_field(field_pointer->c_str(), value_out);
    return (ft_csv_document::set_error(error_code));
}

int32_t ft_csv_document::get_bool(ft_size_t row_index, ft_size_t column_index,
    ft_bool *value_out) const noexcept
{
    const ft_string *field_pointer;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_bool");
    if (value_out == ft_nullptr)
    {
        (void)ft_csv_document::set_error(FT_ERR_INVALID_POINTER);
        return (FT_ERR_INVALID_POINTER);
    }
    field_pointer = this->get_field(row_index, column_index);
    if (field_pointer == ft_nullptr)
        return (ft_csv_document::set_error(ft_csv_document::_last_error));
    error_code = csv_parse_bool_field(field_pointer->c_str(), value_out);
    return (ft_csv_document::set_error(error_code));
}

int32_t ft_csv_document::get_bool(ft_size_t row_index, const char *column_name,
    ft_bool *value_out) const noexcept
{
    const ft_string *field_pointer;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::get_bool");
    if (value_out == ft_nullptr)
    {
        (void)ft_csv_document::set_error(FT_ERR_INVALID_POINTER);
        return (FT_ERR_INVALID_POINTER);
    }
    field_pointer = this->get_field_by_name(row_index, column_name);
    if (field_pointer == ft_nullptr)
        return (ft_csv_document::set_error(ft_csv_document::_last_error));
    error_code = csv_parse_bool_field(field_pointer->c_str(), value_out);
    return (ft_csv_document::set_error(error_code));
}

int32_t ft_csv_document::write_to_string(ft_string &output) const noexcept
{
    ft_size_t row_index;
    ft_size_t column_index;
    ft_size_t row_total;
    ft_size_t column_total;
    const ft_string *field_pointer;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::write_to_string");
    if (output.is_initialised() == FT_FALSE)
    {
        error_code = output.initialize();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    else
    {
        error_code = output.clear();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    row_total = this->_row_lengths.size();
    row_index = 0;
    while (row_index < row_total)
    {
        field_pointer = this->get_row(row_index, &column_total);
        if (field_pointer == ft_nullptr)
            return (FT_ERR_INVALID_OPERATION);
        column_index = 0;
        while (column_index < column_total)
        {
            if (column_index > 0)
            {
                if (output.append(this->_delimiter) != FT_ERR_SUCCESS)
                    return (output.get_error());
            }
            const ft_string &field = field_pointer[column_index];
            error_code = csv_append_escaped_field(output, field.c_str(),
                field.size(), this->_delimiter);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            column_index += 1;
        }
        if (row_index + 1 < row_total)
        {
            if (output.append('\n') != FT_ERR_SUCCESS)
                return (output.get_error());
        }
        row_index += 1;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t csv_parse_int64_field(const char *field, int64_t *value_out)
{
    char *end_pointer;
    int64_t parsed_value;

    if (field == ft_nullptr || value_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    end_pointer = ft_nullptr;
    parsed_value = ft_strtol(field, &end_pointer, 10);
    if (end_pointer == ft_nullptr || end_pointer == field || *end_pointer != '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    *value_out = parsed_value;
    return (FT_ERR_SUCCESS);
}

static int32_t csv_parse_double_field(const char *field, double *value_out)
{
    char *end_pointer;

    if (field == ft_nullptr || value_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    end_pointer = ft_nullptr;
    *value_out = std::strtod(field, &end_pointer);
    if (end_pointer == ft_nullptr || end_pointer == field || *end_pointer != '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

static int32_t csv_parse_uint64_field(const char *field, uint64_t *value_out)
{
    char *end_pointer;
    uint64_t parsed_value;

    if (field == ft_nullptr || value_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    end_pointer = ft_nullptr;
    parsed_value = ft_strtoul(field, &end_pointer, 10);
    if (end_pointer == ft_nullptr || end_pointer == field || *end_pointer != '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    *value_out = parsed_value;
    return (FT_ERR_SUCCESS);
}

static int32_t csv_parse_bool_field(const char *field, ft_bool *value_out)
{
    if (field == ft_nullptr || value_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (ft_strcmp(field, "1") == 0 || ft_strcmp(field, "true") == 0
        || ft_strcmp(field, "yes") == 0 || ft_strcmp(field, "on") == 0)
    {
        *value_out = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }
    if (ft_strcmp(field, "0") == 0 || ft_strcmp(field, "false") == 0
        || ft_strcmp(field, "no") == 0 || ft_strcmp(field, "off") == 0)
    {
        *value_out = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_INVALID_ARGUMENT);
}

int32_t ft_csv_document::write_to_backend(ft_document_sink &sink) const noexcept
{
    ft_string output;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::write_to_backend");
    error_code = output.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->write_to_string(output);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)output.destroy();
        return (error_code);
    }
    error_code = sink.write_all(output.c_str(), output.size());
    (void)output.destroy();
    return (error_code);
}

int32_t ft_csv_document::write_to_file(const char *file_path) const noexcept
{
    ft_string output;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_csv_document::write_to_file");
    if (file_path == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    error_code = output.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->write_to_string(output);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)output.destroy();
        return (error_code);
    }
    error_code = file_write_all(file_path, output.c_str(), output.size());
    (void)output.destroy();
    return (error_code);
}

int32_t ft_csv_document::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_csv_document::get_error");
    return (ft_csv_document::_last_error);
}

const char *ft_csv_document::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_csv_document::get_error_str");
    return (ft_strerror(this->get_error()));
}

int32_t csv_split_line(const char *line, ft_vector<ft_string> &fields,
    char delimiter) noexcept
{
    if (line == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (csv_is_valid_delimiter(delimiter) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (fields.is_initialised() == FT_FALSE)
    {
        int32_t error_code = fields.initialize();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    else
        fields.clear();
    if (*line == '\0')
        return (FT_ERR_SUCCESS);
    ft_string current_field;
    int32_t error_code = current_field.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const char *cursor = line;
    while (FT_TRUE)
    {
        error_code = csv_read_field(cursor, delimiter, FT_FALSE, current_field);
        if (error_code == FT_ERR_SUCCESS)
            error_code = csv_finalize_field(fields, current_field);
        if (error_code != FT_ERR_SUCCESS)
        {
            fields.clear();
            return (error_code);
        }
        if (*cursor == '\0')
            break ;
        cursor += 1U;
    }
    return (FT_ERR_SUCCESS);
}

char *csv_escape_field(const char *field, char delimiter) noexcept
{
    if (field == ft_nullptr)
        return (ft_nullptr);
    const ft_size_t field_length = std::strlen(field);
    if (csv_field_needs_quotes(field, field_length, delimiter) == FT_FALSE)
    {
        char *escaped = static_cast<char *>(cma_malloc(field_length + 1U));
        if (escaped != ft_nullptr)
            ft_memcpy(escaped, field, field_length + 1U);
        return (escaped);
    }
    ft_size_t quote_count = 0U;
    ft_size_t index = 0U;
    while (index < field_length)
    {
        if (field[index] == '"')
            quote_count += 1U;
        index += 1U;
    }
    if (field_length > SIZE_MAX - 3U
        || quote_count > SIZE_MAX - 3U - field_length)
        return (ft_nullptr);
    char *escaped = static_cast<char *>(cma_malloc(field_length + quote_count + 3U));
    if (escaped == ft_nullptr)
        return (ft_nullptr);
    char *destination = escaped;
    *destination++ = '"';
    index = 0U;
    while (index < field_length)
    {
        const char character = field[index];
        if (character == '"')
            *destination++ = '"';
        *destination++ = character;
        index += 1U;
    }
    *destination++ = '"';
    *destination = '\0';
    return (escaped);
}
