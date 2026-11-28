#include "../../../Modules/CPP_class/class_string.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr ft_size_t PAYLOAD_SIZE = 128U * 1024U;
    constexpr ft_size_t ITERATIONS = 256;
    ft_string text;
    ft_string equal;
    ft_string needle;
    ft_string output;
    if (text.initialize(PAYLOAD_SIZE, 'x') != FT_ERR_SUCCESS
        || text.append("xyz") != FT_ERR_SUCCESS
        || equal.initialize(text) != FT_ERR_SUCCESS
        || needle.initialize("xyz") != FT_ERR_SUCCESS)
        return (1);
    ft_size_t checksum = 0;
    ft_size_t index = 0;
    const auto search_start = std::chrono::steady_clock::now();
    while (index++ < ITERATIONS)
        checksum += text.find(needle);
    const auto compare_start = std::chrono::steady_clock::now();
    index = 0;
    while (index++ < ITERATIONS)
        checksum += (text == equal);
    const auto chain_start = std::chrono::steady_clock::now();
    index = 0;
    while (index++ < ITERATIONS)
    {
        output = (text + "-").operator+(needle).operator+('!');
        if (output.get_error() != FT_ERR_SUCCESS)
            return (2);
        checksum += output.size() + output.back();
    }
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> search = compare_start - search_start;
    const std::chrono::duration<double, std::milli> compare = chain_start - compare_start;
    const std::chrono::duration<double, std::milli> chain = end - chain_start;
    std::printf("checksum %" PRIu64 " search %.3f compare %.3f chain %.3f ms\n",
        checksum, search.count(), compare.count(), chain.count());
    return (checksum != ITERATIONS * (2 * PAYLOAD_SIZE + 9 + '!'));
}
