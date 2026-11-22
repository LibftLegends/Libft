#include "../test_internal.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if SQLITE3_AVAILABLE
FT_TEST(test_storage_kv_store_sqlite_round_trips_encrypted_rows_and_wide_ttl)
{
    const char *initial = "test_storage_sqlite_crypto_initial.json";
    const char *path = "test_storage_sqlite_crypto.sqlite";
    const char *key = "0123456789abcdef";
    constexpr char EMPTY[] = "{\"kv_store\":{}}";
    (void)file_delete(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(initial, EMPTY, sizeof(EMPTY) - 1));
    kv_store writer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.initialize(initial, key, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.set_backend(KV_STORE_BACKEND_SQLITE, path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_set("wide", "value crossing an AES block boundary", INT64_C(4000000000)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_set("empty", ""));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_set("another", "short"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_flush());
    kv_store reader;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.initialize(initial, key, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.set_backend(KV_STORE_BACKEND_SQLITE, path));
    FT_ASSERT_EQ(0, ft_strcmp(reader.kv_get("wide"), "value crossing an AES block boundary"));
    FT_ASSERT_EQ(0, ft_strcmp(reader.kv_get("empty"), ""));
    FT_ASSERT_EQ(0, ft_strcmp(reader.kv_get("another"), "short"));
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.export_snapshot(entries));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), entries.size());
    ft_size_t index = 0;
    while (index < entries.size() && ft_strcmp(entries[index].key.c_str(), "wide") != 0)
        ++index;
    FT_ASSERT(index < entries.size());
    FT_ASSERT_EQ(FT_TRUE, entries[index].has_expiration);
    FT_ASSERT(entries[index].expiration_timestamp > INT32_MAX);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(initial));
    return (1);
}

FT_TEST(test_storage_kv_store_sqlite_preserves_snapshot_when_insert_fails)
{
    const char *initial = "test_storage_sqlite_rollback_initial.json";
    const char *path = "test_storage_sqlite_rollback.sqlite";
    constexpr char EMPTY[] = "{\"kv_store\":{}}";
    (void)file_delete(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(initial, EMPTY, sizeof(EMPTY) - 1));
    kv_store writer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.initialize(initial));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.set_backend(KV_STORE_BACKEND_SQLITE, path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_set("keep", "original"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_set("untouched", ""));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_flush());
    sqlite3 *database = ft_nullptr;
    const int32_t open_result = sqlite3_open(path, &database);
    int32_t trigger_result = open_result;
    if (open_result == SQLITE_OK)
        trigger_result = sqlite3_exec(database, "CREATE TRIGGER reject_new_key BEFORE INSERT ON kv_store_entries "
            "WHEN NEW.key = 'reject' BEGIN SELECT RAISE(ABORT, 'test rollback'); END;",
            ft_nullptr, ft_nullptr, ft_nullptr);
    sqlite3_close(database);
    FT_ASSERT_EQ(SQLITE_OK, trigger_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_set("keep", "changed"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer.kv_set("reject", "new"));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, writer.kv_flush());
    kv_store reader;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.initialize(initial));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.set_backend(KV_STORE_BACKEND_SQLITE, path));
    FT_ASSERT_EQ(0, ft_strcmp(reader.kv_get("keep"), "original"));
    FT_ASSERT_EQ(FT_TRUE, reader.kv_contains("untouched"));
    FT_ASSERT_EQ(FT_FALSE, reader.kv_contains("reject"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(initial));
    return (1);
}
#endif
