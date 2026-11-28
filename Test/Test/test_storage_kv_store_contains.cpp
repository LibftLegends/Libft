#include "../test_internal.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Time/time.hpp"
#include <chrono>

FT_TEST(test_storage_kv_store_contains_distinguishes_empty_and_missing)
{
    const char *file_path;
    const char *initial_contents;
    kv_store store;

    file_path = "test_storage_contains.json";
    initial_contents = "{\"kv_store\":{}}\n";
    file_delete(file_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(file_path, initial_contents,
            ft_strlen_size_t(initial_contents)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(file_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("empty", ""));
    FT_ASSERT_EQ(FT_TRUE, store.kv_contains("empty"));
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains("missing"));
    store.destroy();
    file_delete(file_path);
    return (1);
}

FT_TEST(test_storage_kv_store_contains_remains_usable_with_thread_safety)
{
    const char *file_path;
    const char *initial_contents;
    kv_store store;

    file_path = "test_storage_contains_thread_safe.json";
    initial_contents = "{\"kv_store\":{}}\n";
    file_delete(file_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(file_path, initial_contents,
            ft_strlen_size_t(initial_contents)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(file_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.enable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains("missing"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("present", "value"));
    FT_ASSERT_EQ(FT_TRUE, store.kv_contains("present"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.disable_thread_safety());
    store.destroy();
    file_delete(file_path);
    return (1);
}

FT_TEST(test_storage_kv_store_contains_rejects_null_key)
{
    const char *file_path;
    const char *initial_contents;
    kv_store store;

    file_path = "test_storage_contains_null.json";
    initial_contents = "{\"kv_store\":{}}\n";
    file_delete(file_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(file_path, initial_contents,
            ft_strlen_size_t(initial_contents)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(file_path));
    FT_ASSERT_EQ(FT_FALSE, store.kv_contains(ft_nullptr));
    store.destroy();
    file_delete(file_path);
    return (1);
}

FT_TEST(test_storage_kv_store_get_uses_a_borrowed_key_without_allocating)
{
    const char *path = "test_storage_borrowed_lookup.json";
    constexpr char EMPTY_STORE[] = "{\"kv_store\":{}}\n";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("target", "value"));
    cma_set_alloc_limit(1);
    const char *value = store.kv_get("target");
    cma_set_alloc_limit(0);
    FT_ASSERT(value != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(value, "value"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

static thread_local int64_t borrowed_key_clock_seconds;

static std::chrono::system_clock::time_point test_storage_borrowed_key_clock()
{
    return (std::chrono::system_clock::time_point(std::chrono::seconds(borrowed_key_clock_seconds)));
}

FT_TEST(test_storage_kv_store_get_keeps_lookup_valid_when_pruning_its_key_source)
{
    const char *path = "test_storage_pruned_lookup.json";
    constexpr char EMPTY_STORE[] = "{\"kv_store\":{}}\n";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    borrowed_key_clock_seconds = 100;
    time_set_clock_now_hook(test_storage_borrowed_key_clock);
    const int32_t alias_result = store.kv_set("alias", "target", 1);
    const int32_t target_result = store.kv_set("target", "value");
    const char *borrowed = store.kv_get("alias");
    const ft_bool borrowed_key_found = borrowed != ft_nullptr;
    borrowed_key_clock_seconds = 101;
    const char *value = store.kv_get(borrowed);
    const ft_bool alias_present = store.kv_contains("alias");
    time_reset_clock_now_hook();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, alias_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target_result);
    FT_ASSERT_EQ(FT_TRUE, borrowed_key_found);
    FT_ASSERT(value != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(value, "value"));
    FT_ASSERT_EQ(FT_FALSE, alias_present);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_single_update_preserves_data_when_staging_fails)
{
    const char *path = "test_storage_single_update_failure.json";
    constexpr char EMPTY_STORE[] = "{\"kv_store\":{}}\n";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("target", "original", 600));
    kv_store_operation operation;
    ft_vector<kv_store_operation> operations;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, kv_store_init_set_operation(operation, "target",
        "a larger replacement", 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operations.push_back(operation));
    cma_set_alloc_limit(1);
    const int32_t result = store.kv_apply(operations);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, result);
    const char *value = store.kv_get("target");
    FT_ASSERT(value != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(value, "original"));
    kv_store_metrics metrics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(1, metrics.set_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}
