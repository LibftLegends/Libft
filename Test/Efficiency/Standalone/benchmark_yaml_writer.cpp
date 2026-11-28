#include "../../../Modules/YAML/yaml.hpp"
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

int main()
{
    constexpr ft_size_t ITERATIONS = 32;
    std::string text;
    ft_size_t index = 0;
    while (index < 512)
    {
        text += "key_" + std::to_string(index) + ": value\n";
        ++index;
    }
    ft_string input;
    ft_string output;
    ft_memory_document_sink sink;
    if (input.initialize(text.c_str()) != FT_ERR_SUCCESS || output.initialize() != FT_ERR_SUCCESS
        || sink.initialize() != FT_ERR_SUCCESS)
        return (1);
    const std::unique_ptr<yaml_value> root(yaml_read_from_string(input));
    if (!root)
        return (1);
    sink.set_storage(&output);
    const auto start = std::chrono::steady_clock::now();
    index = 0;
    while (index < ITERATIONS)
    {
        if (yaml_write_to_backend(sink, root.get()) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (text != output.c_str())
        return (1);
    std::printf("write=%.3f us\n", std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}
