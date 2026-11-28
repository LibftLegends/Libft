#include "../../../Modules/Template/container_serialization.hpp"
#include <chrono>
#include <cstdio>

static int32_t benchmark_json_vector(ft_size_t length, uint32_t rounds)
{
    using benchmark_clock = std::chrono::steady_clock;
    ft_vector<uint32_t> values;
    if (values.initialize() != FT_ERR_SUCCESS)
        return (1);
    values.reserve(length);
    if (values.get_error() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < length)
    {
        if (values.push_back(index) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    benchmark_clock::duration elapsed{};
    uint32_t iteration = 0U;
    while (iteration < rounds)
    {
        json_group *group = ft_nullptr;
        const auto start = benchmark_clock::now();
        const int32_t result = ft_vector_serialize_json(values, "values", group);
        elapsed += benchmark_clock::now() - start;
        if (result != FT_ERR_SUCCESS || group == ft_nullptr)
            return (1);
        const json_item *item = group->items;
        ft_size_t item_count = 0U;
        while (item != ft_nullptr)
        {
            ++item_count;
            item = item->next;
        }
        json_free_groups(group);
        if (item_count != length + 1U)
            return (1);
        ++iteration;
    }
    std::printf("json_vector length=%" FT_PRIu64 " ns/op=%.2f\n", length,
        std::chrono::duration<double, std::nano>(elapsed).count() / rounds);
    return (0);
}

int main()
{
    if (benchmark_json_vector(256U, 40U) != 0)
        return (1);
    return (benchmark_json_vector(8192U, 5U));
}
