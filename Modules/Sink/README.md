# Sink

The `Sink` module stores formatted log messages in insertion order and supports constant-time indexed lookup. Each message shares an allocation with its record.

## Public API
- `sink_record_message(int32_t level, const char *message)` - Stores one formatted log message and its numeric level.
- `sink_record_message(int32_t level, const char *message, ft_size_t length)` - Copies exactly `length` readable bytes and adds a null terminator.
- `sink_get_record_count(ft_size_t *count_out)` - Returns the number of stored messages.
- `sink_get_record(ft_size_t index, sink_record *record_out)` - Retrieves one stored message by index. The borrowed message pointer stays valid across appends. Coordinate use of the pointer with `sink_clear()`.
- `sink_clear()` - Frees all stored messages.

## Types
- `sink_record` - Stored log message entry with numeric level and message text.
