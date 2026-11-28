#include "../../../Modules/XML/xml.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static int32_t copy_through_text(xml_document &destination, const xml_document &source)
{
    char *serialized = source.write_to_string();
    if (!serialized)
        return (source.get_error());
    const int32_t status = destination.load_from_string(serialized);
    cma_free(serialized);
    return (status);
}

static double measure_copy(const xml_document &source, ft_bool direct)
{
    constexpr ft_size_t ITERATIONS = 32;
    xml_document destination;
    if (destination.initialize() != FT_ERR_SUCCESS)
        return (-1.0);
    const auto start = std::chrono::steady_clock::now();
    ft_size_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        int32_t status;
        if (direct)
            status = destination.initialize(source);
        else
            status = copy_through_text(destination, source);
        if (status != FT_ERR_SUCCESS)
            return (-1.0);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const xml_node *root = destination.get_root();
    if (!root || root->children.size() != source.get_root()->children.size())
        return (-1.0);
    return (std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
}

int main(int argument_count, char **arguments)
{
    if (argument_count != 2)
        return (1);
    const std::string variant = arguments[1];
    if (variant != "direct" && variant != "roundtrip")
        return (1);
    const std::string text(4096, 't');
    std::string input = "<root>";
    ft_size_t index = 0;
    while (index < 128)
    {
        input += "<entry>" + text + "</entry>";
        ++index;
    }
    input += "</root>";
    xml_document source;
    if (source.initialize() != FT_ERR_SUCCESS
        || source.load_from_string(input.c_str()) != FT_ERR_SUCCESS)
        return (1);
    const double elapsed = measure_copy(source, variant == "direct");
    if (elapsed < 0.0)
        return (1);
    std::printf("%s=%.3f us\n", variant.c_str(), elapsed);
    return (0);
}
