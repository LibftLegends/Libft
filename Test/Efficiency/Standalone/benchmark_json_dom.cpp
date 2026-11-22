#include "../../../Modules/JSon/json_dom_bridge.hpp"
#include "../../../Modules/CPP_class/class_big_number.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

static int32_t measure_decimal()
{
    constexpr uint32_t ITERATIONS = 40U;
    constexpr char VALUE[] = "-922337203685477580812345";
    ft_big_number number;
    if (number.initialize() != FT_ERR_SUCCESS)
        return (1);
    number.assign(VALUE);
    ft_size_t before;
    ft_size_t after;
    if (cma_get_stats(&before, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const std::unique_ptr<ft_string> text(number.to_string_base(10));
        if (!text || *text != VALUE)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (cma_get_stats(&after, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    std::printf("big_number_decimal allocations=%" FT_PRIu64 " us/op=%.3f\n",
        static_cast<uint64_t>(after - before),
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_bridge()
{
    constexpr uint32_t ITERATIONS = 32U;
    std::string text = "{\"sample\":{\"number\":18446744073709551616";
    uint32_t item = 0U;
    while (item < 192U)
    {
        text += ",\"item" + std::to_string(item) + "\":\"text\"";
        ++item;
    }
    text += "},\"empty\":{}}";
    json_document source;
    if (source.initialize() != FT_ERR_SUCCESS || source.read_from_string(text.c_str()) != FT_ERR_SUCCESS)
        return (1);
    ft_size_t before;
    ft_size_t after;
    if (cma_get_stats(&before, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_dom_document dom;
        json_document output;
        if (dom.initialize() != FT_ERR_SUCCESS || output.initialize() != FT_ERR_SUCCESS
            || json_document_to_dom(source, dom) != FT_ERR_SUCCESS
            || json_document_from_dom(dom, output) != FT_ERR_SUCCESS
            || ft_strcmp(output.get_value_by_pointer("/sample/number"), "18446744073709551616") != 0
            || output.find_group("empty") == ft_nullptr)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (cma_get_stats(&after, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    std::printf("json_dom_roundtrip allocations=%" FT_PRIu64 " us/op=%.3f\n",
        static_cast<uint64_t>(after - before),
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

int32_t main()
{
    if (measure_decimal() != 0)
        return (1);
    return (measure_bridge());
}
