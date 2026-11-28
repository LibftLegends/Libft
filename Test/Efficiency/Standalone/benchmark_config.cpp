#include "../../../Modules/Config/config.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cstdio>

template <typename Operation>
static int32_t benchmark_operation(const char *label, Operation operation)
{
    constexpr uint32_t ITERATIONS = 4U;
    uint32_t iteration = 0U;
    uint32_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (operation(checksum) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double nanoseconds = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %u\n",
        label, nanoseconds / ITERATIONS, checksum);
    return (0);
}

static int32_t checksum_config(config_data *config, uint32_t &checksum)
{
    if (config == ft_nullptr)
        return (FT_ERR_IO);
    ft_size_t index = 0U;
    while (index < config->entry_count)
    {
        const config_entry &entry = config->entries[index];
        checksum += static_cast<uint32_t>(ft_strlen_size_t(entry.key)
            + ft_strlen_size_t(entry.value));
        index += 1U;
    }
    config_data_free(config);
    return (FT_ERR_SUCCESS);
}

int main(int argument_count, char **arguments)
{
    if (argument_count != 4)
    {
        std::fprintf(stderr, "Usage: benchmark_config BASE_INI OVERRIDE_INI OUTPUT_JSON\n");
        return (1);
    }
    const char *base_path = arguments[1];
    const char *output_path = arguments[3];
    config_data *base = config_parse(base_path);
    config_data *overrides = config_parse(arguments[2]);
    int32_t status = 1;
    if (base != ft_nullptr && overrides != ft_nullptr)
    {
        const auto load_ini = [base_path](uint32_t &checksum)
        {
            return (checksum_config(config_parse(base_path), checksum));
        };
        const auto merge = [base, overrides](uint32_t &checksum)
        {
            return (checksum_config(config_merge(base, overrides), checksum));
        };
        const auto write_json = [base, output_path](uint32_t &checksum)
        {
            const int32_t error = config_write_file(base, output_path);
            if (error == FT_ERR_SUCCESS)
                checksum += static_cast<uint32_t>(base->entry_count);
            return (error);
        };
        const auto load_json = [output_path](uint32_t &checksum)
        {
            return (checksum_config(config_load_file(output_path), checksum));
        };
        status = benchmark_operation("parse INI", load_ini)
            || benchmark_operation("merge configs", merge)
            || benchmark_operation("write JSON", write_json)
            || benchmark_operation("load JSON", load_json);
    }
    config_data_free(base);
    config_data_free(overrides);
    return (status);
}
