#include "../../../Modules/CPP_class/class_fd_istream.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t benchmark_moves(ft_bool with_base_mutex)
{
    constexpr ft_size_t MOVE_COUNT = 50000;
    constexpr int32_t DESCRIPTOR = 17;
    ft_fd_istream first;
    ft_fd_istream second;
    if (first.initialize() != FT_ERR_SUCCESS
        || first.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    if (with_base_mutex == FT_TRUE
        && first.ft_istream::enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    first.set_file_descriptor(DESCRIPTOR);

    ft_size_t index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < MOVE_COUNT)
    {
        if (second.move(first) != FT_ERR_SUCCESS || first.move(second) != FT_ERR_SUCCESS)
            return (2);
        ++index;
    }
    const std::chrono::duration<double, std::nano> elapsed =
        std::chrono::steady_clock::now() - start;
    const int32_t descriptor = first.get_file_descriptor();
    if (descriptor != DESCRIPTOR || first.is_thread_safe() != FT_TRUE
        || first.ft_istream::is_thread_safe() != with_base_mutex)
        return (3);
    std::printf("base-%" PRIu32 " descriptor %" PRId32 " %.3f ns/move\n",
        static_cast<uint32_t>(with_base_mutex), descriptor, elapsed.count() / (2 * MOVE_COUNT));
    return (0);
}

int main()
{
    if (benchmark_moves(FT_FALSE) != 0)
        return (1);
    return (benchmark_moves(FT_TRUE));
}
