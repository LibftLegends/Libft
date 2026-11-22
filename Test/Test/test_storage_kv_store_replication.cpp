#include "../test_internal.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

struct replication_test_state
{
    kv_store *store;
    uint32_t snapshots;
    uint32_t operations;
};

static int32_t count_snapshot(const ft_vector<kv_store_snapshot_entry> &, void *context)
{
    auto *state = static_cast<replication_test_state *>(context);
    ++state->snapshots;
    return (FT_ERR_SUCCESS);
}

static int32_t unregister_during_notification(const ft_vector<kv_store_operation> &operations, void *context)
{
    auto *state = static_cast<replication_test_state *>(context);
    if (operations.size() != 1)
        return (FT_ERR_INVALID_OPERATION);
    ++state->operations;
    return (state->store->unregister_replication_sink(unregister_during_notification, ft_nullptr, context));
}

FT_TEST(test_storage_kv_store_replication_tracks_only_operation_listeners)
{
    const char *path = "test_storage_replication_listeners.json";
    constexpr char EMPTY[] = "{\"kv_store\":{}}";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY, sizeof(EMPTY) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.enable_thread_safety());
    replication_test_state state{&store, 0, 0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.register_replication_sink(ft_nullptr, count_snapshot, &state, FT_TRUE));
    FT_ASSERT_EQ(1U, state.snapshots);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "first"));
    FT_ASSERT_EQ(0U, state.operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.register_replication_sink(unregister_during_notification,
        ft_nullptr, &state, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.register_replication_sink(unregister_during_notification,
        ft_nullptr, &state, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "second"));
    FT_ASSERT_EQ(1U, state.operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "third"));
    FT_ASSERT_EQ(1U, state.operations);
    FT_ASSERT_EQ(1U, state.snapshots);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.unregister_replication_sink(ft_nullptr, count_snapshot, &state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}
