#include "../../../Modules/XML/xml.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>
#include <inttypes.h>
#include <string>

int main()
{
    constexpr ft_size_t ITERATIONS = 128;
    const std::string payload(40000, 'x');
    const std::string input = "<root><child>" + payload + "</child></root>";
    const std::string expected = input + '\n';
    xml_document document;
    ft_string output;
    ft_memory_document_sink sink;
    if (document.initialize() != FT_ERR_SUCCESS
        || document.load_from_string(input.c_str()) != FT_ERR_SUCCESS
        || output.initialize() != FT_ERR_SUCCESS || sink.initialize() != FT_ERR_SUCCESS)
        return (1);
    sink.set_storage(&output);
    if (document.write_to_backend(sink) != FT_ERR_SUCCESS || expected != output.c_str())
        return (1);

    ft_size_t allocations_before = 0;
    ft_size_t allocations_after = 0;
    if (cma_get_stats(&allocations_before, nullptr) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    ft_size_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        if (document.write_to_backend(sink) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (cma_get_stats(&allocations_after, nullptr) != FT_ERR_SUCCESS
        || expected != output.c_str())
        return (1);
    std::printf("backend=%.3f us allocations=%" PRIu64 " bytes=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS,
        static_cast<uint64_t>((allocations_after - allocations_before) / ITERATIONS),
        static_cast<uint64_t>(output.size()));
    return (0);
}
