#include "../../../Modules/API/api_http_common.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static int32_t measure_line_search()
{
    constexpr uint32_t ITERATIONS = 20000U;
    std::string header(8192U, 'x');
    header += "\r\n";
    const char *const begin = header.data();
    const char *const end = begin + header.size();
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        if (api_http_find_crlf(begin, end) != end - 2)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("api_http_find_crlf us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_chunk_decode()
{
    constexpr uint32_t ITERATIONS = 40U;
    constexpr uint32_t CHUNKS = 16U;
    constexpr uint32_t CHUNK_SIZE = 4096U;
    const std::string payload(CHUNK_SIZE, 'x');
    std::string encoded;
    uint32_t index = 0U;
    while (index < CHUNKS)
    {
        encoded += "1000\r\n";
        encoded += payload;
        encoded += "\r\n";
        ++index;
    }
    encoded += "0\r\n\r\n";
    ft_string decoded;
    if (decoded.initialize() != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        ft_size_t consumed = 0U;
        if (!api_http_decode_chunked(encoded.data(), encoded.size(), decoded, consumed)
            || consumed != encoded.size() || decoded.size() != CHUNKS * CHUNK_SIZE)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("api_http_decode_chunked us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

struct api_http_benchmark_sink
{
    ft_string output;
    uint32_t calls;
};

static ft_bool append_payload(const char *data, ft_size_t length, void *context, int32_t &error_code)
{
    auto &sink = *static_cast<api_http_benchmark_sink *>(context);
    ++sink.calls;
    error_code = sink.output.append(data, length);
    return (error_code == FT_ERR_SUCCESS);
}

static int32_t measure_json_stream()
{
    constexpr uint32_t ITEM_COUNT = 128U;
    constexpr uint32_t ITERATIONS = 2000U;
    char group_name[] = "request";
    char key[] = "field";
    char value[] = "payload-value-0123456789-abcdefghijklmnopqrstuvwxyz";
    json_item items[ITEM_COUNT]{};
    uint32_t index = 0;
    while (index < ITEM_COUNT)
    {
        items[index].key = key;
        items[index].value = value;
        if (index + 1U < ITEM_COUNT)
            items[index].next = &items[index + 1U];
        ++index;
    }
    json_group payload{};
    payload.name = group_name;
    payload.items = items;
    api_http_benchmark_sink sink{};
    if (sink.output.initialize() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t measured = 0;
    int32_t error_code = FT_ERR_SUCCESS;
    const auto start = std::chrono::steady_clock::now();
    index = 0U;
    while (index < ITERATIONS)
    {
        if (sink.output.clear() != FT_ERR_SUCCESS || !api_http_measure_json_payload(&payload, measured)
            || !api_http_stream_json_payload(&payload, append_payload, &sink, error_code)
            || sink.output.size() != measured)
            return (1);
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("api_http_json_stream us/op=%.3f calls/op=%u\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS,
        sink.calls / ITERATIONS);
    return (0);
}

int main()
{
    if (measure_line_search() != 0 || measure_chunk_decode() != 0 || measure_json_stream() != 0)
        return (1);
    return (0);
}
