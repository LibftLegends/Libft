#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static constexpr char EMPTY_STORE[] = "{\"kv_store\":{}}\n";

FT_TEST(test_storage_kv_store_snapshot_round_trips_wide_expiration)
{
    const char *path = "test_storage_wide_expiration.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("region", "payload", INT64_C(4000000000)));
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    ft_memory_document_sink sink;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    sink.set_storage(&output);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.write_snapshot(sink));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_flush());
    ft_vector<kv_store_snapshot_entry> before;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(before));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), before.size());
    FT_ASSERT(before[0].expiration_timestamp > INT32_MAX);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    ft_vector<kv_store_snapshot_entry> from_disk;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(from_disk));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), from_disk.size());
    FT_ASSERT_EQ(before[0].expiration_timestamp, from_disk[0].expiration_timestamp);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_delete("region"));
    ft_memory_document_source source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_data(output.data(), output.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.read_snapshot(source));
    ft_vector<kv_store_snapshot_entry> from_memory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(from_memory));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), from_memory.size());
    FT_ASSERT_EQ(before[0].expiration_timestamp, from_memory[0].expiration_timestamp);
    FT_ASSERT_EQ(FT_TRUE, from_memory[0].value == "payload");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_failed_snapshot_import_leaves_store_intact)
{
    const char *path = "test_storage_failed_snapshot_import.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("retained", "old"));
    kv_store_snapshot_entry first;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.key.initialize("first"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.value.initialize("new"));
    first.has_expiration = FT_FALSE;
    first.expiration_timestamp = 0;
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.push_back(first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.push_back(first));
    cma_set_alloc_limit(1);
    const int32_t allocation_result = store.import_snapshot(entries);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, allocation_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries[1].value.destroy());
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.import_snapshot(entries));
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains("first"));
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("retained"), "old"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_backend_switch_preserves_location_on_parse_error)
{
    const char *original = "test_storage_backend_before.json";
    const char *corrupt = "test_storage_backend_corrupt.json";
    const char *fresh = "test_storage_backend_fresh.json";
    (void)file_delete(fresh);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(original, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(corrupt, "{", 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(original));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("retained", "value"));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.set_backend(KV_STORE_BACKEND_JSON, corrupt));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_flush());
    ft_string content;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(corrupt, content));
    FT_ASSERT_EQ(FT_TRUE, content == "{");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.set_backend(KV_STORE_BACKEND_JSON, fresh));
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("retained"), "value"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_flush());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(fresh));
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("retained"), "value"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(original));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(corrupt));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(fresh));
    return (1);
}
