#include "../../../Modules/JSon/document.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static int32_t measure_pointer_lookup(const json_document &document, const char *path,
    const char *label)
{
    constexpr uint32_t ITERATIONS = 100000U;
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    if (cma_get_stats(&allocations_before, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0U;
    while (index < ITERATIONS)
    {
        const char *value = document.get_value_by_pointer(path);
        if (value == ft_nullptr || value[0] != 'v')
            return (1);
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (cma_get_stats(&allocations_after, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%s allocations=%" FT_PRIu64 " us/op=%.3f\n", label,
        static_cast<uint64_t>(allocations_after - allocations_before),
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_document_copy(const json_document &document)
{
    constexpr uint32_t ITERATIONS = 64U;
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    if (cma_get_stats(&allocations_before, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0U;
    while (index < ITERATIONS)
    {
        json_document copy;
        if (copy.initialize(document) != FT_ERR_SUCCESS || copy.get_groups() == ft_nullptr)
            return (1);
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (cma_get_stats(&allocations_after, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    std::printf("json_document_copy allocations=%" FT_PRIu64 " us/op=%.3f\n",
        static_cast<uint64_t>(allocations_after - allocations_before),
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

int32_t main()
{
    std::string text = "{\"config\":{\"plain\":\"value\",\"escaped/name\":\"value\"";
    uint32_t index = 0U;
    while (index < 128U)
    {
        text += ",\"field" + std::to_string(index) + "\":\"value-"
            + std::to_string(index) + '"';
        ++index;
    }
    text += "}}";
    json_document document;
    if (document.initialize() != FT_ERR_SUCCESS
        || document.read_from_string(text.c_str()) != FT_ERR_SUCCESS)
        return (1);
    if (measure_pointer_lookup(document, "/config/plain", "json_pointer_plain") != 0
        || measure_pointer_lookup(document, "/config/escaped~1name", "json_pointer_escaped") != 0)
        return (1);
    return (measure_document_copy(document));
}
