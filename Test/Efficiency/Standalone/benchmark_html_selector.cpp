#include "../../../Modules/HTML/html_parser.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_selector(const char *label, html_node *root, html_node *expected, uint32_t iterations)
{
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0;
    while (iteration < iterations)
    {
        if (html_find_by_selector(root, "[data-id=\"target\"]") != expected)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("%s %.3f us/lookup\n", label,
        std::chrono::duration<double, std::micro>(elapsed).count() / iterations);
    return (0);
}

int main()
{
    constexpr uint32_t NODE_COUNT = 1024;
    char tag[] = "div";
    char key[] = "data-id";
    char partial[] = "target-other";
    char target[] = "target";
    html_attr partial_attribute{key, partial, nullptr};
    html_attr target_attribute{key, target, nullptr};
    html_node nodes[NODE_COUNT]{};
    uint32_t index = 0;
    while (index < NODE_COUNT - 1)
    {
        nodes[index] = {tag, nullptr, &partial_attribute, nullptr, &nodes[index + 1]};
        ++index;
    }
    nodes[index] = {tag, nullptr, &target_attribute, nullptr, nullptr};
    if (measure_selector("single node", &nodes[index], &nodes[index], 50000) != 0)
        return (1);
    return (measure_selector("1024 siblings", nodes, &nodes[index], 3000));
}
