#include "../../../Modules/CPP_class/class_istringstream.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr ft_size_t PAYLOAD_SIZE = 8192;
    constexpr ft_size_t ITERATIONS = 1024;
    ft_string source;
    ft_string extracted;
    if (source.initialize(PAYLOAD_SIZE, 'x') != FT_ERR_SUCCESS)
        return (1);
    ft_size_t checksum = 0;
    double read_ms = 0;
    double extraction_ms = 0;
    ft_size_t index = 0;
    while (index < ITERATIONS)
    {
        ft_istringstream stream;
        if (stream.initialize(source) != FT_ERR_SUCCESS)
            return (2);
        const auto extract_start = std::chrono::steady_clock::now();
        stream >> extracted;
        const auto read_start = std::chrono::steady_clock::now();
        char bytes[PAYLOAD_SIZE];
        stream.read(bytes, sizeof(bytes));
        const auto read_end = std::chrono::steady_clock::now();
        if (extracted.size() != PAYLOAD_SIZE || stream.gcount() != PAYLOAD_SIZE)
            return (3);
        checksum += extracted.size() + bytes[0] + bytes[PAYLOAD_SIZE - 1];
        extraction_ms += std::chrono::duration<double, std::milli>(
            read_start - extract_start).count();
        read_ms += std::chrono::duration<double, std::milli>(read_end - read_start).count();
        ++index;
    }
    std::printf("checksum %" PRIu64 " extract %.3f read %.3f ms\n",
        checksum, extraction_ms, read_ms);
    return (checksum != ITERATIONS * (PAYLOAD_SIZE + 2 * 'x'));
}
