#include "../../../Modules/File/file_utils.hpp"
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
    if (filesystem_temp_path("libft_storage_format_initial", "json", &path) != FT_ERR_SUCCESS
        || file_write_all(path.c_str(), contents.data(), contents.size()) != FT_ERR_SUCCESS)
        return (1);
    int32_t result = measure_format(path.c_str(), "jsonl", KV_STORE_BACKEND_JSON_LINES);
    if (result == 0)
        result = measure_format(path.c_str(), "mapped", KV_STORE_BACKEND_MEMORY_MAPPED);
#if SQLITE3_AVAILABLE
    if (result == 0)
        result = measure_format(path.c_str(), "sqlite", KV_STORE_BACKEND_SQLITE);
#endif
    if (file_delete(path.c_str()) != FT_ERR_SUCCESS)
        return (1);
    return (result);
}
