#include "../../../Modules/XML/xml.hpp"
#include <chrono>
#include <cstdio>
#include <inttypes.h>
#include <string>

int main()
{
    constexpr ft_size_t ITERATIONS = 128;
    const std::string text(512 * 1024, 't');
    const std::string attribute(32768, 'a');
    const std::string input = "<p:root xmlns:p='urn:test' attribute='" + attribute
        + "'><p:entry>" + text + "</p:entry></p:root>";
    xml_document document;
    if (document.initialize() != FT_ERR_SUCCESS
        || document.load_from_string(input.c_str()) != FT_ERR_SUCCESS)
        return (1);
    ft_size_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (document.load_from_string(input.c_str()) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const xml_node *root = document.get_root();
    if (!root || root->children.size() != 1 || text != root->children[0]->text)
        return (1);
    std::printf("parse=%.3f us bytes=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS,
        static_cast<uint64_t>(input.size()));
    return (0);
}
