#ifndef KV_STORE_HPP
#define KV_STORE_HPP

#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../JSon/json.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Template/map.hpp"
#include "../Template/vector.hpp"
#include "../Parser/document_backend.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Threading/thread.hpp"
#include "../PThread/pthread.hpp"
#include "../Compatebility/sqlite_support.hpp"
#include <atomic>
#include <cstdint>

class ft_countdown_latch;

typedef enum e_kv_store_backend_type
{
    KV_STORE_BACKEND_JSON,
    KV_STORE_BACKEND_JSON_LINES,
#if SQLITE3_AVAILABLE
    KV_STORE_BACKEND_SQLITE,
#endif
    KV_STORE_BACKEND_MEMORY_MAPPED
}   kv_store_backend_type;

extern const char *g_kv_store_ttl_prefix;

typedef struct s_kv_store_metrics
{
    int64_t set_operations;
    int64_t delete_operations;
    int64_t get_hits;
    int64_t get_misses;
    int64_t prune_operations;
    int64_t pruned_entries;
    int64_t total_prune_duration_ms;
    int64_t last_prune_duration_ms;
}   kv_store_metrics;

typedef struct s_kv_store_snapshot_entry
{
    ft_string key;
    ft_string value;
    ft_bool has_expiration;
    int64_t expiration_timestamp;

    int32_t initialize(const s_kv_store_snapshot_entry &other) noexcept;
    int32_t initialize(s_kv_store_snapshot_entry &&other) noexcept;
    int32_t destroy() noexcept;
}   kv_store_snapshot_entry;

class kv_store_entry
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_string _value;
        ft_bool _has_expiration;
        int64_t _expiration_timestamp;
        uint8_t _initialised_state;
        mutable pt_recursive_mutex *_mutex;
        friend class kv_store;
        int32_t commit_value(ft_string &value, ft_bool update_expiration,
            ft_bool has_expiration, int64_t expiration_timestamp) noexcept;
    public:
        kv_store_entry() noexcept;
        kv_store_entry(const kv_store_entry &other) noexcept = delete;
        kv_store_entry(kv_store_entry &&other) noexcept = delete;
        kv_store_entry &operator=(const kv_store_entry &other) noexcept = delete;
        kv_store_entry &operator=(kv_store_entry &&other) noexcept = delete;
        ~kv_store_entry() noexcept;
        int32_t move(kv_store_entry &other) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t initialize() noexcept;
        int32_t initialize(const kv_store_entry &other) noexcept;
        int32_t initialize(kv_store_entry &&other) noexcept;
        int32_t destroy() noexcept;

        int32_t set_value(const ft_string &value) noexcept;
        int32_t set_value(const char *value_string) noexcept;
        int32_t copy_value(ft_string &destination) const noexcept;
        int32_t get_value_pointer(const char **value_pointer) const noexcept;

        int32_t configure_expiration(ft_bool has_expiration, int64_t expiration_timestamp) noexcept;
        int32_t has_expiration(ft_bool &has_expiration) const noexcept;
        int32_t get_expiration(int64_t &expiration_timestamp) const noexcept;
        int32_t is_expired(int64_t current_time, ft_bool &expired) const noexcept;

};

typedef enum e_kv_store_operation_type
{
    KV_STORE_OPERATION_TYPE_SET,
    KV_STORE_OPERATION_TYPE_DELETE
}   kv_store_operation_type;

typedef struct s_kv_store_operation
{
    kv_store_operation_type _type;
    ft_string _key;
    ft_string _value;
    ft_bool _has_value;
    ft_bool _has_ttl;
    int64_t _ttl_seconds;

    int32_t initialize(const s_kv_store_operation &other) noexcept;
    int32_t initialize(s_kv_store_operation &&other) noexcept;
    int32_t destroy() noexcept;
}   kv_store_operation;

typedef int32_t (*kv_store_replication_operations_callback)(const ft_vector<kv_store_operation> &operations, void *user_data);
typedef int32_t (*kv_store_replication_snapshot_callback)(const ft_vector<kv_store_snapshot_entry> &entries, void *user_data);

typedef struct s_kv_store_replication_sink
{
    kv_store_replication_operations_callback _operations_callback;
    kv_store_replication_snapshot_callback _snapshot_callback;
    void *_user_data;
}   kv_store_replication_sink;

class kv_store
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        ft_map<ft_string, kv_store_entry> _data;
        ft_string _file_path;
        ft_string _encryption_key;
        ft_string _encryption_algorithm_name;
        ft_bool _encryption_enabled;
        kv_store_backend_type _backend_type;
        int64_t _next_expiration;
        std::atomic<uint32_t> _background_interval_milliseconds;
        ft_countdown_latch *_background_wake;
        ft_thread _background_thread;
        mutable pt_recursive_mutex *_background_mutex;
        mutable pt_recursive_mutex *_mutex;
        mutable int64_t _metrics_set_operations;
        mutable int64_t _metrics_delete_operations;
        mutable int64_t _metrics_get_hits;
        mutable int64_t _metrics_get_misses;
        mutable int64_t _metrics_prune_operations;
        mutable int64_t _metrics_pruned_entries;
        mutable int64_t _metrics_total_prune_duration_ms;
        mutable int64_t _metrics_last_prune_duration_ms;
        ft_vector<kv_store_replication_sink> _replication_sinks;
        std::atomic<ft_size_t> _replication_operation_count;
        mutable pt_recursive_mutex *_replication_mutex;

        int32_t lock_store(ft_bool *lock_acquired) const noexcept;
        void unlock_store_guard(ft_bool lock_acquired, int32_t error_code) const noexcept;
        int32_t encrypt_value(const ft_string &plain_string, ft_string &encoded_string) const;
        int32_t decrypt_value(const ft_string &encoded_string, ft_string &plain_string) const;
        int32_t prune_expired_locked(ft_size_t *lookup_index = ft_nullptr);
        int32_t apply_set_operation_locked(ft_map<ft_string, kv_store_entry> &data,
            const kv_store_operation &operation,
            kv_store_entry *existing_entry);
        int32_t apply_existing_updates_locked(const ft_vector<kv_store_operation> &operations,
            ft_size_t operation_count);
        int32_t compute_expiration(int64_t ttl_seconds, ft_bool &has_expiration, int64_t &expiration_timestamp) const;
        int64_t current_time_seconds() const;
        int32_t parse_expiration_timestamp(const char *value_string, int64_t &expiration_timestamp) const;
        void record_set_operation() const noexcept;
        void record_delete_operation() const noexcept;
        void record_get_hit() const noexcept;
        void record_get_miss() const noexcept;
        void record_prune_metrics(int64_t removed_entries, int64_t duration_ms) const noexcept;
        int32_t start_background_thread_locked(uint32_t interval_milliseconds) noexcept;
        static void background_compaction_worker(kv_store *store, ft_countdown_latch *wake) noexcept;
        int32_t apply_snapshot_locked(const ft_vector<kv_store_snapshot_entry> &entries);
        int32_t apply_loaded_snapshot_locked(ft_vector<kv_store_snapshot_entry> &entries);
        template <typename Entry>
        int32_t apply_snapshot_entries_locked(Entry *entries, ft_size_t count);
        int32_t export_snapshot_locked(ft_vector<kv_store_snapshot_entry> &entries) const;
        int32_t load_json_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries);
        int32_t parse_json_groups(json_group *group_head, ft_vector<kv_store_snapshot_entry> &out_entries);
        int32_t build_json_entries(const ft_vector<kv_store_snapshot_entry> &entries,
            json_group **output) const;
        int32_t load_json_lines_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries);
        int32_t flush_json_lines_entries(const ft_vector<kv_store_snapshot_entry> &entries) const;
        int32_t write_json_lines_descriptor(int32_t descriptor,
            const ft_vector<kv_store_snapshot_entry> &entries) const;
        int32_t parse_json_line_record(const char *record,
            ft_vector<kv_store_snapshot_entry> &entries) const;
#if SQLITE3_AVAILABLE
        int32_t load_sqlite_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries);
        int32_t flush_sqlite_entries(const ft_vector<kv_store_snapshot_entry> &entries) const;
#endif
        int32_t load_memory_mapped_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries);
        int32_t flush_memory_mapped_entries(const ft_vector<kv_store_snapshot_entry> &entries) const;
        int32_t flush_backend_entries(const ft_vector<kv_store_snapshot_entry> &entries) const;
        int32_t load_backend_entries(kv_store_backend_type backend_type, const char *location, ft_vector<kv_store_snapshot_entry> &out_entries);
        int32_t assign_backend_location(const char *location);
        int32_t get_active_encryption_algorithm_name(ft_string &algorithm_name) const;
        int32_t lock_replication(ft_bool *lock_acquired) const noexcept;
        int32_t notify_replication_listeners(const ft_vector<kv_store_operation> &operations) const;
        int32_t dispatch_snapshot_to_sink(kv_store_replication_snapshot_callback snapshot_callback, void *user_data) const;
        int32_t initialize_members() noexcept;
        int32_t copy_configuration_locked(const kv_store &other) noexcept;

    public:
        kv_store() noexcept;
        kv_store(const kv_store &other) noexcept = delete;
        kv_store(kv_store &&other) noexcept = delete;
        kv_store &operator=(const kv_store &other) noexcept = delete;
        kv_store &operator=(kv_store &&other) noexcept = delete;
        ~kv_store() noexcept;
        int32_t move(kv_store &other) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t initialize(const kv_store &other) noexcept;
        int32_t initialize(kv_store &&other) noexcept;
        int32_t initialize(const char *file_path, const char *encryption_key = ft_nullptr, ft_bool enable_encryption = FT_FALSE, const char *encryption_algorithm_name = ft_nullptr);
        int32_t destroy();

        int32_t prune_expired();
        int32_t kv_set(const char *key_string, const char *value_string, int64_t ttl_seconds = -1);
        const char *kv_get(const char *key_string) const;
        ft_bool kv_contains(const char *key_string) const;
        int32_t kv_delete(const char *key_string);
        int32_t kv_flush() const;
        int32_t configure_encryption(const char *encryption_key, ft_bool enable_encryption, const char *encryption_algorithm_name = ft_nullptr);
        int32_t set_backend(kv_store_backend_type backend_type, const char *location);
        kv_store_backend_type get_backend() const;
        int32_t kv_apply(const ft_vector<kv_store_operation> &operations);
        int32_t kv_compare_and_swap(const char *key_string, const char *expected_value, const char *new_value, int64_t ttl_seconds = -1);
        int32_t get_metrics(kv_store_metrics &out_metrics) const;
        int32_t export_snapshot(ft_vector<kv_store_snapshot_entry> &out_entries) const;
        // Replace the output with nonempty suffixes of live keys, in store order.
        int32_t export_key_suffixes(const char *prefix, ft_vector<ft_string> &out_suffixes) const;
        int32_t export_snapshot_to_file(const char *file_path) const;
        int32_t import_snapshot(const ft_vector<kv_store_snapshot_entry> &entries);
        int32_t write_snapshot(ft_document_sink &sink) const;
        int32_t read_snapshot(ft_document_source &source);
        int32_t start_background_compaction(int64_t interval_seconds);
        int32_t stop_background_compaction();
        int32_t register_replication_sink(kv_store_replication_operations_callback operations_callback,
            kv_store_replication_snapshot_callback snapshot_callback, void *user_data,
            ft_bool ship_initial_snapshot);
        int32_t unregister_replication_sink(kv_store_replication_operations_callback operations_callback,
            kv_store_replication_snapshot_callback snapshot_callback, void *user_data);
        int32_t ship_replication_snapshot(kv_store_replication_snapshot_callback snapshot_callback, void *user_data) const;
};

int32_t kv_store_init_set_operation(kv_store_operation &operation, const char *key_string, const char *value_string, int64_t ttl_seconds = -1);
int32_t kv_store_init_delete_operation(kv_store_operation &operation, const char *key_string);


#endif
