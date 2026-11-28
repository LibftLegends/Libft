#include "../../../Modules/CPP_class/class_ofstream.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr ft_size_t MOVE_PAIRS = 50000;
    ft_ofstream first;
    ft_ofstream second;
    if (first.initialize() != FT_ERR_SUCCESS
        || first.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    ft_size_t index = 0;
    while (index < MOVE_PAIRS)
    {
        if (second.move(first) != FT_ERR_SUCCESS || first.move(second) != FT_ERR_SUCCESS)
            return (2);
        ++index;
    }
    const std::chrono::duration<double, std::nano> elapsed =
        std::chrono::steady_clock::now() - start;
    if (first.is_thread_safe() != FT_TRUE)
        return (3);
    std::printf("%.3f ns/move\n", elapsed.count() / (2 * MOVE_PAIRS));
    return (0);
}
