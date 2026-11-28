#include "../../../Modules/Parser/document_backend.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/File/file_utils.hpp"
#include <chrono>
#include <cstdio>
#include <inttypes.h>
#include <string>

int main()
{
    constexpr ft_size_t ITERATIONS = 64;
    constexpr char PATH[] = "benchmark-parser-file-source.tmp";
    const std::string payload(2 * 1024 * 1024, 'p');
    ft_file_document_source source;
    ft_file_document_sink sink;
    if (source.initialize() != FT_ERR_SUCCESS || sink.initialize() != FT_ERR_SUCCESS)
        return (1);
    source.set_path(PATH);
    sink.set_path(PATH);
    if (sink.write_all(payload.data(), payload.size()) != FT_ERR_SUCCESS)
        return (1);

    ft_string output;
    ft_size_t allocations_before = 0;
    ft_size_t allocations_after = 0;
    if (cma_get_stats(&allocations_before, nullptr) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    ft_size_t index = 0;
    int32_t status = FT_ERR_SUCCESS;
    while (index < ITERATIONS && status == FT_ERR_SUCCESS)
    {
        status = output.initialize();
        if (status == FT_ERR_SUCCESS)
            status = source.read_all(output);
        if (status == FT_ERR_SUCCESS && output.size() != payload.size())
            status = FT_ERR_IO;
        if (status == FT_ERR_SUCCESS && index + 1 != ITERATIONS)
            status = output.destroy();
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const int32_t stats_status = cma_get_stats(&allocations_after, nullptr);
    const int32_t delete_status = file_delete(PATH);
    if (status != FT_ERR_SUCCESS || stats_status != FT_ERR_SUCCESS || delete_status != FT_ERR_SUCCESS
        || payload != output.c_str())
        return (1);
    std::printf("read=%.3f us allocations=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS,
        static_cast<uint64_t>((allocations_after - allocations_before) / ITERATIONS));
    return (0);
}
