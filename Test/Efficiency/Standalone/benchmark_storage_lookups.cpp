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

static int32_t measure_tail_delete(kv_store &store, const char *key, const char *first_key)
{
    constexpr uint32_t ITERATIONS = 256;
    std::chrono::duration<double, std::micro> elapsed{};
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = store.kv_delete(key);
        elapsed += std::chrono::steady_clock::now() - start;
        if (result != FT_ERR_SUCCESS || store.kv_get(key) != ft_nullptr
            || store.kv_set(key, "original value") != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const char *first_value = store.kv_get(first_key);
    if (first_value == ft_nullptr || ft_strcmp(first_value, "original value") != 0)
        return (1);
    std::printf("%.3f us/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

static int32_t measure_delete_batch(kv_store &store)
{
    constexpr uint32_t ITERATIONS = 32;
    const char *const keys[] = {"key-3", "key-100", "key-20", "key-600"};
    ft_vector<kv_store_operation> operations;
    if (operations.initialize() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t index = 0;
    while (index < sizeof(keys) / sizeof(keys[0]))
    {
        kv_store_operation operation;
        if (kv_store_init_delete_operation(operation, keys[index]) != FT_ERR_SUCCESS
            || operations.push_back(operation) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    std::chrono::duration<double, std::micro> elapsed{};
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = store.kv_apply(operations);
        elapsed += std::chrono::steady_clock::now() - start;
        if (result != FT_ERR_SUCCESS)
            return (1);
        index = 0;
        while (index < sizeof(keys) / sizeof(keys[0]))
        {
            if (store.kv_get(keys[index]) != ft_nullptr
                || store.kv_set(keys[index], "original value") != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        ++iteration;
    }
    const char *retained = store.kv_get("key-0");
    if (retained == ft_nullptr || ft_strcmp(retained, "original value") != 0)
        return (1);
    std::printf("%.3f us/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

static int32_t measure_compare_swap(kv_store &store, const char *key)
{
    constexpr uint32_t ITERATIONS = 512;
    uint32_t iteration = 0;
    const char *expected = "original value";
    const char *replacement = "updated value";
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (store.kv_compare_and_swap(key, expected, replacement) != FT_ERR_SUCCESS)
            return (1);
        const char *old_expected = expected;
        expected = replacement;
        replacement = old_expected;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const char *value = store.kv_get(key);
    if (value == ft_nullptr || ft_strcmp(value, expected) != 0)
        return (1);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

int main(int argument_count, char **arguments)
{
    const ft_bool tail_delete = (argument_count == 2 && ft_strcmp(arguments[1], "--delete-tail") == 0);
    const ft_bool delete_batch = (argument_count == 2 && ft_strcmp(arguments[1], "--delete-batch") == 0);
    const ft_bool compare_swap = (argument_count == 2 && ft_strcmp(arguments[1], "--compare-swap") == 0);
    uint32_t entry_count = 1024;
    if (tail_delete == FT_TRUE)
        entry_count = 4096;
    std::string contents = "{\"kv_store\":{";
    uint32_t index = 0;
    while (index < entry_count)
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
        const std::string key = "key-" + std::to_string(entry_count - 1);
        if (tail_delete == FT_TRUE)
            result = measure_tail_delete(store, key.c_str(), "key-0");
        else if (delete_batch == FT_TRUE)
            result = measure_delete_batch(store);
        else if (compare_swap == FT_TRUE)
            result = measure_compare_swap(store, key.c_str());
        else
        {
            result = measure_storage(store, key.c_str(), "no-ttl");
            if (result == 0)
            {
                result = 1;
                if (store.kv_set("key-0", "original value", 600) == FT_ERR_SUCCESS)
                    result = measure_storage(store, key.c_str(), "future-ttl");
            }
            if (result == 0)
            {
                const std::string second_key = "key-" + std::to_string(entry_count - 2);
                result = measure_existing_batch(store, key.c_str(), second_key.c_str());
            }
        }
        if (store.destroy() != FT_ERR_SUCCESS)
            result = 1;
    }
    if (file_delete(path.c_str()) != FT_ERR_SUCCESS)
        return (1);
    return (result);
}
