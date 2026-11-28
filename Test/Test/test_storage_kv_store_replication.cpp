#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

struct replication_test_state
{
    kv_store *store;
    uint32_t snapshots;
    uint32_t operations;
};

struct ordered_replication_listener
{
    uint32_t identifier;
    uint32_t *next_identifier;
    uint32_t stop_at;
};

static int32_t record_ordered_notification(const ft_vector<kv_store_operation> &operations, void *user_data)
{
    auto *listener = static_cast<ordered_replication_listener *>(user_data);
    if (operations.size() != 1 || operations[0]._key != "key"
        || listener->identifier != *listener->next_identifier)
        return (FT_ERR_INVALID_OPERATION);
    ++*listener->next_identifier;
    if (listener->identifier == listener->stop_at)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_storage_kv_store_replication_dispatches_large_listener_snapshots_in_order)
{
    const char *path = "test_storage_replication_dispatch.json";
    constexpr char EMPTY[] = "{\"kv_store\":{}}";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY, sizeof(EMPTY) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "before"));
    constexpr uint32_t LISTENER_COUNT = 9;
    uint32_t next_identifier = 0;
    ordered_replication_listener listeners[LISTENER_COUNT];
    uint32_t index = 0;
    while (index < LISTENER_COUNT)
    {
        listeners[index] = {index, &next_identifier, LISTENER_COUNT};
        FT_ASSERT_EQ(FT_ERR_SUCCESS, store.register_replication_sink(
            record_ordered_notification, ft_nullptr, &listeners[index], FT_FALSE));
        ++index;
    }
    cma_set_alloc_limit(128);
    const int32_t refused = store.kv_set("key", "committed");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, refused);
    FT_ASSERT_EQ(0U, next_identifier);
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("key"), "committed"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "all listeners"));
    FT_ASSERT_EQ(LISTENER_COUNT, next_identifier);
    next_identifier = 0;
    listeners[3].stop_at = 3;
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, store.kv_set("key", "stopped"));
    FT_ASSERT_EQ(4U, next_identifier);
    FT_ASSERT_EQ(0, ft_strcmp(store.kv_get("key"), "stopped"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

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
    const kv_store_operation &operation = operations[0];
    const char *committed = state->store->kv_get("key");
    if (operation._type != KV_STORE_OPERATION_TYPE_SET || operation._key != "key"
        || operation._has_value != FT_TRUE || operation._value != "second"
        || operation._has_ttl != FT_TRUE || operation._ttl_seconds != 600
        || committed == ft_nullptr || ft_strcmp(committed, "second") != 0)
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
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "second", 600));
    FT_ASSERT_EQ(1U, state.operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("key", "third"));
    FT_ASSERT_EQ(1U, state.operations);
    FT_ASSERT_EQ(1U, state.snapshots);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.unregister_replication_sink(ft_nullptr, count_snapshot, &state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

static int32_t count_valid_delete(const ft_vector<kv_store_operation> &operations, void *context)
{
    auto *state = static_cast<replication_test_state *>(context);
    if (operations.size() != 1)
        return (FT_ERR_INVALID_OPERATION);
    const kv_store_operation &operation = operations[0];
    if (operation._type != KV_STORE_OPERATION_TYPE_DELETE
        || operation._key != "replicated" || operation._has_value != FT_FALSE
        || state->store->kv_get("replicated") != ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    ++state->operations;
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_storage_kv_store_delete_stages_notification_only_for_operation_listeners)
{
    const char *path = "test_storage_replication_delete.json";
    constexpr char EMPTY[] = "{\"kv_store\":{}}";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY, sizeof(EMPTY) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("local", "first"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_set("replicated", "second"));
    replication_test_state state{&store, 0, 0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.register_replication_sink(
        ft_nullptr, count_snapshot, &state, FT_TRUE));
    cma_set_alloc_limit(1);
    const int32_t local_delete = store.kv_delete("local");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, local_delete);
    FT_ASSERT(store.kv_get("local") == ft_nullptr);
    FT_ASSERT_EQ(1U, state.snapshots);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.register_replication_sink(
        count_valid_delete, ft_nullptr, &state, FT_FALSE));
    cma_set_alloc_limit(1);
    const int32_t replica_delete = store.kv_delete("replicated");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, replica_delete);
    FT_ASSERT(store.kv_get("replicated") != ft_nullptr);
    FT_ASSERT_EQ(0U, state.operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.kv_delete("replicated"));
    FT_ASSERT_EQ(1U, state.operations);
    kv_store_metrics metrics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_metrics(metrics));
    FT_ASSERT_EQ(2, metrics.delete_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}
