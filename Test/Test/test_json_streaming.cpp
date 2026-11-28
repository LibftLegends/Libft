#include "../test_internal.hpp"
#include "../../Modules/JSon/json_stream_reader.hpp"
#include "../../Modules/JSon/json_stream_writer.hpp"
#include <string>
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

typedef struct memory_stream
{
    const char *data;
    ft_size_t length;
    ft_size_t offset;
} memory_stream;

typedef struct captured_event
{
    json_stream_event_type type;
    ft_string value;
    bool bool_value;
} captured_event;

static ft_size_t memory_stream_reader(void *user_data, char *buffer,
    ft_size_t max_size)
{
    memory_stream *stream;
    ft_size_t remaining;
    ft_size_t index;

    if (!user_data || !buffer || max_size == 0)
        return (0);
    stream = static_cast<memory_stream *>(user_data);
    if (stream->offset >= stream->length)
        return (0);
    remaining = stream->length - stream->offset;
    if (remaining > max_size)
        remaining = max_size;
    index = 0;
    while (index < remaining)
    {
        buffer[index] = stream->data[stream->offset + index];
        index += 1;
    }
    stream->offset += remaining;
    return (remaining);
}

static int capture_event(void *user_data, const json_stream_event *event)
{
    ft_vector<captured_event> *events;
    captured_event record;

    if (!user_data || !event)
        return (-1);
    events = static_cast<ft_vector<captured_event> *>(user_data);
    if (record.value.initialize() != FT_ERR_SUCCESS)
        return (-1);
    record.type = event->type;
    record.bool_value = event->bool_value;
    if (event->value.data != ft_nullptr && event->value.length > 0)
        record.value.assign(event->value.data, event->value.length);
    else
        record.value.clear();
    events->push_back(record);
    (void)record.value.destroy();
    return (0);
}

static int gather_events(const char *json_text, ft_vector<captured_event> &events)
{
    memory_stream stream;
    json_stream_reader reader = json_stream_reader();
    int status;

    events.clear();
    if (!json_text)
        return (-1);
    stream.data = json_text;
    stream.length = ft_strlen_size_t(json_text);
    stream.offset = 0;
    if (json_stream_reader_init_callback(&reader, memory_stream_reader, &stream, 16) != 0)
        return (-1);
    status = json_stream_reader_traverse(&reader, capture_event, &events);
    json_stream_reader_destroy(&reader);
    return (status);
}

static ft_size_t string_sink_write(void *user_data, const char *buffer,
    ft_size_t size)
{
    ft_string *output;

    if (!user_data || !buffer)
        return (0);
    output = static_cast<ft_string *>(user_data);
    output->append(buffer, size);
    return (size);
}

FT_TEST(test_json_stream_reader_emits_events)
{
    const char *json_text;
    ft_vector<captured_event> events;
    int status;
    size_t expected_count;
    size_t index;
    static const json_stream_event_type expected_types[] = {
        JSON_STREAM_EVENT_BEGIN_DOCUMENT,
        JSON_STREAM_EVENT_BEGIN_OBJECT,
        JSON_STREAM_EVENT_KEY,
        JSON_STREAM_EVENT_BEGIN_ARRAY,
        JSON_STREAM_EVENT_NUMBER,
        JSON_STREAM_EVENT_NUMBER,
        JSON_STREAM_EVENT_NUMBER,
        JSON_STREAM_EVENT_END_ARRAY,
        JSON_STREAM_EVENT_KEY,
        JSON_STREAM_EVENT_BEGIN_OBJECT,
        JSON_STREAM_EVENT_KEY,
        JSON_STREAM_EVENT_BOOLEAN,
        JSON_STREAM_EVENT_KEY,
        JSON_STREAM_EVENT_STRING,
        JSON_STREAM_EVENT_END_OBJECT,
        JSON_STREAM_EVENT_END_OBJECT,
        JSON_STREAM_EVENT_END_DOCUMENT
    };

    json_text = "{\n  \"numbers\": [1, 2, 3],\n  \"nested\": {\n    \"flag\": true,\n    \"text\": \"hello\"\n  }\n}\n";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());
    status = gather_events(json_text, events);
    FT_ASSERT_EQ(0, status);
    expected_count = sizeof(expected_types) / sizeof(expected_types[0]);
    FT_ASSERT_EQ(expected_count, events.size());
    index = 0;
    while (index < expected_count)
    {
        FT_ASSERT_EQ(static_cast<int>(expected_types[index]), static_cast<int>(events[index].type));
        index += 1;
    }
    FT_ASSERT_EQ(0, ft_strcmp(events[2].value.c_str(), "numbers"));
    FT_ASSERT_EQ(0, ft_strcmp(events[4].value.c_str(), "1"));
    FT_ASSERT_EQ(0, ft_strcmp(events[5].value.c_str(), "2"));
    FT_ASSERT_EQ(0, ft_strcmp(events[6].value.c_str(), "3"));
    FT_ASSERT_EQ(0, ft_strcmp(events[8].value.c_str(), "nested"));
    FT_ASSERT_EQ(0, ft_strcmp(events[10].value.c_str(), "flag"));
    FT_ASSERT_EQ(true, events[11].bool_value);
    FT_ASSERT_EQ(0, ft_strcmp(events[12].value.c_str(), "text"));
    FT_ASSERT_EQ(0, ft_strcmp(events[13].value.c_str(), "hello"));
    return (1);
}

FT_TEST(test_json_stream_writer_round_trip)
{
    const char *json_text;
    ft_vector<captured_event> events;
    json_stream_writer writer;
    ft_string encoded;
    size_t index;
    size_t count;

    json_text = "{\n  \"numbers\": [1, 2, 3],\n  \"nested\": {\n    \"flag\": true,\n    \"text\": \"hello\"\n  }\n}\n";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.initialize());
    FT_ASSERT_EQ(0, gather_events(json_text, events));
    FT_ASSERT_EQ(0, json_stream_writer_init(&writer, string_sink_write, &encoded));
    index = 0;
    count = events.size();
    while (index < count)
    {
        json_stream_event event;

        ft_bzero(&event, sizeof(event));
        event.type = events[index].type;
        event.bool_value = events[index].bool_value;
        if (events[index].value.size() > 0)
        {
            event.value.data = events[index].value.c_str();
            event.value.length = events[index].value.size();
        }
        else
        {
            event.value.data = ft_nullptr;
            event.value.length = 0;
        }
        FT_ASSERT_EQ(0, json_stream_writer_process(&writer, &event));
        index += 1;
    }
    FT_ASSERT_EQ(0, json_stream_writer_finish(&writer));
    json_stream_writer_destroy(&writer);
    FT_ASSERT_EQ(0, ft_strcmp(encoded.c_str(), "{\"numbers\":[1,2,3],\"nested\":{\"flag\":true,\"text\":\"hello\"}}"));
    return (1);
}

struct json_partial_sink
{
    std::string output;
    ft_size_t largest_offer;
};

static ft_size_t json_partial_sink_write(void *user_data, const char *buffer, ft_size_t size)
{
    json_partial_sink *sink = static_cast<json_partial_sink *>(user_data);
    if (size > sink->largest_offer)
        sink->largest_offer = size;
    if (size > 17U)
        size = 17U;
    sink->output.append(buffer, size);
    return (size);
}

FT_TEST(test_json_stream_writer_batches_plain_spans_and_retries_partial_writes)
{
    const std::string prefix(4096U, 'a');
    const std::string suffix(2050U, 'b');
    const std::string value = prefix + std::string("\0\n\"\\", 4U) + suffix;
    const std::string expected = "\"" + prefix + "\\u0000\\n\\\"\\\\" + suffix + "\"";
    json_partial_sink sink{};
    json_stream_writer writer{};
    json_stream_event event{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_writer_init(&writer, json_partial_sink_write, &sink));
    event.type = JSON_STREAM_EVENT_BEGIN_DOCUMENT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_writer_process(&writer, &event));
    event.type = JSON_STREAM_EVENT_STRING;
    event.value = {value.data(), value.size()};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_writer_process(&writer, &event));
    event.type = JSON_STREAM_EVENT_END_DOCUMENT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_writer_process(&writer, &event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_writer_finish(&writer));
    json_stream_writer_destroy(&writer);
    FT_ASSERT_EQ(expected, sink.output);
    FT_ASSERT_EQ(prefix.size(), sink.largest_offer);
    return (1);
}

static ft_size_t json_overreporting_sink(void *, const char *, ft_size_t size)
{
    return (size + 1U);
}

FT_TEST(test_json_stream_writer_reset_reuses_storage_after_failed_document)
{
    json_stream_writer writer{};
    json_partial_sink sink{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_writer_init(&writer, json_partial_sink_write, &sink));
    const json_stream_event begin_document{JSON_STREAM_EVENT_BEGIN_DOCUMENT, {}, FT_FALSE};
    const json_stream_event begin_array{JSON_STREAM_EVENT_BEGIN_ARRAY, {}, FT_FALSE};
    const json_stream_event end_array{JSON_STREAM_EVENT_END_ARRAY, {}, FT_FALSE};
    const json_stream_event end_document{JSON_STREAM_EVENT_END_DOCUMENT, {}, FT_FALSE};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_writer_process(&writer, &begin_document));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_writer_process(&writer, &begin_array));
    FT_ASSERT(json_stream_writer_process(&writer, &end_document) != FT_ERR_SUCCESS);
    const auto *const scratch = writer.context_stack;
    FT_ASSERT(scratch != ft_nullptr);
    sink.output.clear();
    cma_set_alloc_limit(1);
    const int32_t reset = json_stream_writer_reset(&writer);
    const int32_t document = json_stream_writer_process(&writer, &begin_document);
    const int32_t array = json_stream_writer_process(&writer, &begin_array);
    const int32_t close_array = json_stream_writer_process(&writer, &end_array);
    const int32_t close_document = json_stream_writer_process(&writer, &end_document);
    const ft_bool reused = writer.context_stack == scratch;
    cma_set_alloc_limit(0);
    const int32_t finished = json_stream_writer_finish(&writer);
    json_stream_writer_destroy(&writer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reset);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, array);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, close_array);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, close_document);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finished);
    FT_ASSERT_EQ(FT_TRUE, reused);
    FT_ASSERT_EQ(std::string("[]"), sink.output);
    return (1);
}

FT_TEST(test_json_stream_writer_rejects_a_callback_that_overreports)
{
    json_stream_writer writer{};
    json_stream_event event{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_writer_init(&writer, json_overreporting_sink, ft_nullptr));
    event.type = JSON_STREAM_EVENT_BEGIN_DOCUMENT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_writer_process(&writer, &event));
    event.type = JSON_STREAM_EVENT_STRING;
    event.value = {"content", 7U};
    FT_ASSERT(json_stream_writer_process(&writer, &event) != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_IO, writer.error_code);
    json_stream_writer_destroy(&writer);
    return (1);
}

FT_TEST(test_json_stream_reader_keeps_decoded_nul_and_numeric_spans_in_events)
{
    const std::string prefix(4097U, 'p');
    const std::string suffix(1030U, 's');
    const std::string digits(240U, '7');
    const std::string number = "-" + digits + ".125e+12";
    const std::string document = "[\"" + prefix + "\\u0000" + suffix + "\"," + number + "]";
    const std::string decoded = prefix + '\0' + suffix;
    ft_vector<captured_event> events;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gather_events(document.c_str(), events));
    FT_ASSERT_EQ(6U, events.size());
    FT_ASSERT_EQ(JSON_STREAM_EVENT_STRING, events[2].type);
    FT_ASSERT_EQ(decoded, std::string(events[2].value.c_str(), events[2].value.size()));
    FT_ASSERT_EQ(JSON_STREAM_EVENT_NUMBER, events[3].type);
    FT_ASSERT_EQ(number, std::string(events[3].value.c_str(), events[3].value.size()));
    return (1);
}

static ft_size_t json_overreporting_source(void *, char *, ft_size_t capacity)
{
    return (capacity + 1U);
}

FT_TEST(test_json_stream_reader_rejects_a_callback_that_overreports)
{
    json_stream_reader reader{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_stream_reader_init_callback(&reader,
        json_overreporting_source, ft_nullptr, 32U));
    const int32_t result = json_stream_reader_traverse(&reader, ft_nullptr, ft_nullptr);
    const int32_t error = reader.error_code;
    json_stream_reader_destroy(&reader);
    FT_ASSERT(result != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_IO, error);
    return (1);
}
