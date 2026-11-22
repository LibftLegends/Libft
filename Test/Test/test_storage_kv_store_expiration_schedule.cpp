#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Time/time.hpp"
#include <chrono>

static thread_local int64_t clock_seconds = 0;
static thread_local uint32_t clock_reads = 0;

static std::chrono::system_clock::time_point test_storage_clock()
{
    ++clock_reads;
    return (std::chrono::system_clock::time_point(std::chrono::seconds(clock_seconds)));
}

static int32_t check_storage_expiration_schedule(kv_store &store)
{
    clock_seconds = 100;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("permanent", "value"));
    clock_reads = 0;
    FT_ASSERT_EQ(FT_TRUE, store.kv_contains("permanent"));
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains("missing"));
    FT_ASSERT_EQ(0U, clock_reads);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("first", "old", 5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("second", "value", 10));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_compare_and_swap("first", "old", "kept"));
    clock_seconds = 105;
    FT_ASSERT_EQ(FT_TRUE, store.kv_contains("first"));
    FT_ASSERT_EQ(FT_TRUE, store.kv_contains("second"));
    clock_seconds = 110;
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains("second"));
    FT_ASSERT_EQ(FT_TRUE, store.kv_contains("permanent"));
    kv_store_metrics metrics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(INT64_C(1), metrics.pruned_entries);
    clock_reads = 0;
    FT_ASSERT_EQ(FT_TRUE, store.kv_contains("first"));
    FT_ASSERT_EQ(0U, clock_reads);
    clock_seconds = -1;
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION,
        store.kv_compare_and_swap("first", "kept", "discarded", 5));
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("first"), "kept"));
    return (1);
}

FT_TEST(test_storage_kv_store_prunes_when_next_expiration_is_due)
{
    const char *path = "test_storage_expiration_schedule.json";
    constexpr char EMPTY[] = "{\"kv_store\":{}}";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY, sizeof(EMPTY) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    time_set_clock_now_hook(test_storage_clock);
    const int32_t result = check_storage_expiration_schedule(store);
    time_reset_clock_now_hook();
    FT_ASSERT_EQ(1, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

static int32_t check_imported_expiration_schedule(kv_store &source)
{
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.initialize());
    kv_store_snapshot_entry expired;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expired.key.initialize("expired"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expired.value.initialize("old"));
    expired.has_expiration = FT_TRUE;
    expired.expiration_timestamp = -20;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.push_back(expired));
    kv_store_snapshot_entry later;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, later.key.initialize("later"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, later.value.initialize("kept"));
    later.has_expiration = FT_TRUE;
    later.expiration_timestamp = 120;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.push_back(later));
    clock_seconds = 100;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.import_snapshot(entries));
    kv_store copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    kv_store moved;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.move(source));
    FT_ASSERT_EQ(FT_FALSE, copy.kv_contains("expired"));
    FT_ASSERT_EQ(FT_FALSE, moved.kv_contains("expired"));
    FT_ASSERT_EQ(FT_TRUE, moved.kv_contains("later"));
    clock_seconds = 120;
    FT_ASSERT_EQ(FT_FALSE, copy.kv_contains("later"));
    FT_ASSERT_EQ(FT_FALSE, moved.kv_contains("later"));
    return (1);
}

FT_TEST(test_storage_kv_store_preserves_imported_expiration_across_copy_and_move)
{
    const char *path = "test_storage_expiration_import.json";
    constexpr char EMPTY[] = "{\"kv_store\":{}}";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY, sizeof(EMPTY) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    time_set_clock_now_hook(test_storage_clock);
    const int32_t result = check_imported_expiration_schedule(store);
    time_reset_clock_now_hook();
    FT_ASSERT_EQ(1, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_prunes_multiple_entries_without_allocating)
{
    const char *path = "test_storage_expiration_compact.json";
    constexpr char EMPTY[] = "{\"kv_store\":{}}";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY, sizeof(EMPTY) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("expired-first", "discard", 3600));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("keep-first", "first"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("expired-second", "discard", 3600));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("keep-second", "second"));
    const char *const first = store.kv_get("keep-first");
    const char *const second = store.kv_get("keep-second");
    FT_ASSERT(first != ft_nullptr && second != ft_nullptr);
    clock_seconds = time_now() + 7200;
    time_set_clock_now_hook(test_storage_clock);
    cma_set_alloc_limit(1);
    const int32_t result = store.prune_expired();
    cma_set_alloc_limit(0);
    time_reset_clock_now_hook();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT(store.kv_get("keep-first") == first);
    FT_ASSERT(store.kv_get("keep-second") == second);
    ft_vector<kv_store_snapshot_entry> snapshot;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(snapshot));
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), snapshot.size());
    FT_ASSERT_EQ(0, ft_strcmp(snapshot[0].key.c_str(), "keep-first"));
    FT_ASSERT_EQ(0, ft_strcmp(snapshot[1].key.c_str(), "keep-second"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}
