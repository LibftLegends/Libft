#include "../../../Modules/CPP_class/class_string.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr ft_size_t TEXT_SIZE = 128U * 1024U;
    constexpr ft_size_t ITERATIONS = 256;
    constexpr ft_size_t SPARE_SIZE = 512U * 1024U;
    ft_string text;
    ft_string reserved;
    if (text.initialize(TEXT_SIZE, 'x') != FT_ERR_SUCCESS
        || reserved.initialize(SPARE_SIZE, 'y') != FT_ERR_SUCCESS
        || reserved.resize_length(16) != FT_ERR_SUCCESS)
        return (1);
    ft_size_t checksum = 0;
    ft_size_t index = 0;
    const auto literal_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        ft_string copy;
        if (copy.initialize(text.c_str()) != FT_ERR_SUCCESS
            || copy.size() != TEXT_SIZE)
            return (2);
        checksum += copy.size() + copy.c_str()[TEXT_SIZE - 1];
        ++index;
    }
    const auto copy_start = std::chrono::steady_clock::now();
    while (index > 0)
    {
        ft_string copy;
        if (copy.initialize(reserved) != FT_ERR_SUCCESS || copy.size() != 16)
            return (3);
        checksum += copy.size() + copy.c_str()[15];
        --index;
    }
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> literal_time = copy_start - literal_start;
    const std::chrono::duration<double, std::milli> copy_time = end - copy_start;
    std::printf("checksum %" PRIu64 " text %.3f copy %.3f ms\n",
        checksum, literal_time.count(), copy_time.count());
    return (checksum != ITERATIONS * (TEXT_SIZE + 'x' + 16 + 'y'));
}
