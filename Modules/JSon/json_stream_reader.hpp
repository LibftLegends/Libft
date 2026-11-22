#ifndef JSON_STREAM_READER_HPP
#define JSON_STREAM_READER_HPP

#include <cstddef>
#include <cstdio>
#include "../PThread/recursive_mutex.hpp"

#include "json_stream_events.hpp"

struct json_group;

// Return at most max_size bytes, zero at end of input, or (ft_size_t)-1 for I/O failure.
typedef ft_size_t (*json_stream_read_callback)(void *user_data, char *buffer, ft_size_t max_size);

typedef struct json_stream_reader
{
    json_stream_read_callback read_callback;
    void *user_data;
    char *buffer;
    ft_size_t buffer_capacity;
    ft_size_t buffer_size;
    ft_size_t buffer_index;
    ft_bool end_of_stream;
    int32_t error_code;
    mutable pt_recursive_mutex *_mutex;
    mutable int32_t _error_code;
} json_stream_reader;

int32_t         json_stream_reader_init_file(json_stream_reader *reader, FILE *file, ft_size_t buffer_capacity);
int32_t         json_stream_reader_init_callback(json_stream_reader *reader, json_stream_read_callback callback, void *user_data, ft_size_t buffer_capacity);
void        json_stream_reader_destroy(json_stream_reader *reader);
json_group  *json_stream_reader_parse(json_stream_reader *reader);
json_group  *json_read_from_file_stream(FILE *file, ft_size_t buffer_capacity);
json_group  *json_read_from_stream(json_stream_read_callback callback, void *user_data, ft_size_t buffer_capacity);
int32_t         json_stream_reader_traverse(json_stream_reader *reader, json_stream_event_callback callback, void *user_data);
int32_t         json_stream_read_from_file_events(FILE *file,
                ft_size_t buffer_capacity,
                json_stream_event_callback callback,
                void *user_data);
int32_t         json_stream_read_from_stream_events(json_stream_read_callback callback,
                void *user_data,
                ft_size_t buffer_capacity,
                json_stream_event_callback event_callback,
                void *event_user_data);

int32_t         json_stream_reader_enable_thread_safety(json_stream_reader *reader);
int32_t         json_stream_reader_lock(json_stream_reader *reader);
void        json_stream_reader_finalize_lock(json_stream_reader *reader);
void        json_stream_reader_set_error(json_stream_reader *reader, int32_t error_code);
void        json_stream_reader_set_error_unlocked(json_stream_reader *reader, int32_t error_code);
int32_t         json_stream_reader_get_error(const json_stream_reader *reader);
const char  *json_stream_reader_get_error_str(const json_stream_reader *reader);

#endif
