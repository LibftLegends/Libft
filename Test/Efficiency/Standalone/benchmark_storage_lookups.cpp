#include "../../../Modules/Storage/kv_store.hpp"
#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static int32_t measure_existing_batch(kv_store &store, const char *first_key, const char *second_key)
{
    constexpr uint32_t ITERATIONS = 200;
    ft_vector<kv_store_operation> operations;
    kv_store_operation first;
    kv_store_operation second;
    if (operations.initialize() != FT_ERR_SUCCESS
        || kv_store_init_set_operation(first, first_key, "batch value") != FT_ERR_SUCCESS
        || kv_store_init_set_operation(second, second_key, "batch value") != FT_ERR_SUCCESS
        || operations.push_back(first) != FT_ERR_SUCCESS || operations.push_back(second) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (store.kv_apply(operations) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const char *first_value = store.kv_get(first_key);
    const char *second_value = store.kv_get(second_key);
    if (first_value == ft_nullptr || second_value == ft_nullptr
        || ft_strcmp(first_value, "batch value") != 0 || ft_strcmp(second_value, "batch value") != 0)
        return (1);
    std::printf("kv_apply_existing operations=2 us/batch=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_storage(kv_store &store, const char *key, const char *workload)
{
    constexpr uint32_t UPDATE_COUNT = 200;
    constexpr uint32_t LOOKUP_COUNT = 2000;
    uint32_t iteration = 0;
    const auto update_start = std::chrono::steady_clock::now();
    while (iteration < UPDATE_COUNT)
    {
        if (store.kv_set(key, "updated value") != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto update_elapsed = std::chrono::steady_clock::now() - update_start;
    iteration = 0;
    const auto lookup_start = std::chrono::steady_clock::now();
    while (iteration < LOOKUP_COUNT)
    {
        const char *value = store.kv_get(key);
        if (value == ft_nullptr || ft_strcmp(value, "updated value") != 0)
            return (1);
        ++iteration;
    }
    const auto lookup_elapsed = std::chrono::steady_clock::now() - lookup_start;
    std::printf("kv_set_existing workload=%s us/op=%.3f\n", workload,
        std::chrono::duration<double, std::micro>(update_elapsed).count() / UPDATE_COUNT);
    std::printf("kv_get_existing workload=%s us/op=%.3f\n", workload,
        std::chrono::duration<double, std::micro>(lookup_elapsed).count() / LOOKUP_COUNT);
    return (0);
}

int main()
{
    constexpr uint32_t ENTRIES = 1024;
    std::string contents = "{\"kv_store\":{";
    uint32_t index = 0;
    while (index < ENTRIES)
    {
        if (index != 0)
            contents += ',';
        contents += "\"key-" + std::to_string(index) + "\":\"original value\"";
        ++index;
    }
    contents += "}}";
    ft_string path;
    if (filesystem_temp_path("libft_storage_benchmark", "json", &path) != FT_ERR_SUCCESS
        || file_write_all(path.c_str(), contents.data(), contents.size()) != FT_ERR_SUCCESS)
        return (1);
    kv_store store;
    int32_t result = 1;
    if (store.initialize(path.c_str()) == FT_ERR_SUCCESS)
    {
        const std::string key = "key-" + std::to_string(ENTRIES - 1);
        result = measure_storage(store, key.c_str(), "no-ttl");
        if (result == 0)
        {
            result = 1;
            if (store.kv_set("key-0", "original value", 600) == FT_ERR_SUCCESS)
                result = measure_storage(store, key.c_str(), "future-ttl");
        }
        if (result == 0)
        {
            const std::string second_key = "key-" + std::to_string(ENTRIES - 2);
            result = measure_existing_batch(store, key.c_str(), second_key.c_str());
        }
        if (store.destroy() != FT_ERR_SUCCESS)
            result = 1;
    }
    if (file_delete(path.c_str()) != FT_ERR_SUCCESS)
        return (1);
    return (result);
}
