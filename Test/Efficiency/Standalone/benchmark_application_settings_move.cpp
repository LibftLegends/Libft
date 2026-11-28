#include "../../../Modules/Application/application_settings.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr ft_size_t MOVE_PAIRS = 50000;
    application_settings source;
    application_settings destination;
    if (source.initialize("profile", "data/users.json", "benchmark-placeholder",
            FT_TRUE) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    ft_size_t index = 0;
    while (index < MOVE_PAIRS)
    {
        if (destination.move(source) != FT_ERR_SUCCESS
            || source.move(destination) != FT_ERR_SUCCESS)
            return (2);
        ++index;
    }
    const std::chrono::duration<double, std::nano> elapsed =
        std::chrono::steady_clock::now() - start;
    ft_string path;
    if (source.get_database_relative_path(path) != FT_ERR_SUCCESS || path != "data/users.json")
        return (3);
    std::printf("%.3f ns/move\n", elapsed.count() / (2 * MOVE_PAIRS));
    return (0);
}
