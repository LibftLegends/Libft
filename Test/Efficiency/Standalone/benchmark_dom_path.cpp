#include "../../../Modules/Parser/dom.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>
#include <inttypes.h>
#include <new>

int main()
{
    constexpr ft_size_t ITERATIONS = 25000;
    ft_dom_node root;
    ft_string path;
    if (root.initialize() != FT_ERR_SUCCESS || path.initialize("child/child/child/child") != FT_ERR_SUCCESS)
        return (1);
    ft_dom_node *leaf = &root;
    ft_size_t depth = 0;
    while (depth < 4)
    {
        ft_dom_node *child = new (std::nothrow) ft_dom_node();
        if (!child || child->initialize() != FT_ERR_SUCCESS || child->set_name("child") != FT_ERR_SUCCESS
            || leaf->add_child(child) != FT_ERR_SUCCESS)
        {
            delete child;
            return (1);
        }
        leaf = child;
        ++depth;
    }

    ft_size_t allocations_before = 0;
    ft_size_t allocations_after = 0;
    if (cma_get_stats(&allocations_before, nullptr) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    ft_size_t index = 0;
    while (index < ITERATIONS)
    {
        const ft_dom_node *found = nullptr;
        if (ft_dom_find_path(&root, path, &found) != FT_ERR_SUCCESS || found != leaf)
            return (1);
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (cma_get_stats(&allocations_after, nullptr) != FT_ERR_SUCCESS)
        return (1);
    std::printf("lookup=%.3f us allocations=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS,
        static_cast<uint64_t>((allocations_after - allocations_before) / ITERATIONS));
    return (0);
}
