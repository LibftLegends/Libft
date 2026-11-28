#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static constexpr char EMPTY_STORE[] = "{\"kv_store\":{}}";

FT_TEST(test_storage_kv_store_structural_mixed_batch_reuses_deleted_slots)
{
    constexpr char FIXTURE[] = "{\"kv_store\":{\"old\":\"previous\",\"keep\":\"unchanged\"}}";
    const char *path = "test_storage_structural_mixed_batch.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, FIXTURE, sizeof(FIXTURE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("old", "previous", 600));
    const char *retained = store.kv_get("keep");
    FT_ASSERT(retained != ft_nullptr);
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    kv_store_operation insert;
    kv_store_operation remove_old;
    kv_store_operation remove_new;
    kv_store_operation reinsert;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(insert, "new", "temporary", 600));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(remove_old, "old"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(remove_new, "new"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(reinsert, "old", "reinserted"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(insert)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(remove_old)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(remove_new)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(reinsert)));
    kv_store_operation missing;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(missing, "new"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(missing)));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.kv_apply(operations));
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("old"), "previous"));
    FT_ASSERT(store.kv_get("keep") == retained);
    operations.pop_back();

    cma_set_alloc_limit(64);
    const int32_t result = store.kv_apply(operations);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT(store.kv_get("keep") == retained);
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains("new"));
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(entries));
    FT_ASSERT_EQ(2U, entries.size());
    FT_ASSERT(entries[0].key == "keep" && entries[1].key == "old");
    FT_ASSERT(entries[1].value == "reinserted");
    FT_ASSERT_EQ(FT_FALSE, entries[1].has_expiration);
    kv_store_metrics metrics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(INT64_C(3), metrics.set_operations);
    FT_ASSERT_EQ(INT64_C(2), metrics.delete_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_set_batch_grows_once_and_retains_existing_buffers)
{
    constexpr char FIXTURE[] = "{\"kv_store\":{\"keep\":\"retained\",\"update\":\"old\"}}";
    const char *path = "test_storage_append_batch_growth.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, FIXTURE, sizeof(FIXTURE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    ft_string large;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, large.initialize(4096, 'x'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("keep", large.c_str()));
    const char *retained = store.kv_get("keep");
    FT_ASSERT(retained != ft_nullptr);
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    kv_store_operation first;
    kv_store_operation second;
    kv_store_operation repeat;
    kv_store_operation update;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(first, "new-a", "initial", 600));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(second, "new-b", "second"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(repeat, "new-a", "final"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(update, "update", "updated"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(first)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(second)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(repeat)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(update)));

    cma_set_alloc_limit(64);
    const int32_t failed = store.kv_apply(operations);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, failed);
    FT_ASSERT(store.kv_get("keep") == retained);
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("update"), "old"));
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains("new-a"));

    cma_set_alloc_limit(2048);
    const int32_t committed = store.kv_apply(operations);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, committed);
    FT_ASSERT(store.kv_get("keep") == retained);
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(entries));
    FT_ASSERT_EQ(4U, entries.size());
    FT_ASSERT(entries[1].key == "update" && entries[1].value == "updated");
    FT_ASSERT(entries[2].key == "new-a" && entries[2].value == "final");
    FT_ASSERT_EQ(FT_TRUE, entries[2].has_expiration);
    FT_ASSERT(entries[3].key == "new-b" && entries[3].value == "second");
    kv_store_metrics metrics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(INT64_C(5), metrics.set_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_mixed_batch_stages_updates_and_preserves_survivors)
{
    constexpr char FIXTURE[] = "{\"kv_store\":{\"delete\":\"discard\",\"keep\":\"untouched\",\"update\":\"old\"}}";
    const char *path = "test_storage_mixed_existing_batch.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, FIXTURE, sizeof(FIXTURE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    const char *const borrowed = store.kv_get("keep");
    FT_ASSERT(borrowed != ft_nullptr);
    ft_string replacement;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement.initialize(4096, 'x'));
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    kv_store_operation erase;
    kv_store_operation first;
    kv_store_operation second;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(erase, "delete"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(first, "update", "first", 600));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(second, "update", replacement.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(erase)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(first)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(second)));

    cma_set_alloc_limit(256);
    const int32_t failed = store.kv_apply(operations);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, failed);
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("delete"), "discard"));
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("update"), "old"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_apply(operations));
    FT_ASSERT_EQ(ft_nullptr, store.kv_get("delete"));
    FT_ASSERT(store.kv_get("keep") == borrowed);
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(entries));
    FT_ASSERT_EQ(2U, entries.size());
    FT_ASSERT(entries[0].key == "keep" && entries[1].key == "update");
    FT_ASSERT_EQ(FT_TRUE, entries[1].has_expiration);
    FT_ASSERT(entries[1].value == replacement);
    kv_store_metrics metrics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(INT64_C(2), metrics.set_operations);
    FT_ASSERT_EQ(INT64_C(1), metrics.delete_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_batch_reinsert_starts_at_tail_without_old_expiration)
{
    constexpr char FIXTURE[] = "{\"kv_store\":{\"first\":\"old\",\"second\":\"keep\"}}";
    const char *path = "test_storage_batch_reinsert.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, FIXTURE, sizeof(FIXTURE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("first", "old", 600));
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    kv_store_operation erase;
    kv_store_operation set;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(erase, "first"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(set, "first", "new"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(erase)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(ft_move(set)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_apply(operations));
    ft_vector<kv_store_snapshot_entry> entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(entries));
    FT_ASSERT_EQ(2U, entries.size());
    FT_ASSERT(entries[0].key == "second" && entries[1].key == "first" && entries[1].value == "new");
    FT_ASSERT_EQ(FT_FALSE, entries[1].has_expiration);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_single_insert_grows_without_copying_live_values)
{
    constexpr char FIXTURE[] = "{\"kv_store\":{\"one\":\"first\",\"two\":\"second\"}}";
    const char *path = "test_storage_single_insert_growth.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, FIXTURE, sizeof(FIXTURE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    ft_string large_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, large_value.initialize(4096, 'x'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("one", large_value.c_str(), 600));
    const char *retained = store.kv_get("one");
    FT_ASSERT(retained != ft_nullptr);

    cma_set_alloc_limit(64);
    const int32_t failed = store.kv_set("three", "third");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, failed);
    FT_ASSERT(store.kv_get("one") == retained);
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains("three"));

    cma_set_alloc_limit(2048);
    const int32_t grown = store.kv_set("three", "third");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grown);
    FT_ASSERT(store.kv_get("one") == retained);
    ft_vector<kv_store_snapshot_entry> snapshot;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(snapshot));
    FT_ASSERT_EQ(3U, snapshot.size());
    FT_ASSERT(snapshot[0].key == "one" && snapshot[0].has_expiration == FT_TRUE);
    FT_ASSERT(snapshot[1].key == "two" && snapshot[1].value == "second");
    FT_ASSERT(snapshot[2].key == "three" && snapshot[2].value == "third");
    kv_store_metrics metrics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(INT64_C(2), metrics.set_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_single_set_preserves_borrowed_value_and_expiration)
{
    const char *path = "test_storage_single_set_borrowed.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    ft_string expected;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected.initialize(4096, 'x'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", expected.c_str(), 600));
    ft_vector<kv_store_snapshot_entry> snapshot;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(snapshot));
    const int64_t expiration = snapshot[0].expiration_timestamp;
    const char *borrowed = store.kv_get("key");
    FT_ASSERT(borrowed != ft_nullptr);
    cma_set_alloc_limit(64);
    const int32_t failed_set = store.kv_set("key", borrowed + 1);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, failed_set);
    FT_ASSERT(store.kv_get("key") == borrowed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", borrowed + 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(snapshot));
    FT_ASSERT_EQ(FT_TRUE, snapshot[0].has_expiration);
    FT_ASSERT_EQ(expiration, snapshot[0].expiration_timestamp);
    FT_ASSERT_EQ(0, ft_strcmp(snapshot[0].value.c_str(), expected.c_str() + 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_update_batch_preserves_unrelated_value_storage)
{
    const char *path = "test_storage_batch_unrelated.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("one", "original"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("two", "unrelated"));
    const char *const unrelated = store.kv_get("two");
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    kv_store_operation first;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(first, "one", "temporary", 600));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(first));
    kv_store_operation last;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(last, "one", "final"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(last));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_apply(operations));
    FT_ASSERT(store.kv_get("two") == unrelated);
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("one"), "final"));
    ft_vector<kv_store_snapshot_entry> snapshot;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(snapshot));
    FT_ASSERT_EQ(FT_TRUE, snapshot[0].has_expiration);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_structural_batch_failure_preserves_original_entries)
{
    const char *path = "test_storage_batch_failed_insert.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("one", "original"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("two", "untouched"));
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    kv_store_operation insertion;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(insertion, "new", "inserted", 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(insertion));
    kv_store_operation deletion;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(deletion, "missing"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(deletion));
    const char *const original = store.kv_get("one");
    FT_ASSERT(original != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.kv_apply(operations));
    FT_ASSERT(store.kv_get("one") == original);
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains("new"));
    cma_set_alloc_limit(1);
    const int32_t result = store.kv_apply(operations);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, result);
    FT_ASSERT(store.kv_get("one") == original);
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("two"), "untouched"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_update_batch_commits_only_after_all_values_are_staged)
{
    const char *path = "test_storage_batch_failed_update.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("one", "original"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("two", "untouched"));
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    kv_store_operation first;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(first, "one", "temporary", 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(first));
    ft_string large_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, large_value.initialize(256, 'x'));
    kv_store_operation second;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(second, "two", large_value.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(second));
    cma_set_alloc_limit(64);
    const int32_t result = store.kv_apply(operations);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, result);
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("one"), "original"));
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("two"), "untouched"));
    kv_store_metrics metrics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(INT64_C(2), metrics.set_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_single_delete_batch_preserves_survivors_without_allocating)
{
    const char *path = "test_storage_batch_single_delete.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("first", "discard"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("second", "untouched"));
    const char *const second = store.kv_get("second");
    FT_ASSERT(second != ft_nullptr);
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    kv_store_operation deletion;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(deletion, "first"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(deletion));
    cma_set_alloc_limit(1);
    const int32_t result = store.kv_apply(operations);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains("first"));
    FT_ASSERT(store.kv_get("second") == second);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_delete_batch_validates_before_erasing_and_preserves_survivors)
{
    const char *path = "test_storage_batch_delete_only.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("first", "discard"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("second", "retained", 600));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("third", "discard"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("fourth", "untouched"));
    const char *const retained = store.kv_get("second");
    FT_ASSERT(retained != ft_nullptr);

    kv_store_operation first;
    kv_store_operation third;
    kv_store_operation missing;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(first, "first"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(third, "third"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(missing, "missing"));
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(third));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(missing));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.kv_apply(operations));
    FT_ASSERT(store.kv_get("third") != ft_nullptr);
    operations.clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(third));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(third));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.kv_apply(operations));
    FT_ASSERT(store.kv_get("third") != ft_nullptr);

    operations.clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(third));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(first));
    kv_store_metrics metrics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(INT64_C(0), metrics.delete_operations);
    cma_set_alloc_limit(1);
    const int32_t applied = store.kv_apply(operations);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, applied);
    FT_ASSERT(store.kv_get("first") == ft_nullptr);
    FT_ASSERT(store.kv_get("third") == ft_nullptr);
    FT_ASSERT(store.kv_get("second") == retained);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(INT64_C(2), metrics.delete_operations);
    ft_vector<kv_store_snapshot_entry> survivors;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(survivors));
    FT_ASSERT_EQ(2U, survivors.size());
    FT_ASSERT(survivors[0].key == "second");
    FT_ASSERT_EQ(FT_TRUE, survivors[0].has_expiration);
    FT_ASSERT(survivors[1].key == "fourth");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_large_delete_batch_allocation_failure_leaves_every_entry_intact)
{
    const char *path = "test_storage_batch_delete_allocation.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    char key[] = "key-0";
    while (key[4] <= '9')
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set(key, "discard"));
        kv_store_operation deletion;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_delete_operation(deletion, key));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(deletion));
        ++key[4];
    }
    cma_set_alloc_limit(1);
    const int32_t result = store.kv_apply(operations);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, result);
    key[4] = '0';
    while (key[4] <= '9')
    {
        FT_ASSERT(store.kv_get(key) != ft_nullptr);
        ++key[4];
    }
    kv_store_metrics metrics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(INT64_C(0), metrics.delete_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_apply(operations));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(INT64_C(10), metrics.delete_operations);
    ft_vector<kv_store_snapshot_entry> survivors;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.export_snapshot(survivors));
    FT_ASSERT_EQ(0U, survivors.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}
