#include "../../../Modules/JSon/document.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static int32_t measure_updates(json_document &document, json_group *group,
    const char *key, const char *first, const char *second, const char *name)
{
    constexpr uint32_t ITERATIONS = 10000U;
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    if (cma_get_stats(&allocations_before, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const char *value = first;
        if (iteration % 2U != 0U)
            value = second;
        document.update_item(group, key, value);
        if (document.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (cma_get_stats(&allocations_after, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    const json_item *item = document.find_item(group, key);
    if (item == ft_nullptr || ft_strcmp(item->value, second) != 0)
        return (1);
    std::printf("%s allocations=%" FT_PRIu64 " us/op=%.3f\n", name,
        static_cast<uint64_t>(allocations_after - allocations_before),
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

int32_t main()
{
    json_document document;
    const std::string original(128U, 'a');
    const std::string changed(128U, 'b');
    if (document.initialize() != FT_ERR_SUCCESS)
        return (1);
    json_group *group = document.create_group("settings");
    if (group == ft_nullptr)
        return (1);
    document.append_group(group);
    uint32_t index = 0U;
    while (index < 127U)
    {
        const std::string key = "field-" + std::to_string(index);
        json_item *item = document.create_item(key.c_str(), "unused");
        if (item == ft_nullptr)
            return (1);
        document.add_item(group, item);
        ++index;
    }
    json_item *target = document.create_item("target", original.c_str());
    if (target == ft_nullptr)
        return (1);
    document.add_item(group, target);
    if (measure_updates(document, group, "target", original.c_str(), original.c_str(),
            "unchanged_json_update") != 0)
        return (1);
    return (measure_updates(document, group, "target", original.c_str(), changed.c_str(),
        "changed_json_update"));
}
