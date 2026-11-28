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

static int32_t measure_large_sets(kv_store &store, const char *key)
{
    constexpr uint32_t ITERATIONS = 512;
    const std::string first(64 * 1024, 'a');
    const std::string second(64 * 1024, 'b');
    const char *next = first.c_str();
    const char *previous = second.c_str();
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (store.kv_set(key, next) != FT_ERR_SUCCESS)
            return (1);
        const char *written = next;
        next = previous;
        previous = written;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const char *stored = store.kv_get(key);
    if (stored == ft_nullptr || ft_strcmp(stored, previous) != 0)
        return (1);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_insert_growth(const char *path, const char *last_key, const char *expected)
{
    constexpr uint32_t ITERATIONS = 8;
    std::chrono::duration<double, std::micro> elapsed{};
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        kv_store store;
        if (store.initialize(path) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = store.kv_set("new-entry", "new-value");
        elapsed += std::chrono::steady_clock::now() - start;
        const char *inserted = store.kv_get("new-entry");
        const char *retained = store.kv_get(last_key);
        if (result != FT_ERR_SUCCESS || inserted == ft_nullptr || retained == ft_nullptr
            || ft_strcmp(inserted, "new-value") != 0 || ft_strcmp(retained, expected) != 0
            || store.destroy() != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    std::printf("%.3f us/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

static int32_t measure_json_flush(kv_store &store, const char *initial_path, const char *last_key,
    const char *expected, ft_bool json_lines, ft_bool measure_load)
{
    constexpr uint32_t ITERATIONS = 8;
    ft_string lines_path;
    if (json_lines == FT_TRUE
        && (filesystem_temp_path("libft_storage_lines_benchmark", "jsonl", &lines_path) != FT_ERR_SUCCESS
            || store.set_backend(KV_STORE_BACKEND_JSON_LINES, lines_path.c_str()) != FT_ERR_SUCCESS))
        return (1);
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (store.kv_flush() != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    kv_store reader;
    if (reader.initialize(initial_path) != FT_ERR_SUCCESS)
        return (1);
    if (json_lines == FT_TRUE)
    {
        uint32_t load_count = 1;
        if (measure_load == FT_TRUE)
            load_count = ITERATIONS;
        const auto load_start = std::chrono::steady_clock::now();
        while (load_count != 0)
        {
            if (reader.set_backend(KV_STORE_BACKEND_JSON_LINES, lines_path.c_str()) != FT_ERR_SUCCESS)
                return (1);
            --load_count;
        }
        if (measure_load == FT_TRUE)
            elapsed = std::chrono::steady_clock::now() - load_start;
        if (file_delete(lines_path.c_str()) != FT_ERR_SUCCESS)
            return (1);
    }
    const char *retained = reader.kv_get(last_key);
    if (retained == ft_nullptr || ft_strcmp(retained, expected) != 0)
        return (1);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_mixed_batch(kv_store &store, const char *last_key, const char *retained_value)
{
    constexpr uint32_t ITERATIONS = 16;
    ft_vector<kv_store_operation> operations;
    kv_store_operation erase;
    kv_store_operation update;
    if (operations.initialize() != FT_ERR_SUCCESS
        || kv_store_init_delete_operation(erase, "key-0") != FT_ERR_SUCCESS
        || kv_store_init_set_operation(update, last_key, "updated value") != FT_ERR_SUCCESS
        || operations.push_back(ft_move(erase)) != FT_ERR_SUCCESS
        || operations.push_back(ft_move(update)) != FT_ERR_SUCCESS)
        return (1);
    std::chrono::duration<double, std::micro> elapsed{};
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = store.kv_apply(operations);
        elapsed += std::chrono::steady_clock::now() - start;
        if (result != FT_ERR_SUCCESS || store.kv_contains("key-0")
            || store.kv_set("key-0", retained_value) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const char *retained = store.kv_get("key-1");
    const char *updated = store.kv_get(last_key);
    if (retained == ft_nullptr || updated == ft_nullptr || ft_strcmp(retained, retained_value) != 0
        || ft_strcmp(updated, "updated value") != 0)
        return (1);
    std::printf("%.3f us/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

static int32_t measure_insert_batch(kv_store &store, const char *last_key, const char *retained_value,
    ft_bool replace_key)
{
    constexpr uint32_t ITERATIONS = 16;
    ft_vector<kv_store_operation> operations;
    if (operations.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (replace_key == FT_TRUE)
    {
        kv_store_operation erase;
        if (kv_store_init_delete_operation(erase, "key-0") != FT_ERR_SUCCESS
            || operations.push_back(ft_move(erase)) != FT_ERR_SUCCESS)
            return (1);
    }
    kv_store_operation insert;
    kv_store_operation update;
    if (kv_store_init_set_operation(insert, "new-entry", "inserted value") != FT_ERR_SUCCESS
        || kv_store_init_set_operation(update, last_key, "updated value") != FT_ERR_SUCCESS
        || operations.push_back(ft_move(insert)) != FT_ERR_SUCCESS
        || operations.push_back(ft_move(update)) != FT_ERR_SUCCESS)
        return (1);
    std::chrono::duration<double, std::micro> elapsed{};
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = store.kv_apply(operations);
        elapsed += std::chrono::steady_clock::now() - start;
        const char *inserted = store.kv_get("new-entry");
        if (result != FT_ERR_SUCCESS || inserted == ft_nullptr || ft_strcmp(inserted, "inserted value") != 0
            || store.kv_delete("new-entry") != FT_ERR_SUCCESS)
            return (1);
        if (replace_key == FT_TRUE && (store.kv_contains("key-0")
            || store.kv_set("key-0", retained_value) != FT_ERR_SUCCESS))
            return (1);
        ++iteration;
    }
    const char *retained = store.kv_get("key-1");
    const char *updated = store.kv_get(last_key);
    if (retained == ft_nullptr || updated == ft_nullptr || ft_strcmp(retained, retained_value) != 0
        || ft_strcmp(updated, "updated value") != 0)
        return (1);
    std::printf("%.3f us/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

static int32_t measure_snapshot_import(kv_store &store, ft_size_t count, const char *last_key)
{
    constexpr uint32_t ITERATIONS = 6;
    ft_vector<kv_store_snapshot_entry> snapshot;
    if (store.export_snapshot(snapshot) != FT_ERR_SUCCESS || snapshot.size() != count)
        return (1);
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (store.import_snapshot(snapshot) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    ft_vector<kv_store_snapshot_entry> loaded;
    if (store.export_snapshot(loaded) != FT_ERR_SUCCESS || loaded.size() != count
        || loaded[count - 1].key != last_key || loaded[count - 1].value != "original value"
        || snapshot.size() != count || snapshot[count - 1].key != last_key)
        return (1);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t count_replicated_set(const ft_vector<kv_store_operation> &operations, void *context)
{
    if (operations.size() != 1 || operations[0]._value != "replicated value")
        return (FT_ERR_INVALID_OPERATION);
    ++*static_cast<uint32_t *>(context);
    return (FT_ERR_SUCCESS);
}

static int32_t measure_replicated_update(kv_store &store)
{
    constexpr uint32_t ITERATIONS = 20000;
    ft_vector<kv_store_operation> operations;
    kv_store_operation set;
    uint32_t notifications = 0;
    if (operations.initialize() != FT_ERR_SUCCESS
        || kv_store_init_set_operation(set, "key-0", "replicated value") != FT_ERR_SUCCESS
        || operations.push_back(ft_move(set)) != FT_ERR_SUCCESS
        || store.register_replication_sink(count_replicated_set, ft_nullptr, &notifications, FT_FALSE)
            != FT_ERR_SUCCESS)
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
    if (store.unregister_replication_sink(count_replicated_set, ft_nullptr, &notifications) != FT_ERR_SUCCESS
        || notifications != ITERATIONS || ft_strcmp(store.kv_get("key-0"), "replicated value") != 0)
        return (1);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_prefix_export(kv_store &store, ft_size_t count, const char *last_suffix)
{
    constexpr uint32_t ITERATIONS = 16;
    ft_vector<ft_string> suffixes;
    if (suffixes.initialize() != FT_ERR_SUCCESS || suffixes.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (store.export_key_suffixes("key-", suffixes) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (suffixes.size() != count || suffixes[0] != "0" || suffixes[count - 1] != last_suffix
        || suffixes.is_thread_safe() != FT_TRUE)
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
    const ft_bool large_sets = (argument_count == 2 && ft_strcmp(arguments[1], "--set-large") == 0);
    const ft_bool insert_growth = (argument_count == 2 && ft_strcmp(arguments[1], "--insert-growth") == 0);
    const ft_bool flush_json = (argument_count == 2 && ft_strcmp(arguments[1], "--flush-json") == 0);
    const ft_bool flush_json_lines = (argument_count == 2 && ft_strcmp(arguments[1], "--flush-json-lines") == 0);
    const ft_bool load_json_lines = (argument_count == 2 && ft_strcmp(arguments[1], "--load-json-lines") == 0);
    const ft_bool mixed_batch = (argument_count == 2 && ft_strcmp(arguments[1], "--mixed-batch") == 0);
    const ft_bool insert_batch = (argument_count == 2 && ft_strcmp(arguments[1], "--insert-batch") == 0);
    const ft_bool replace_batch = (argument_count == 2 && ft_strcmp(arguments[1], "--replace-batch") == 0);
    const ft_bool import_snapshot = (argument_count == 2 && ft_strcmp(arguments[1], "--import-snapshot") == 0);
    const ft_bool replicate_update = (argument_count == 2 && ft_strcmp(arguments[1], "--replicated-set") == 0);
    const ft_bool prefix_export = (argument_count == 2 && ft_strcmp(arguments[1], "--export-prefix") == 0);
    uint32_t entry_count = 1024;
    if (tail_delete == FT_TRUE)
        entry_count = 4096;
    if (insert_growth == FT_TRUE)
        entry_count = 256;
    if (flush_json == FT_TRUE || mixed_batch == FT_TRUE || insert_batch == FT_TRUE || replace_batch == FT_TRUE)
        entry_count = 96;
    if (import_snapshot == FT_TRUE)
        entry_count = 2048;
    if (replicate_update == FT_TRUE)
        entry_count = 8;
    std::string initial_value = "original value";
    if (insert_growth == FT_TRUE || flush_json == FT_TRUE || mixed_batch == FT_TRUE
        || insert_batch == FT_TRUE || replace_batch == FT_TRUE)
        initial_value.assign(4096, 'x');
    std::string contents = "{\"kv_store\":{";
    uint32_t index = 0;
    while (index < entry_count)
    {
        if (index != 0)
            contents += ',';
        contents += "\"key-" + std::to_string(index) + "\":\"" + initial_value + "\"";
        ++index;
    }
    contents += "}}";
    ft_string path;
    if (filesystem_temp_path("libft_storage_benchmark", "json", &path) != FT_ERR_SUCCESS
        || file_write_all(path.c_str(), contents.data(), contents.size()) != FT_ERR_SUCCESS)
        return (1);
    kv_store store;
    int32_t result = 1;
    const std::string key = "key-" + std::to_string(entry_count - 1);
    if (insert_growth == FT_TRUE)
        result = measure_insert_growth(path.c_str(), key.c_str(), initial_value.c_str());
    else if (store.initialize(path.c_str()) == FT_ERR_SUCCESS)
    {
        if (tail_delete == FT_TRUE)
            result = measure_tail_delete(store, key.c_str(), "key-0");
        else if (delete_batch == FT_TRUE)
            result = measure_delete_batch(store);
        else if (compare_swap == FT_TRUE)
            result = measure_compare_swap(store, key.c_str());
        else if (large_sets == FT_TRUE)
            result = measure_large_sets(store, key.c_str());
        else if (flush_json == FT_TRUE || flush_json_lines == FT_TRUE || load_json_lines == FT_TRUE)
            result = measure_json_flush(store, path.c_str(), key.c_str(), initial_value.c_str(),
                flush_json_lines || load_json_lines, load_json_lines);
        else if (mixed_batch == FT_TRUE)
            result = measure_mixed_batch(store, key.c_str(), initial_value.c_str());
        else if (insert_batch == FT_TRUE || replace_batch == FT_TRUE)
            result = measure_insert_batch(store, key.c_str(), initial_value.c_str(), replace_batch);
        else if (import_snapshot == FT_TRUE)
            result = measure_snapshot_import(store, entry_count, key.c_str());
        else if (replicate_update == FT_TRUE)
            result = measure_replicated_update(store);
        else if (prefix_export == FT_TRUE)
            result = measure_prefix_export(store, entry_count, key.c_str() + 4);
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
