#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static constexpr char EMPTY_STORE[] = "{\"kv_store\":{}}\n";

FT_TEST(test_storage_kv_store_key_suffixes_replace_output_only_after_staging)
{
    constexpr char FIXTURE[] = "{\"kv_store\":{\"match/\":\"exact\",\"match/0\":\"a\",\"match/1\":\"b\","
        "\"unrelated\":\"c\",\"match/2\":\"d\",\"match/3\":\"e\",\"match/4\":\"f\",\"match/5\":\"g\","
        "\"match/6\":\"h\",\"match/7\":\"i\",\"match/8\":\"j\",\"match/9\":\"k\",\"match/10\":\"l\","
        "\"small/z\":\"m\",\"small/a\":\"n\",\"small/b\":\"o\"}}";
    const char *path = "test_storage_key_suffix_output.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, FIXTURE, sizeof(FIXTURE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    ft_vector<ft_string> output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.enable_thread_safety());
    ft_string prefix;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, prefix.initialize("match/"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.push_back(prefix));
    const char *borrowed = output[0].c_str();
    cma_set_alloc_limit(64);
    const int32_t failed = store.export_key_suffixes(borrowed, output);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed);
    FT_ASSERT_EQ(1U, output.size());
    FT_ASSERT(output[0].c_str() == borrowed);
    cma_set_alloc_limit(11 * sizeof(ft_string));
    const int32_t exported = store.export_key_suffixes(borrowed, output);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, exported);
    FT_ASSERT_EQ(11U, output.size());
    FT_ASSERT(output[0] == "0" && output[8] == "8" && output[10] == "10");
    FT_ASSERT_EQ(FT_TRUE, output.is_thread_safe());

    ft_vector<ft_string> shorter;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shorter.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, prefix.assign("small/", 6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shorter.push_back(prefix));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.replace_from(shorter));
    FT_ASSERT_EQ(1U, output.capacity());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_key_suffixes(output[0].c_str(), output));
    FT_ASSERT_EQ(3U, output.size());
    FT_ASSERT(output[0] == "z" && output[1] == "a" && output[2] == "b");
    FT_ASSERT_EQ(FT_TRUE, output.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_large_snapshot_import_preserves_duplicate_order)
{
    const char *path = "test_storage_indexed_snapshot.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.initialize());
    uint32_t index = 0;
    while (index < 70)
    {
        char key[] = "key-00";
        key[4] = static_cast<char>('0' + index / 10);
        key[5] = static_cast<char>('0' + index % 10);
        kv_store_snapshot_entry entry;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.key.initialize(key));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.value.initialize("original"));
        entry.has_expiration = FT_FALSE;
        entry.expiration_timestamp = 0;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.push_back(ft_move(entry)));
        ++index;
    }
    kv_store_snapshot_entry duplicate;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.key.initialize("key-04"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.value.initialize("replacement"));
    duplicate.has_expiration = FT_TRUE;
    duplicate.expiration_timestamp = INT64_C(4000000000);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.push_back(ft_move(duplicate)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.import_snapshot(entries));
    FT_ASSERT_EQ(71U, entries.size());
    FT_ASSERT(entries[4].value == "original" && entries[70].key == "key-04");
    ft_vector<kv_store_snapshot_entry> exported;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(exported));
    FT_ASSERT_EQ(70U, exported.size());
    FT_ASSERT(exported[4].key == "key-04" && exported[4].value == "replacement");
    FT_ASSERT_EQ(FT_TRUE, exported[4].has_expiration);
    FT_ASSERT_EQ(INT64_C(4000000000), exported[4].expiration_timestamp);
    FT_ASSERT(exported[69].key == "key-69" && exported[69].value == "original");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_snapshot_writers_round_trip_escaped_fields)
{
    const char *path = "test_storage_escaped_snapshot.json";
    const char *mapped_path = "test_storage_escaped_mapped_snapshot.json";
    (void)file_delete(mapped_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    const char key[] = "key\"\\\n";
    const char value[] = "value\"\\\t\x01";
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set(key, value, INT64_C(4000000000)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_flush());
    ft_string file_data;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path, file_data));
    ft_string sink_data;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_data.initialize());
    ft_memory_document_sink sink;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    sink.set_storage(&sink_data);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.write_snapshot(sink));
    FT_ASSERT(sink_data == file_data);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.set_backend(KV_STORE_BACKEND_MEMORY_MAPPED, mapped_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_flush());
    ft_string mapped_data;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(mapped_path, mapped_data));
    FT_ASSERT(mapped_data == file_data);

    kv_store loaded;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded.initialize(path));
    const char *restored = loaded.kv_get(key);
    FT_ASSERT(restored != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(restored, value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded.set_backend(KV_STORE_BACKEND_MEMORY_MAPPED, mapped_path));
    restored = loaded.kv_get(key);
    FT_ASSERT(restored != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(restored, value));
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded.export_snapshot(entries));
    FT_ASSERT_EQ(1U, entries.size());
    FT_ASSERT_EQ(FT_TRUE, entries[0].has_expiration);
    FT_ASSERT(entries[0].expiration_timestamp > INT32_MAX);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(mapped_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

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
