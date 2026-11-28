#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include "../../../Modules/Storage/kv_store.hpp"
#include "../../../Modules/Time/time.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static constexpr uint32_t ENTRY_COUNT = 512;

static int32_t measure_format(const char *initial_path, const char *name,
    kv_store_backend_type backend)
{
    constexpr uint32_t ITERATIONS = 8;
    ft_string path;
    if (filesystem_temp_path("libft_storage_format", name, &path) != FT_ERR_SUCCESS)
        return (1);
    kv_store store;
    if (store.initialize(initial_path) != FT_ERR_SUCCESS
        || store.set_backend(backend, path.c_str()) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0;
    int32_t result = FT_ERR_SUCCESS;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && result == FT_ERR_SUCCESS)
    {
        result = store.kv_flush();
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const int32_t cleanup = file_delete(path.c_str());
    if (result != FT_ERR_SUCCESS || cleanup != FT_ERR_SUCCESS)
        return (1);
    std::printf("storage_flush_%s entries=%u ms/op=%.3f\n", name, ENTRY_COUNT,
        std::chrono::duration<double, std::milli>(elapsed).count() / ITERATIONS);
    return (0);
}

#if SQLITE3_AVAILABLE
static int32_t measure_sqlite_large(const char *initial_path, const std::string &expected)
{
    constexpr uint32_t ITERATIONS = 8;
    ft_string path;
    if (filesystem_temp_path("libft_storage_sqlite_large", "sqlite", &path) != FT_ERR_SUCCESS)
        return (1);
    kv_store writer;
    kv_store reader;
    if (writer.initialize(initial_path) != FT_ERR_SUCCESS || reader.initialize(initial_path) != FT_ERR_SUCCESS
        || writer.set_backend(KV_STORE_BACKEND_SQLITE, path.c_str()) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0;
    const auto flush_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (writer.kv_flush() != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto flush_elapsed = std::chrono::steady_clock::now() - flush_start;
    iteration = 0;
    const auto load_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (reader.set_backend(KV_STORE_BACKEND_SQLITE, path.c_str()) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto load_elapsed = std::chrono::steady_clock::now() - load_start;
    if (file_delete(path.c_str()) != FT_ERR_SUCCESS
        || ft_strcmp(reader.kv_get("key-31"), expected.c_str()) != 0)
        return (1);
    std::printf("sqlite_large flush=%.3f ms load=%.3f ms\n",
        std::chrono::duration<double, std::milli>(flush_elapsed).count() / ITERATIONS,
        std::chrono::duration<double, std::milli>(load_elapsed).count() / ITERATIONS);
    return (0);
}
#endif

int main(int argument_count, char **arguments)
{
    const ft_bool sqlite_large = (argument_count == 2 && ft_strcmp(arguments[1], "--sqlite-large") == 0);
    uint32_t entry_count = ENTRY_COUNT;
    std::string value = "payload";
    if (sqlite_large == FT_TRUE)
    {
#if SQLITE3_AVAILABLE
        entry_count = 32;
        value.assign(32 * 1024, 'v');
#else
        return (1);
#endif
    }
    std::string contents = "{\"kv_store\":{";
    const std::string expiration = std::to_string(time_now() + 600);
    uint32_t index = 0;
    while (index < entry_count)
    {
        if (index != 0)
            contents += ',';
        const std::string key = "key-" + std::to_string(index);
        contents += "\"" + key + "\":\"" + value + "\",\"__ttl__" + key + "\":\"" + expiration + "\"";
        ++index;
    }
    contents += "}}";
    ft_string path;
    if (filesystem_temp_path("libft_storage_format_initial", "json", &path) != FT_ERR_SUCCESS
        || file_write_all(path.c_str(), contents.data(), contents.size()) != FT_ERR_SUCCESS)
        return (1);
    int32_t result;
    if (sqlite_large == FT_TRUE)
    {
#if SQLITE3_AVAILABLE
        result = measure_sqlite_large(path.c_str(), value);
#else
        result = 1;
#endif
    }
    else
    {
        result = measure_format(path.c_str(), "jsonl", KV_STORE_BACKEND_JSON_LINES);
        if (result == 0)
            result = measure_format(path.c_str(), "mapped", KV_STORE_BACKEND_MEMORY_MAPPED);
#if SQLITE3_AVAILABLE
        if (result == 0)
            result = measure_format(path.c_str(), "sqlite", KV_STORE_BACKEND_SQLITE);
#endif
    }
    if (file_delete(path.c_str()) != FT_ERR_SUCCESS)
        return (1);
    return (result);
}
