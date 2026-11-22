#ifndef JSON_STREAM_WRITER_HPP
#define JSON_STREAM_WRITER_HPP

#include <cstddef>
#include <stdbool.h>

#include "json_stream_events.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// Return the number of bytes accepted from this span. Partial writes are retried.
// Return zero to signal an I/O failure.
typedef ft_size_t (*json_stream_write_callback)(void *user_data, const char *buffer, ft_size_t size);

typedef struct json_stream_writer_context
{
    char type;
    ft_bool need_comma;
    ft_bool expecting_key;
} json_stream_writer_context;

typedef struct json_stream_writer
{
    json_stream_write_callback write_callback;
    void *user_data;
    json_stream_writer_context *context_stack;
    ft_size_t context_size;
    ft_size_t context_capacity;
    ft_bool began_document;
    ft_bool finished_document;
    ft_bool root_value_emitted;
    int32_t error_code;
} json_stream_writer;

int32_t     json_stream_writer_init(json_stream_writer *writer, json_stream_write_callback callback, void *user_data);
void    json_stream_writer_destroy(json_stream_writer *writer);
int32_t     json_stream_writer_process(json_stream_writer *writer, const json_stream_event *event);
int32_t     json_stream_writer_finish(json_stream_writer *writer);

#ifdef __cplusplus
}
#endif

#endif
