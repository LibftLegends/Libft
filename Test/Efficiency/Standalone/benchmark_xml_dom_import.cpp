#include "../../../Modules/XML/xml_dom_bridge.hpp"
#include <chrono>
#include <cstdio>
#include <string>

int main()
{
    constexpr ft_size_t CHILD_COUNT = 16;
    constexpr ft_size_t ITERATIONS = 64;
    const std::string text(32 * 1024, 't');
    std::string input = "<root>";
    ft_size_t index = 0;
    while (index < CHILD_COUNT)
    {
        input += "<entry attr='value'>" + text + "</entry>";
        ++index;
    }
    input += "</root>";
    xml_document source;
    xml_document destination;
    ft_dom_document dom;
    if (source.initialize() != FT_ERR_SUCCESS || destination.initialize() != FT_ERR_SUCCESS
        || dom.initialize() != FT_ERR_SUCCESS || source.load_from_string(input.c_str()) != FT_ERR_SUCCESS
        || xml_document_to_dom(source, dom) != FT_ERR_SUCCESS
        || xml_document_from_dom(dom, destination) != FT_ERR_SUCCESS)
        return (1);

    const auto start = std::chrono::steady_clock::now();
    index = 0;
    while (index < ITERATIONS)
    {
        if (xml_document_from_dom(dom, destination) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const xml_node *root = destination.get_root();
    if (!root || root->children.size() != CHILD_COUNT || text != root->children[0]->text)
        return (1);
    std::printf("import=%.3f us\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}
