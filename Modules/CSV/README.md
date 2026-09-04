# CSV

The `CSV` module provides a lightweight CSV document wrapper plus field escaping and row parsing helpers for tabular text data.

## Types

- `ft_csv_document` - Lifecycle-managed CSV table with flat field storage, row offsets, and optional header metadata.

## `ft_csv_document`

- `ft_csv_document()` / `~ft_csv_document()` - Construct and destroy the document wrapper.
- `initialize()` - Creates an empty initialized document.
- `initialize(const char *content, char delimiter = ',', ft_bool has_header = FT_FALSE)` - Parses CSV text into the document.
- `initialize(ft_document_source &source, char delimiter = ',', ft_bool has_header = FT_FALSE)` - Reads CSV text from a document backend.
- `initialize_from_file(const char *file_path, char delimiter = ',', ft_bool has_header = FT_FALSE)` - Reads CSV text from a file path.
- `initialize(const ft_csv_document &other)` / `initialize(ft_csv_document &&other)` - Copy or move initialize from another document.
- `destroy()` / `move(ft_csv_document &other)` - Release resources or move state from another document.
- `row_count()` / `column_count(ft_size_t row_index)` - Inspect parsed row and column counts.
- `has_header()` / `delimiter()` - Read parsed metadata.
- `get_row(ft_size_t row_index, ft_size_t *column_count_out)` - Returns a contiguous row span for manual iteration.
- `get_field(ft_size_t row_index, ft_size_t column_index)` / `get_field_by_name(ft_size_t row_index, const char *column_name)` / `get_header(ft_size_t column_index)` - Access parsed field values by row, column, or header name.
- `get_int64(ft_size_t row_index, ft_size_t column_index, int64_t *value_out)` / `get_int64(ft_size_t row_index, const char *column_name, int64_t *value_out)` - Parse a field as a signed 64-bit integer.
- `get_uint64(ft_size_t row_index, ft_size_t column_index, uint64_t *value_out)` / `get_uint64(ft_size_t row_index, const char *column_name, uint64_t *value_out)` - Parse a field as an unsigned 64-bit integer.
- `get_double(ft_size_t row_index, ft_size_t column_index, double *value_out)` / `get_double(ft_size_t row_index, const char *column_name, double *value_out)` - Parse a field as a double.
- `get_bool(ft_size_t row_index, ft_size_t column_index, ft_bool *value_out)` / `get_bool(ft_size_t row_index, const char *column_name, ft_bool *value_out)` - Parse a field as a project boolean.
- `write_to_string(ft_string &output)` / `write_to_backend(ft_document_sink &sink)` / `write_to_file(const char *file_path)` - Serialize the document back to CSV text.
- `get_error()` / `get_error_str()` - Query the most recent error state.

## Free Helpers

- `csv_split_line(const char *line, ft_vector<ft_string> &fields, char delimiter = ',')` - Splits one CSV record into fields.
- `csv_escape_field(const char *field, char delimiter = ',')` - Returns an allocated escaped CSV field string.
