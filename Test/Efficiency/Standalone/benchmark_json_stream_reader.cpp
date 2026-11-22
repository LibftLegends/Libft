#include "../../../Modules/JSon/json_stream_reader.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cstdio>
#include <string>

struct json_stream_fixture
{
    const std::string *document;
    ft_size_t offset;
    ft_size_t string_bytes;
    ft_size_t number_bytes;
};

static ft_size_t read_json_fixture(void *user_data, char *buffer, ft_size_t capacity)
{
    json_stream_fixture *fixture = static_cast<json_stream_fixture *>(user_data);
    ft_size_t length = fixture->document->size() - fixture->offset;
    if (length > capacity)
        length = capacity;
    ft_memcpy(buffer, fixture->document->data() + fixture->offset, length);
    fixture->offset += length;
    return (length);
}

static int32_t count_scalar_bytes(void *user_data, const json_stream_event *event)
{
    json_stream_fixture *fixture = static_cast<json_stream_fixture *>(user_data);
    if (event->type == JSON_STREAM_EVENT_STRING)
        fixture->string_bytes += event->value.length;
    if (event->type == JSON_STREAM_EVENT_NUMBER)
        fixture->number_bytes += event->value.length;
    return (FT_ERR_SUCCESS);
}

static int32_t measure_stream_read(const char *name, ft_size_t text_length,
    ft_size_t whitespace_length, ft_size_t digit_count, uint32_t iterations)
{
    const std::string text(text_length, 'v');
    const std::string whitespace(whitespace_length, ' ');
    const std::string digits(digit_count, '7');
    const std::string document = "[\"" + text + "\\n" + text + "\"," + whitespace
        + "-" + digits + ".125e+10]";
    json_stream_fixture fixture{&document, 0U, 0U, 0U};
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        fixture.offset = 0U;
        const int32_t result = json_stream_read_from_stream_events(read_json_fixture, &fixture,
            4096U, count_scalar_bytes, &fixture);
        if (result != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (fixture.string_bytes != iterations * (text.size() * 2U + 1U)
        || fixture.number_bytes != iterations * (digits.size() + 9U))
        return (1);
    std::printf("%s us/op=%.3f\n", name,
        std::chrono::duration<double, std::micro>(elapsed).count() / iterations);
    return (0);
}

int32_t main()
{
    if (measure_stream_read("short_stream_read", 12U, 8U, 4U, 24000U) != 0)
        return (1);
    return (measure_stream_read("long_stream_read", 65536U, 8192U, 16384U, 240U));
}
