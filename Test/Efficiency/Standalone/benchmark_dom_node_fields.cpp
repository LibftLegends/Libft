#include "../../../Modules/Parser/dom.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>
#include <inttypes.h>

int main()
{
    constexpr ft_size_t ITERATIONS = 20000;
    const char *text = "The value fits in the existing DOM string storage.";
    ft_dom_node node;
    if (node.initialize() != FT_ERR_SUCCESS || node.set_name("entry") != FT_ERR_SUCCESS
        || node.set_value(text) != FT_ERR_SUCCESS || node.add_attribute("data", text) != FT_ERR_SUCCESS)
        return (1);

    ft_size_t allocations_before = 0;
    ft_size_t allocations_after = 0;
    if (cma_get_stats(&allocations_before, nullptr) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    ft_size_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        if (node.set_name("entry") != FT_ERR_SUCCESS
            || node.set_value(text) != FT_ERR_SUCCESS
            || node.add_attribute("data", text) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (cma_get_stats(&allocations_after, nullptr) != FT_ERR_SUCCESS
        || node.get_attribute_values().size() != 1 || node.get_attribute_values()[0] != text)
        return (1);
    std::printf("update=%.3f us allocations=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS,
        static_cast<uint64_t>((allocations_after - allocations_before) / ITERATIONS));
    return (0);
}
