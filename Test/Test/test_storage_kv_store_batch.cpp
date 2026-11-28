#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static constexpr char EMPTY_STORE[] = "{\"kv_store\":{}}";

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
