#include "../../../Modules/Sink/sink.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t RECORD_COUNT = 16384U;
    sink_clear();
    uint32_t index = 0U;
    const auto start_append = std::chrono::steady_clock::now();
    while (index < RECORD_COUNT)
    {
        if (sink_record_message(static_cast<int32_t>(index), "representative log message") != FT_ERR_SUCCESS)
        {
            sink_clear();
            return (1);
        }
        index += 1U;
    }
    const auto start_read = std::chrono::steady_clock::now();
    uint64_t checksum = 0U;
    index = 0U;
    while (index < RECORD_COUNT)
    {
        sink_record record;
        if (sink_get_record(index, &record) != FT_ERR_SUCCESS)
        {
            sink_clear();
            return (1);
        }
        checksum += static_cast<uint32_t>(record.level) + static_cast<uint8_t>(record.message[0]);
        index += 1U;
    }
    const auto end_read = std::chrono::steady_clock::now();
    sink_clear();
    const double append_ns = std::chrono::duration<double, std::nano>(start_read - start_append).count();
    const double read_ns = std::chrono::duration<double, std::nano>(end_read - start_read).count();
    std::printf("append %.2f ns/op, index %.2f ns/op, checksum %" PRIu64 "\n",
        append_ns / RECORD_COUNT, read_ns / RECORD_COUNT, checksum);
    return (0);
}
