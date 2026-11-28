#include "../../../Modules/CSV/csv.hpp"
#include <chrono>
#include <cstdio>
#include <string>

template <typename Operation>
static int32_t benchmark_operation(const char *label, uint32_t iterations,
    Operation operation)
{
    uint32_t index = 0U;
    uint32_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (operation(checksum) != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %u\n",
        label, elapsed / iterations, checksum);
    return (0);
}

int32_t main()
{
    const std::string record = "17," + std::string(160U, 'x')
        + ",\"" + std::string(256U, 'y') + ",\"\"quoted\"\"\",42,";
    std::string input;
    uint32_t row_index = 0U;
    while (row_index < 256U)
    {
        input += record;
        input += '\n';
        row_index += 1U;
    }
    ft_csv_document document;
    if (document.initialize(input.c_str()) != FT_ERR_SUCCESS)
        return (1);
    const auto parse = [&input](uint32_t &checksum)
    {
        ft_csv_document parsed;
        const int32_t error = parsed.initialize(input.c_str());
        if (error == FT_ERR_SUCCESS)
            checksum += parsed.get_field(127U, 2U)->size();
        return (error);
    };
    const auto split = [&record](uint32_t &checksum)
    {
        ft_vector<ft_string> fields;
        const int32_t error = csv_split_line(record.c_str(), fields);
        if (error == FT_ERR_SUCCESS)
            checksum += fields[2].size();
        return (error);
    };
    const auto write = [&document](uint32_t &checksum)
    {
        ft_string output;
        const int32_t error = document.write_to_string(output);
        if (error == FT_ERR_SUCCESS)
            checksum += output.size();
        return (error);
    };
    const std::string quoted_field = std::string(256U, 'a') + ",\"quote\"";
    const auto escape = [&quoted_field](uint32_t &checksum)
    {
        char *escaped = csv_escape_field(quoted_field.c_str());
        if (escaped == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        checksum += ft_strlen_size_t(escaped);
        cma_free(escaped);
        return (FT_ERR_SUCCESS);
    };
    if (benchmark_operation("parse document", 40U, parse)
        || benchmark_operation("split one line", 4096U, split)
        || benchmark_operation("write document", 60U, write)
        || benchmark_operation("escape field", 4096U, escape))
        return (1);
    return (0);
}
