#include "../../../Modules/CPP_class/class_data_buffer.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr ft_size_t PAYLOAD_SIZE = 8192;
    constexpr ft_size_t VALUE_COUNT = 512;
    DataBuffer source;
    DataBuffer destination;
    ft_string payload;
    ft_string decoded;
    if (source.initialize() != FT_ERR_SUCCESS
        || payload.initialize(PAYLOAD_SIZE, 'x') != FT_ERR_SUCCESS)
        return (1);

    const auto write_start = std::chrono::steady_clock::now();
    ft_size_t index = 0;
    while (index < VALUE_COUNT)
    {
        if ((source << payload).get_error() != FT_ERR_SUCCESS)
            return (2);
        ++index;
    }
    const auto copy_start = std::chrono::steady_clock::now();
    if (destination.initialize(source) != FT_ERR_SUCCESS)
        return (3);
    const auto read_start = std::chrono::steady_clock::now();
    ft_size_t checksum = 0;
    while (index > 0)
    {
        if ((destination >> decoded).get_error() != FT_ERR_SUCCESS
            || decoded.size() != PAYLOAD_SIZE)
            return (4);
        checksum += decoded.size() + decoded.c_str()[0];
        --index;
    }
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> write_time = copy_start - write_start;
    const std::chrono::duration<double, std::milli> copy_time = read_start - copy_start;
    const std::chrono::duration<double, std::milli> read_time = end - read_start;
    std::printf("checksum %" PRIu64 " write %.3f copy %.3f read %.3f ms\n",
        checksum, write_time.count(), copy_time.count(), read_time.count());
    return (checksum != VALUE_COUNT * (PAYLOAD_SIZE + 'x'));
}
