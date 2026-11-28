#include "../../../Modules/JSon/json.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

int main()
{
    constexpr uint32_t ITEM_COUNT = 8192U;
    constexpr uint32_t ITERATIONS = 10U;
    std::array<json_item, ITEM_COUNT> items{};
    char key[] = "field";
    char name[] = "group";
    std::string value(96U, 'v');
    uint32_t index = 0U;
    while (index < ITEM_COUNT)
    {
        items[index].key = key;
        items[index].value = value.data();
        if (index + 1U < ITEM_COUNT)
            items[index].next = &items[index + 1U];
        ++index;
    }
    json_group group{};
    group.name = name;
    group.items = items.data();

    constexpr char DIRECTORY[] = "json-file-benchmark";
    constexpr char PATH[] = "json-file-benchmark/output.json";
    std::error_code file_error;
    if (!std::filesystem::create_directory(DIRECTORY, file_error))
        return (1);
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS && json_write_to_file(PATH, &group) == FT_ERR_SUCCESS)
        ++iteration;
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const uint64_t output_size = std::filesystem::file_size(PATH, file_error);
    const ft_bool valid = !file_error && output_size != 0U && iteration == ITERATIONS;
    std::remove(PATH);
    std::filesystem::remove(DIRECTORY, file_error);
    if (!valid)
        return (1);
    std::printf("json_file_write bytes=%" FT_PRIu64 " ms/op=%.3f\n", output_size,
        std::chrono::duration<double, std::milli>(elapsed).count() / ITERATIONS);
    return (0);
}
