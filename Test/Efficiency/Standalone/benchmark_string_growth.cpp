#include "../../../Modules/CPP_class/class_string.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr ft_size_t TEXT_SIZE = 128U * 1024U;
    constexpr ft_size_t ITERATIONS = 256;
    ft_string source;
    ft_string destination;
    if (source.initialize(TEXT_SIZE, 'x') != FT_ERR_SUCCESS
        || destination.initialize(TEXT_SIZE * 3, 'y') != FT_ERR_SUCCESS)
        return (1);
    ft_size_t checksum = 0;
    ft_size_t index = 0;
    const auto append_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (destination.clear() != FT_ERR_SUCCESS
            || destination.append(source) != FT_ERR_SUCCESS
            || destination.append(destination) != FT_ERR_SUCCESS)
            return (2);
        checksum += destination.size() + destination.c_str()[2 * TEXT_SIZE - 1];
        ++index;
    }
    const auto fill_start = std::chrono::steady_clock::now();
    while (index > 0)
    {
        if (destination.assign(TEXT_SIZE, 'z') != FT_ERR_SUCCESS)
            return (3);
        checksum += destination.size() + destination.c_str()[TEXT_SIZE - 1];
        --index;
    }
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> append_time = fill_start - append_start;
    const std::chrono::duration<double, std::milli> fill_time = end - fill_start;
    std::printf("checksum %" PRIu64 " append %.3f fill %.3f ms\n",
        checksum, append_time.count(), fill_time.count());
    return (checksum != ITERATIONS * (3 * TEXT_SIZE + 'x' + 'z'));
}
