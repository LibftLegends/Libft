#include "../../../Modules/JSon/json.hpp"
#include "../../../Modules/JSon/json_stream_writer.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

static int32_t measure_read_groups()
{
    constexpr uint32_t ITEM_COUNT = 2500U;
    constexpr uint32_t ITERATIONS = 5U;
    const std::string value(64U, 'v');
    std::string document = "{\"group\":{";
    uint32_t item = 0U;
    while (item < ITEM_COUNT)
    {
        if (item != 0U)
            document += ',';
        document += "\"field-" + std::to_string(item);
        if (item % 2U == 0U)
            document += "\":\"" + value + '"';
        else
            document += "\":-123.45e2";
        ++item;
    }
    document += "}}";
    using group_owner = std::unique_ptr<json_group, decltype(&json_free_groups)>;
    uint32_t iteration = 0U;
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    if (cma_get_stats(&allocations_before, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const group_owner groups(json_read_from_string(document.c_str()), json_free_groups);
        if (!groups)
            return (1);
        uint32_t count = 0U;
        const json_item *entry = groups->items;
        while (entry != ft_nullptr)
        {
            ++count;
            entry = entry->next;
        }
        if (count != ITEM_COUNT)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (cma_get_stats(&allocations_after, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    std::printf("json_read_and_destroy allocations/op=%" FT_PRIu64 " ms/op=%.3f\n",
        static_cast<uint64_t>((allocations_after - allocations_before) / ITERATIONS),
        std::chrono::duration<double, std::milli>(elapsed).count() / ITERATIONS);
    return (0);
}

struct json_benchmark_sink
{
    ft_size_t bytes;
    ft_size_t calls;
};

static ft_size_t count_output(void *user_data, const char *, ft_size_t size)
{
    json_benchmark_sink *sink = static_cast<json_benchmark_sink *>(user_data);
    sink->bytes += size;
    ++sink->calls;
    return (size);
}

static int32_t measure_write_string()
{
    constexpr uint32_t ITERATIONS = 1000U;
    const std::string prefix(8192U, 'v');
    const std::string value = prefix + "\n" + prefix;
    json_benchmark_sink sink{};
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        json_stream_writer writer{};
        json_stream_event event{};
        if (json_stream_writer_init(&writer, count_output, &sink) != FT_ERR_SUCCESS)
            return (1);
        event.type = JSON_STREAM_EVENT_BEGIN_DOCUMENT;
        if (json_stream_writer_process(&writer, &event) != FT_ERR_SUCCESS)
            return (1);
        event.type = JSON_STREAM_EVENT_STRING;
        event.value = {value.data(), value.size()};
        if (json_stream_writer_process(&writer, &event) != FT_ERR_SUCCESS)
            return (1);
        event.type = JSON_STREAM_EVENT_END_DOCUMENT;
        if (json_stream_writer_process(&writer, &event) != FT_ERR_SUCCESS)
            return (1);
        json_stream_writer_destroy(&writer);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (sink.bytes != ITERATIONS * (value.size() + 3U))
        return (1);
    std::printf("json_stream_write calls/op=%u ms/op=%.4f\n",
        static_cast<uint32_t>(sink.calls / ITERATIONS),
        std::chrono::duration<double, std::milli>(elapsed).count() / ITERATIONS);
    return (0);
}

int32_t main()
{
    if (measure_read_groups() != 0)
        return (1);
    return (measure_write_string());
}
