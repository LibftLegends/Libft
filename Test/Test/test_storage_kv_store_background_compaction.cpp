#include "../test_internal.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Time/time.hpp"

static constexpr char EMPTY_STORE[] = "{\"kv_store\":{}}";

FT_TEST(test_storage_kv_store_background_stop_wakes_waiting_worker)
{
    const char *path = "test_storage_background_stop.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.start_background_compaction(4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "value"));
    kv_store_metrics initial{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(initial));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.start_background_compaction(4));
    pt_thread_sleep(50U);
    const int64_t start = time_monotonic();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.stop_background_compaction());
    FT_ASSERT(time_monotonic() - start < 1500);
    kv_store_metrics stopped{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(stopped));
    FT_ASSERT_EQ(initial.prune_operations, stopped.prune_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.stop_background_compaction());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_storage_kv_store_background_prunes_after_restart)
{
    const char *path = "test_storage_background_restart.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY_STORE, sizeof(EMPTY_STORE) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "value"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.start_background_compaction(4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.stop_background_compaction());
    kv_store_metrics initial{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(initial));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.start_background_compaction(1));
    const int64_t deadline = time_monotonic() + 3500;
    kv_store_metrics current{};
    do
    {
        pt_thread_sleep(25U);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(current));
    } while (current.prune_operations == initial.prune_operations && time_monotonic() < deadline);
    FT_ASSERT(current.prune_operations > initial.prune_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, store.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}
