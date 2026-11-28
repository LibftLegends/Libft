#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Template/move.hpp"

static constexpr char EMPTY_STORE[] = "{\"kv_store\":{}}";

FT_TEST(test_storage_kv_store_copy_preserves_unflushed_data_and_backend)
{
    const char *initial_path = "test_storage_copy_initial.json";
    const char *backend_path = "test_storage_copy_backend.jsonl";
    (void)file_delete(backend_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(initial_path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(initial_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.set_backend(KV_STORE_BACKEND_JSON_LINES, backend_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.kv_set("key", "unsaved", INT64_C(4000000000)));
    kv_store copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(KV_STORE_BACKEND_JSON_LINES, copy.get_backend());
    FT_ASSERT_EQ(0, ft_strcmp(copy.kv_get("key"), "unsaved"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.kv_set("key", "copy"));
    FT_ASSERT_EQ(0, ft_strcmp(source.kv_get("key"), "unsaved"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(initial_path));
    return (1);
}

FT_TEST(test_storage_kv_store_move_transfers_live_entries_and_backend)
{
    const char *initial_path = "test_storage_move_initial.json";
    const char *backend_path = "test_storage_move_backend.jsonl";
    (void)file_delete(backend_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(initial_path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(initial_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.set_backend(KV_STORE_BACKEND_JSON_LINES, backend_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.kv_set("key", "unsaved"));
    const char *const source_value = source.kv_get("key");
    FT_ASSERT(source_value != ft_nullptr);
    kv_store destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(initial_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.kv_set("old", "discard"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(ft_move(source)));
    FT_ASSERT_EQ(KV_STORE_BACKEND_JSON_LINES, destination.get_backend());
    FT_ASSERT_EQ(FT_FALSE, destination.kv_contains("old"));
    FT_ASSERT(destination.kv_get("key") == source_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.kv_flush());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(initial_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.set_backend(KV_STORE_BACKEND_JSON_LINES, backend_path));
    FT_ASSERT_EQ(0, ft_strcmp(source.kv_get("key"), "unsaved"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(backend_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(initial_path));
    return (1);
}

FT_TEST(test_storage_kv_store_failed_move_keeps_source_usable)
{
    const char *path = "test_storage_move_allocation.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.kv_set("key", "unsaved"));
    kv_store destination;
    cma_set_alloc_limit(1U);
    const int32_t result = destination.move(source);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(0, ft_strcmp(source.kv_get("key"), "unsaved"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}
