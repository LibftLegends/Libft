#include "../../../Modules/Template/unordered_map.hpp"
#include <chrono>
#include <cstdio>

struct s_benchmark_collision_key
{
    uint32_t value;

    ft_bool operator==(const s_benchmark_collision_key &other) const noexcept;
};

ft_bool s_benchmark_collision_key::operator==(const s_benchmark_collision_key &other) const noexcept
{
    return (this->value == other.value);
}

template <>
struct std::hash<s_benchmark_collision_key>
{
    std::size_t operator()(const s_benchmark_collision_key &) const noexcept;
};

std::size_t std::hash<s_benchmark_collision_key>::operator()(const s_benchmark_collision_key &) const noexcept
{
    return (4094U);
}

int32_t main()
{
    constexpr uint32_t LENGTH = 1024U;
    constexpr uint32_t ITERATIONS = 128U;
    ft_unordered_map<s_benchmark_collision_key, uint32_t> values(4096U);
    if (values.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < LENGTH)
    {
        values.insert({index}, index);
        ++index;
    }
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        values.erase({iteration});
        values.insert({iteration}, iteration);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    auto sample = values.find({0U});
    if (values.size() != LENGTH || sample == values.end() || sample->second != 0U)
        return (1);
    std::printf("unordered_map_cluster_erase_insert ns/pair=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}
