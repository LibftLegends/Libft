#include "../test_internal.hpp"
#include "compatebility_system_test_hooks.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <string>

static constexpr char EMPTY_JSON_STORE[] = "{\"kv_store\":{}}";

FT_TEST(test_storage_kv_store_json_lines_round_trips_escaped_records_and_wide_ttl)
{
    const char *initial = "test_storage_lines_initial.json";
    const char *path = "test_storage_round_trip.jsonl";
    (void)file_delete(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(initial, EMPTY_JSON_STORE, sizeof(EMPTY_JSON_STORE) - 1));
    kv_store writer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.initialize(initial));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.set_backend(KV_STORE_BACKEND_JSON_LINES, path));
    const char *escaped_key = "key-{\"\\}";
    const std::string large_value = std::string(16380, 'x') + "\n}\\\"tail";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_set(escaped_key, large_value.c_str(), INT64_C(4000000000)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_set("empty", ""));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_flush());
    ft_string file_contents;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_contents.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path, file_contents));
    uint32_t line_count = 0;
    ft_size_t index = 0;
    while (index < file_contents.size())
    {
        if (file_contents.c_str()[index] == '\n')
            ++line_count;
        ++index;
    }
    FT_ASSERT_EQ(2U, line_count);
    kv_store reader;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.initialize(initial));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.set_backend(KV_STORE_BACKEND_JSON_LINES, path));
    const char *restored = reader.kv_get(escaped_key);
    FT_ASSERT(restored != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(restored, large_value.c_str()));
    FT_ASSERT_EQ(FT_TRUE, reader.kv_contains("empty"));
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.export_snapshot(entries));
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), entries.size());
    FT_ASSERT_EQ(FT_TRUE, entries[0].has_expiration);
    FT_ASSERT(entries[0].expiration_timestamp > INT32_MAX);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(initial));
    return (1);
}

FT_TEST(test_storage_kv_store_loads_legacy_pretty_json_lines)
{
    const char *initial = "test_storage_lines_legacy_initial.json";
    const char *path = "test_storage_legacy.jsonl";
    constexpr char LEGACY[] = "{\r\n  \"entry\": {\r\n    \"key\": \"old\",\r\n"
        "    \"value\": \"saved\",\r\n    \"has_expiration\": \"FT_FALSE\",\r\n"
        "    \"expiration\": \"\"\r\n  }\r\n}\r\n\r\n"
        "{\n  \"entry\": {\"key\":\"second\",\"value\":\"record\"}\n}\n";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(initial, EMPTY_JSON_STORE, sizeof(EMPTY_JSON_STORE) - 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, LEGACY, sizeof(LEGACY) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(initial));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.set_backend(KV_STORE_BACKEND_JSON_LINES, path));
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("old"), "saved"));
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("second"), "record"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(initial));
    return (1);
}

FT_TEST(test_storage_kv_store_mapped_flush_preserves_snapshot_on_sync_failure)
{
    const char *initial = "test_storage_mapped_initial.json";
    const char *path = "test_storage_mapped_snapshot.json";
    (void)file_delete(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(initial, EMPTY_JSON_STORE, sizeof(EMPTY_JSON_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(initial));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.set_backend(KV_STORE_BACKEND_MEMORY_MAPPED, path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "old", INT64_C(4000000000)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_flush());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "new"));
    cmp_test_set_file_sync_failure(FT_ERR_IO);
    const int32_t flush_result = store.kv_flush();
    cmp_test_clear_sync_failures();
    FT_ASSERT_EQ(FT_ERR_IO, flush_result);
    kv_store reader;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.initialize(initial));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.set_backend(KV_STORE_BACKEND_MEMORY_MAPPED, path));
    FT_ASSERT_EQ(0, ft_strcmp(reader.kv_get("key"), "old"));
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.export_snapshot(entries));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), entries.size());
    FT_ASSERT(entries[0].expiration_timestamp > INT32_MAX);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(initial));
    return (1);
}

FT_TEST(test_storage_kv_store_json_flushes_preserve_snapshot_on_rename_failure)
{
    const char *json_path = "test_storage_atomic_snapshot.json";
    const char *lines_path = "test_storage_atomic_snapshot.jsonl";
    (void)file_delete(lines_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(json_path, EMPTY_JSON_STORE,
        sizeof(EMPTY_JSON_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(json_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "old"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_flush());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "new"));
    cmp_set_force_cross_device_move(1);
    const int32_t json_result = store.kv_flush();
    cmp_set_force_cross_device_move(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, json_result);
    kv_store reader;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.initialize(json_path));
    FT_ASSERT_EQ(0, ft_strcmp(reader.kv_get("key"), "old"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.set_backend(KV_STORE_BACKEND_JSON_LINES, lines_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.kv_flush());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.kv_set("key", "new"));
    cmp_set_force_cross_device_move(1);
    const int32_t lines_result = reader.kv_flush();
    cmp_set_force_cross_device_move(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, lines_result);
    kv_store lines_reader;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines_reader.initialize(json_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines_reader.set_backend(KV_STORE_BACKEND_JSON_LINES, lines_path));
    FT_ASSERT_EQ(0, ft_strcmp(lines_reader.kv_get("key"), "old"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(lines_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(json_path));
    return (1);
}
