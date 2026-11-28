#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include "../../../Modules/PThread/pthread.hpp"
#include "../../../Modules/Storage/kv_store.hpp"
#include "../../../Modules/Time/time.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static constexpr uint32_t ENTRY_COUNT = 2048;
static constexpr uint32_t ITERATIONS = 8;

static int32_t measure_snapshot_writes(const char *path)
{
    kv_store store;
    if (store.initialize(path) != FT_ERR_SUCCESS)
        return (1);
    ft_vector<kv_store_snapshot_entry> entries;
    if (entries.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0;
    const auto export_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (store.export_snapshot(entries) != FT_ERR_SUCCESS || entries.size() != ENTRY_COUNT)
            return (1);
        ++iteration;
    }
    const auto export_elapsed = std::chrono::steady_clock::now() - export_start;
    const auto flush_start = std::chrono::steady_clock::now();
    iteration = 0;
    while (iteration < ITERATIONS)
    {
        if (store.kv_flush() != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto flush_elapsed = std::chrono::steady_clock::now() - flush_start;
    std::printf("kv_export_snapshot entries=%u ms/op=%.3f\n", ENTRY_COUNT,
        std::chrono::duration<double, std::milli>(export_elapsed).count() / ITERATIONS);
    std::printf("kv_flush entries=%u ms/op=%.3f\n", ENTRY_COUNT,
        std::chrono::duration<double, std::milli>(flush_elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_snapshot_reads(const char *path)
{
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        kv_store store;
        if (store.initialize(path) != FT_ERR_SUCCESS
            || store.kv_contains("key-2047") != FT_TRUE)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("kv_reload entries=%u ms/op=%.3f\n", ENTRY_COUNT,
        std::chrono::duration<double, std::milli>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_store_move(const char *path)
{
    kv_store source;
    kv_store destination;
    if (source.initialize(path) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (destination.move(source) != FT_ERR_SUCCESS || source.move(destination) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (source.kv_contains("key-2047") != FT_TRUE)
        return (1);
    std::printf("kv_move entries=%u us/op=%.3f\n", ENTRY_COUNT,
        std::chrono::duration<double, std::micro>(elapsed).count() / (2U * ITERATIONS));
    return (0);
}

static int32_t measure_compaction_stop(const char *path)
{
    kv_store store;
    if (store.initialize(path) != FT_ERR_SUCCESS
        || store.enable_thread_safety() != FT_ERR_SUCCESS
        || store.start_background_compaction(1) != FT_ERR_SUCCESS)
        return (1);
    pt_thread_sleep(50U);
    const auto start = std::chrono::steady_clock::now();
    if (store.stop_background_compaction() != FT_ERR_SUCCESS)
        return (1);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("kv_background_stop interval=1s ms=%.3f\n",
        std::chrono::duration<double, std::milli>(elapsed).count());
    return (0);
}

static int32_t measure_expiration_prune(const char *path)
{
    constexpr uint32_t PRUNE_ENTRIES = 256;
    kv_store store;
    ft_vector<kv_store_snapshot_entry> entries;
    if (store.initialize(path) != FT_ERR_SUCCESS || entries.initialize() != FT_ERR_SUCCESS)
        return (1);
    entries.reserve(PRUNE_ENTRIES);
    if (entries.get_error() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0;
    while (index < PRUNE_ENTRIES)
    {
        kv_store_snapshot_entry entry;
        const std::string key = "entry-" + std::to_string(index);
        entry.has_expiration = (index % 2U == 0);
        entry.expiration_timestamp = 0;
        if (entry.key.initialize(key.c_str()) != FT_ERR_SUCCESS
            || entry.value.initialize("stored value") != FT_ERR_SUCCESS
            || entries.push_back(entry) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    std::chrono::steady_clock::duration elapsed{};
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        if (store.import_snapshot(entries) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = store.prune_expired();
        elapsed += std::chrono::steady_clock::now() - start;
        kv_store_metrics metrics{};
        if (result != FT_ERR_SUCCESS || store.get_metrics(metrics) != FT_ERR_SUCCESS
            || metrics.pruned_entries != PRUNE_ENTRIES / 2U)
            return (1);
        ++iteration;
    }
    std::printf("kv_prune entries=256 expired=128 us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_encrypted_snapshot(const char *path)
{
    constexpr ft_size_t PAYLOAD_SIZE = 65536U;
    constexpr char EMPTY[] = "{\"kv_store\":{}}";
    const std::string payload(PAYLOAD_SIZE, 'x');
    kv_store store;
    ft_string snapshot;
    ft_memory_document_sink sink;
    ft_memory_document_source source;
    if (file_write_all(path, EMPTY, sizeof(EMPTY) - 1) != FT_ERR_SUCCESS
        || store.initialize(path, "0123456789abcdef", FT_TRUE) != FT_ERR_SUCCESS
        || store.kv_set("payload", payload.c_str()) != FT_ERR_SUCCESS
        || snapshot.initialize() != FT_ERR_SUCCESS || sink.initialize() != FT_ERR_SUCCESS
        || source.initialize() != FT_ERR_SUCCESS)
        return (1);
    sink.set_storage(&snapshot);
    uint32_t iteration = 0;
    const auto write_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (store.write_snapshot(sink) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto write_elapsed = std::chrono::steady_clock::now() - write_start;
    source.set_data(snapshot.c_str(), snapshot.size());
    iteration = 0;
    const auto read_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (store.read_snapshot(source) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto read_elapsed = std::chrono::steady_clock::now() - read_start;
    const char *loaded_value = store.kv_get("payload");
    if (loaded_value == ft_nullptr || payload != loaded_value)
        return (1);
    std::printf("kv_encrypted_write bytes=65536 ms/op=%.3f\n",
        std::chrono::duration<double, std::milli>(write_elapsed).count() / ITERATIONS);
    std::printf("kv_encrypted_read bytes=65536 ms/op=%.3f\n",
        std::chrono::duration<double, std::milli>(read_elapsed).count() / ITERATIONS);
    return (0);
}

int main()
{
    std::string contents = "{\"kv_store\":{";
    const std::string expiration = std::to_string(time_now() + 600);
    uint32_t index = 0;
    while (index < ENTRY_COUNT)
    {
        if (index != 0)
            contents += ',';
        const std::string key = "key-" + std::to_string(index);
        contents += "\"" + key + "\":\"payload\",\"__ttl__" + key + "\":\"" + expiration + "\"";
        ++index;
    }
    contents += "}}";
    ft_string path;
    if (filesystem_temp_path("libft_storage_snapshots", "json", &path) != FT_ERR_SUCCESS)
        return (1);
    int32_t result = 1;
    if (file_write_all(path.c_str(), contents.data(), contents.size()) == FT_ERR_SUCCESS
        && measure_snapshot_writes(path.c_str()) == 0)
        result = measure_snapshot_reads(path.c_str());
    if (result == 0)
        result = measure_store_move(path.c_str());
    if (result == 0)
        result = measure_compaction_stop(path.c_str());
    if (result == 0)
        result = measure_expiration_prune(path.c_str());
    if (result == 0)
        result = measure_encrypted_snapshot(path.c_str());
    if (file_delete(path.c_str()) != FT_ERR_SUCCESS)
        return (1);
    return (result);
}
