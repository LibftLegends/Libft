#include <atomic>
#include <charconv>
#include <fcntl.h>
#include <sys/stat.h>
#include "../Compatebility/sqlite_support.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#if SQLITE3_AVAILABLE
# include <sqlite3.h>
#endif
#include "../Advanced/advanced.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_file.hpp"
#include "../Errno/errno.hpp"
#include "../GetNextLine/get_next_line.hpp"
#include "../Basic/basic.hpp"
#include "../JSon/json.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "readline_internal.hpp"
#include "readline.hpp"

#define RL_MAX_KEY_BINDINGS 64

struct rl_key_binding_entry
{
    int32_t key;
    t_rl_key_binding_callback callback;
    void *user_data;
};

static pt_recursive_mutex g_customization_mutex;
static ft_bool g_customization_mutex_initialised = FT_FALSE;
static rl_key_binding_entry g_key_bindings[RL_MAX_KEY_BINDINGS];
static int32_t g_key_binding_count = 0;
static t_rl_completion_callback g_completion_callback = ft_nullptr;
static void *g_completion_user_data = ft_nullptr;
static char *g_dynamic_suggestions[MAX_SUGGESTIONS];
static int32_t g_dynamic_suggestion_count = 0;

struct rl_history_path_context
{
    char *path;
};

#if SQLITE3_AVAILABLE
struct rl_history_sqlite_context
{
    sqlite3 *database;
};

#endif
typedef struct s_rl_history_backend
{
    const char *name;
    int32_t (*configure)(void **context_pointer, const char *location);
    void (*shutdown)(void *context_pointer);
    int32_t (*load)(void *context_pointer);
    int32_t (*save)(void *context_pointer);
}   rl_history_backend;

struct rl_history_backend_state
{
    const rl_history_backend *backend;
    void *backend_context;
};

static rl_history_backend_state g_history_backend_state = {ft_nullptr, ft_nullptr};
static std::atomic<ft_bool> g_history_auto_save_enabled{FT_FALSE};

static int32_t rl_customization_lock(ft_bool *lock_acquired)
{
    int32_t initialize_error;
    int32_t mutex_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (g_customization_mutex_initialised == FT_FALSE)
    {
        initialize_error = g_customization_mutex.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
        g_customization_mutex_initialised = FT_TRUE;
    }
    mutex_error = pt_recursive_mutex_lock_if_not_null(&g_customization_mutex);
    if (mutex_error != FT_ERR_SUCCESS)
        return (mutex_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

static int32_t rl_customization_unlock(ft_bool lock_acquired)
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (g_customization_mutex_initialised == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(&g_customization_mutex);
    return (FT_ERR_SUCCESS);
}

static int32_t rl_history_assign_path(char **target_path, const char *location)
{
    char *new_path;

    if (target_path == ft_nullptr || location == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    new_path = adv_strdup(location);
    if (new_path == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    if (*target_path != ft_nullptr)
        cma_free(*target_path);
    *target_path = new_path;
    return (FT_ERR_SUCCESS);
}

static int32_t rl_history_prepare_path_context(void **context_pointer, const char *location)
{
    rl_history_path_context *path_context;

    if (context_pointer == ft_nullptr || location == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    path_context = static_cast<rl_history_path_context *>(*context_pointer);
    if (path_context == ft_nullptr)
    {
        path_context = static_cast<rl_history_path_context *>(cma_malloc(sizeof(*path_context)));
        if (path_context == ft_nullptr)
        {
            return (FT_ERR_INTERNAL);
        }
        path_context->path = ft_nullptr;
        *context_pointer = path_context;
    }
    if (rl_history_assign_path(&path_context->path, location) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static int32_t rl_history_plain_configure(void **context_pointer, const char *location)
{
    if (rl_history_prepare_path_context(context_pointer, location) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static void rl_history_plain_shutdown(void *context_pointer)
{
    rl_history_path_context *path_context;

    if (context_pointer == ft_nullptr)
        return ;
    path_context = static_cast<rl_history_path_context *>(context_pointer);
    if (path_context->path != ft_nullptr)
        cma_free(path_context->path);
    cma_free(path_context);
    return ;
}

static int32_t rl_history_plain_load(void *context_pointer)
{
    rl_history_path_context *path_context;
    ft_file history_file;
    int32_t open_result;
    int32_t file_descriptor;

    path_context = static_cast<rl_history_path_context *>(context_pointer);
    if (path_context == ft_nullptr || path_context->path == ft_nullptr)
        return (FT_ERR_INTERNAL);
    open_result = history_file.open(path_context->path, O_RDONLY);
    if (open_result != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    rl_clear_history();
    file_descriptor = history_file.get_file_descriptor();
    if (file_descriptor == -1)
    {
        history_file.close();
        return (FT_ERR_INTERNAL);
    }
    while (1)
    {
        char *line_buffer;

        line_buffer = get_next_line(file_descriptor, 1024);
        if (line_buffer == ft_nullptr)
            break ;
        ft_size_t line_length;

        line_length = ft_strlen_size_t(line_buffer);
        if (line_length > 0 && line_buffer[line_length - 1] == '\n')
            line_buffer[line_length - 1] = '\0';
        rl_history_adopt_entry(line_buffer);
    }
    history_file.close();
    return (FT_ERR_SUCCESS);
}

static int32_t rl_history_plain_save(void *context_pointer)
{
    const auto *path_context = static_cast<const rl_history_path_context *>(context_pointer);
    if (path_context == ft_nullptr || path_context->path == ft_nullptr)
        return (FT_ERR_INTERNAL);
    ft_file history_file;
    if (history_file.open(path_context->path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    const int32_t file_descriptor = history_file.get_file_descriptor();
    if (file_descriptor < 0)
        return (FT_ERR_INTERNAL);

    char output[4096];
    ft_size_t used = 0;
    int32_t history_index = 0;
    while (history_index < history_count)
    {
        const char *entry = history[history_index++];
        if (entry == ft_nullptr)
            continue ;
        const ft_size_t length = ft_strlen_size_t(entry);
        if (length >= sizeof(output) - used)
        {
            if (su_write(file_descriptor, output, used) < 0)
                return (FT_ERR_INTERNAL);
            used = 0;
        }
        if (length >= sizeof(output))
        {
            if (su_write(file_descriptor, entry, length) < 0)
                return (FT_ERR_INTERNAL);
        }
        else
        {
            (void)ft_memcpy(output + used, entry, length);
            used += length;
        }
        output[used++] = '\n';
    }
    if (su_write(file_descriptor, output, used) < 0)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static int32_t rl_history_json_configure(void **context_pointer, const char *location)
{
    if (rl_history_prepare_path_context(context_pointer, location) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static void rl_history_json_shutdown(void *context_pointer)
{
    rl_history_plain_shutdown(context_pointer);
    return ;
}

static int32_t rl_history_json_load(void *context_pointer)
{
    rl_history_path_context *path_context;
    json_group *group_head;
    json_group *history_group;
    json_item *item_pointer;

    path_context = static_cast<rl_history_path_context *>(context_pointer);
    if (path_context == ft_nullptr || path_context->path == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    group_head = json_read_from_file(path_context->path);
    if (group_head == ft_nullptr)
        return (FT_ERR_SUCCESS);
    history_group = json_find_group(group_head, "history");
    if (history_group == ft_nullptr)
    {
        json_free_groups(group_head);
        return (FT_ERR_SUCCESS);
    }
    rl_clear_history();
    item_pointer = history_group->items;
    while (item_pointer != ft_nullptr)
    {
        char *entry = item_pointer->value;
        item_pointer->value = ft_nullptr;
        if (entry == ft_nullptr)
            entry = adv_strdup("");
        rl_history_adopt_entry(entry);
        item_pointer = item_pointer->next;
    }
    json_free_groups(group_head);
    return (FT_ERR_SUCCESS);
}

static ft_size_t rl_history_json_append(void *context, const char *bytes, ft_size_t length)
{
    auto *output = static_cast<ft_string *>(context);
    if (output->append(bytes, length) != FT_ERR_SUCCESS)
        return (0);
    return (length);
}

static int32_t rl_history_json_event(json_stream_writer &writer, json_stream_event_type type,
        const char *bytes = ft_nullptr, ft_size_t length = 0)
{
    const json_stream_event event{type, {bytes, length}, FT_FALSE};
    return (json_stream_writer_process(&writer, &event));
}

static int32_t rl_history_json_build(ft_string &output)
{
    json_stream_writer writer;
    if (json_stream_writer_init(&writer, rl_history_json_append, &output) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    int32_t result = FT_ERR_INTERNAL;
    if (rl_history_json_event(writer, JSON_STREAM_EVENT_BEGIN_DOCUMENT) == FT_ERR_SUCCESS
        && rl_history_json_event(writer, JSON_STREAM_EVENT_BEGIN_OBJECT) == FT_ERR_SUCCESS
        && rl_history_json_event(writer, JSON_STREAM_EVENT_KEY, "history", 7) == FT_ERR_SUCCESS
        && rl_history_json_event(writer, JSON_STREAM_EVENT_BEGIN_OBJECT) == FT_ERR_SUCCESS)
        result = FT_ERR_SUCCESS;
    int32_t history_index = 0;
    while (result == FT_ERR_SUCCESS && history_index < history_count)
    {
        const char *entry = history[history_index];
        if (entry == ft_nullptr)
            entry = "";
        char key[12];
        const auto conversion = std::to_chars(key, key + sizeof(key), history_index);
        if (conversion.ec != std::errc()
            || rl_history_json_event(writer, JSON_STREAM_EVENT_KEY, key,
                static_cast<ft_size_t>(conversion.ptr - key)) != FT_ERR_SUCCESS
            || rl_history_json_event(writer, JSON_STREAM_EVENT_STRING, entry,
                ft_strlen_size_t(entry)) != FT_ERR_SUCCESS)
            result = FT_ERR_INTERNAL;
        ++history_index;
    }
    if (result == FT_ERR_SUCCESS
        && (rl_history_json_event(writer, JSON_STREAM_EVENT_END_OBJECT) != FT_ERR_SUCCESS
            || rl_history_json_event(writer, JSON_STREAM_EVENT_END_OBJECT) != FT_ERR_SUCCESS
            || rl_history_json_event(writer, JSON_STREAM_EVENT_END_DOCUMENT) != FT_ERR_SUCCESS
            || json_stream_writer_finish(&writer) != FT_ERR_SUCCESS
            || output.append('\n') != FT_ERR_SUCCESS))
        result = FT_ERR_INTERNAL;
    json_stream_writer_destroy(&writer);
    return (result);
}

static int32_t rl_history_json_save(void *context_pointer)
{
    const auto *path_context = static_cast<const rl_history_path_context *>(context_pointer);
    if (path_context == ft_nullptr || path_context->path == ft_nullptr)
        return (FT_ERR_INTERNAL);
    ft_string output;
    if (output.initialize() != FT_ERR_SUCCESS || rl_history_json_build(output) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);

    const int32_t descriptor = su_open(path_context->path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (descriptor < 0)
        return (FT_ERR_INTERNAL);
    const int64_t written = su_write(descriptor, output.c_str(), output.size());
    const int32_t close_result = su_close(descriptor);
    if (written < 0 || close_result != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

#if SQLITE3_AVAILABLE
static int32_t rl_history_sqlite_configure(void **context_pointer, const char *location)
{
    if (context_pointer == ft_nullptr || location == ft_nullptr)
        return (FT_ERR_INTERNAL);
    sqlite3 *database_handle = ft_nullptr;
    if (sqlite3_open(location, &database_handle) != SQLITE_OK)
    {
        if (database_handle != ft_nullptr)
            sqlite3_close(database_handle);
        return (FT_ERR_INTERNAL);
    }
    if (sqlite3_exec(database_handle,
            "CREATE TABLE IF NOT EXISTS readline_history ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "entry TEXT NOT NULL);",
            ft_nullptr, ft_nullptr, ft_nullptr) != SQLITE_OK)
    {
        sqlite3_close(database_handle);
        return (FT_ERR_INTERNAL);
    }
    auto *sqlite_context = static_cast<rl_history_sqlite_context *>(*context_pointer);
    if (sqlite_context == ft_nullptr)
    {
        sqlite_context = static_cast<rl_history_sqlite_context *>(cma_malloc(sizeof(*sqlite_context)));
        if (sqlite_context == ft_nullptr)
        {
            sqlite3_close(database_handle);
            return (FT_ERR_INTERNAL);
        }
        *context_pointer = sqlite_context;
    }
    else
        sqlite3_close(sqlite_context->database);
    sqlite_context->database = database_handle;
    return (FT_ERR_SUCCESS);
}

static void rl_history_sqlite_shutdown(void *context_pointer)
{
    auto *sqlite_context = static_cast<rl_history_sqlite_context *>(context_pointer);
    if (sqlite_context == ft_nullptr)
        return ;
    sqlite3_close(sqlite_context->database);
    cma_free(sqlite_context);
    return ;
}

static int32_t rl_history_sqlite_load(void *context_pointer)
{
    rl_history_sqlite_context *sqlite_context;
    sqlite3_stmt *statement_handle;
    int32_t prepare_result;

    sqlite_context = static_cast<rl_history_sqlite_context *>(context_pointer);
    if (sqlite_context == ft_nullptr || sqlite_context->database == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    prepare_result = sqlite3_prepare_v2(sqlite_context->database,
            "SELECT entry FROM readline_history ORDER BY id ASC;",
            -1, &statement_handle, ft_nullptr);
    if (prepare_result != SQLITE_OK)
    {
        return (FT_ERR_INTERNAL);
    }
    rl_clear_history();
    while (1)
    {
        int32_t step_result;

        step_result = sqlite3_step(statement_handle);
        if (step_result == SQLITE_DONE)
            break ;
        if (step_result == SQLITE_ROW)
        {
            const uint8_t *text_pointer;
            const char *entry_text;

            text_pointer = sqlite3_column_text(statement_handle, 0);
            if (text_pointer == ft_nullptr)
                entry_text = "";
            else
                entry_text = reinterpret_cast<const char *>(text_pointer);
            rl_history_adopt_entry(adv_strdup(entry_text));
            continue ;
        }
        sqlite3_finalize(statement_handle);
        return (FT_ERR_INTERNAL);
    }
    sqlite3_finalize(statement_handle);
    return (FT_ERR_SUCCESS);
}

static int32_t rl_history_sqlite_save(void *context_pointer)
{
    rl_history_sqlite_context *sqlite_context;
    sqlite3_stmt *insert_statement;
    int32_t history_index;

    sqlite_context = static_cast<rl_history_sqlite_context *>(context_pointer);
    if (sqlite_context == ft_nullptr || sqlite_context->database == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    if (sqlite3_exec(sqlite_context->database, "BEGIN IMMEDIATE TRANSACTION;",
            ft_nullptr, ft_nullptr, ft_nullptr) != SQLITE_OK)
        return (FT_ERR_INTERNAL);
    if (sqlite3_exec(sqlite_context->database, "DELETE FROM readline_history;",
            ft_nullptr, ft_nullptr, ft_nullptr) != SQLITE_OK)
    {
        sqlite3_exec(sqlite_context->database, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        return (FT_ERR_INTERNAL);
    }
    if (sqlite3_prepare_v2(sqlite_context->database,
            "INSERT INTO readline_history(entry) VALUES (?1);",
            -1, &insert_statement, ft_nullptr) != SQLITE_OK)
    {
        sqlite3_exec(sqlite_context->database, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        return (FT_ERR_INTERNAL);
    }
    history_index = 0;
    while (history_index < history_count)
    {
        const char *history_entry;
        int32_t bind_result;
        int32_t step_result;

        history_entry = history[history_index];
        if (history_entry == ft_nullptr)
            history_entry = "";
        sqlite3_reset(insert_statement);
        // History remains alive until the statement is rebound or finalized.
        bind_result = sqlite3_bind_text(insert_statement, 1, history_entry, -1, SQLITE_STATIC);
        if (bind_result != SQLITE_OK)
        {
            sqlite3_finalize(insert_statement);
            sqlite3_exec(sqlite_context->database, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
            return (FT_ERR_INTERNAL);
        }
        step_result = sqlite3_step(insert_statement);
        if (step_result != SQLITE_DONE)
        {
            sqlite3_finalize(insert_statement);
            sqlite3_exec(sqlite_context->database, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
            return (FT_ERR_INTERNAL);
        }
        history_index += 1;
    }
    sqlite3_finalize(insert_statement);
    if (sqlite3_exec(sqlite_context->database, "COMMIT;",
            ft_nullptr, ft_nullptr, ft_nullptr) != SQLITE_OK)
    {
        sqlite3_exec(sqlite_context->database, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        return (FT_ERR_INTERNAL);
    }
    return (FT_ERR_SUCCESS);
}

static rl_history_backend g_history_sqlite_backend = {
    "sqlite",
    rl_history_sqlite_configure,
    rl_history_sqlite_shutdown,
    rl_history_sqlite_load,
    rl_history_sqlite_save
};
#endif

static rl_history_backend g_history_plain_backend = {
    "plain-text",
    rl_history_plain_configure,
    rl_history_plain_shutdown,
    rl_history_plain_load,
    rl_history_plain_save
};

static rl_history_backend g_history_json_backend = {
    "json",
    rl_history_json_configure,
    rl_history_json_shutdown,
    rl_history_json_load,
    rl_history_json_save
};

static const rl_history_backend *g_history_backend_catalog[] = {
    &g_history_plain_backend,
    &g_history_json_backend,
#if SQLITE3_AVAILABLE
    &g_history_sqlite_backend
#endif
};

static const rl_history_backend *rl_history_find_backend(const char *backend_name)
{
    ft_size_t backend_index;

    if (backend_name == ft_nullptr)
    {
        return (ft_nullptr);
    }
    backend_index = 0;
    while (backend_index < sizeof(g_history_backend_catalog) / sizeof(g_history_backend_catalog[0]))
    {
        const rl_history_backend *candidate_backend;

        candidate_backend = g_history_backend_catalog[backend_index];
        if (candidate_backend != ft_nullptr
            && candidate_backend->name != ft_nullptr
            && ft_strcmp(candidate_backend->name, backend_name) == FT_ERR_SUCCESS)
        {
            return (candidate_backend);
        }
        backend_index += 1;
    }
    return (ft_nullptr);
}

static int32_t rl_history_configure_backend_locked(const rl_history_backend *backend,
        const char *location)
{
    const rl_history_backend *existing_backend;
    void *existing_context;
    void *working_context;

    if (backend == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    existing_backend = g_history_backend_state.backend;
    existing_context = g_history_backend_state.backend_context;
    working_context = existing_context;
    if (backend != existing_backend)
        working_context = ft_nullptr;
    if (backend->configure != ft_nullptr)
    {
        if (backend->configure(&working_context, location) != FT_ERR_SUCCESS)
        {
            if (backend != existing_backend && working_context != ft_nullptr
                && backend->shutdown != ft_nullptr)
                backend->shutdown(working_context);
            return (FT_ERR_INTERNAL);
        }
    }
    if (backend != existing_backend)
    {
        if (existing_backend != ft_nullptr && existing_backend->shutdown != ft_nullptr)
            existing_backend->shutdown(existing_context);
    }
    g_history_backend_state.backend = backend;
    g_history_backend_state.backend_context = working_context;
    return (FT_ERR_SUCCESS);
}

static void rl_completion_reset_dynamic_matches_locked(void)
{
    int32_t index;

    index = 0;
    while (index < g_dynamic_suggestion_count)
    {
        if (g_dynamic_suggestions[index] != ft_nullptr)
        {
            cma_free(g_dynamic_suggestions[index]);
            g_dynamic_suggestions[index] = ft_nullptr;
        }
        index += 1;
    }
    g_dynamic_suggestion_count = 0;
    return ;
}

static int32_t rl_key_binding_lower_bound_locked(int32_t key)
{
    int32_t begin = 0;
    int32_t end = g_key_binding_count;
    while (begin < end)
    {
        const int32_t middle = begin + (end - begin) / 2;
        if (g_key_bindings[middle].key < key)
            begin = middle + 1;
        else
            end = middle;
    }
    return (begin);
}

int32_t rl_bind_key(int32_t key, t_rl_key_binding_callback callback, void *user_data)
{
    if (callback == ft_nullptr)
        return (FT_ERR_INTERNAL);
    ft_bool lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    const int32_t index = rl_key_binding_lower_bound_locked(key);
    if (index >= g_key_binding_count || g_key_bindings[index].key != key)
    {
        if (g_key_binding_count == RL_MAX_KEY_BINDINGS)
        {
            (void)rl_customization_unlock(lock_acquired);
            return (FT_ERR_INTERNAL);
        }
        if (index < g_key_binding_count)
            (void)ft_memmove(g_key_bindings + index + 1, g_key_bindings + index,
                static_cast<ft_size_t>(g_key_binding_count - index) * sizeof(g_key_bindings[0]));
        ++g_key_binding_count;
    }
    g_key_bindings[index] = {key, callback, user_data};
    (void)rl_customization_unlock(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t rl_unbind_key(int32_t key)
{
    ft_bool lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    const int32_t index = rl_key_binding_lower_bound_locked(key);
    if (index >= g_key_binding_count || g_key_bindings[index].key != key)
    {
        (void)rl_customization_unlock(lock_acquired);
        return (FT_ERR_INTERNAL);
    }
    --g_key_binding_count;
    if (index < g_key_binding_count)
        (void)ft_memmove(g_key_bindings + index, g_key_bindings + index + 1,
            static_cast<ft_size_t>(g_key_binding_count - index) * sizeof(g_key_bindings[0]));
    (void)rl_customization_unlock(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t rl_dispatch_custom_key(readline_state_t *state, const char *prompt, int32_t key, ft_bool *key_handled)
{
    if (key_handled == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *key_handled = FT_FALSE;
    ft_bool lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    const int32_t index = rl_key_binding_lower_bound_locked(key);
    if (index >= g_key_binding_count || g_key_bindings[index].key != key)
    {
        (void)rl_customization_unlock(lock_acquired);
        return (FT_ERR_SUCCESS);
    }
    const rl_key_binding_entry binding = g_key_bindings[index];
    (void)rl_customization_unlock(lock_acquired);
    const int32_t result = binding.callback(state, prompt, binding.user_data);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *key_handled = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

static int32_t rl_state_insert_text_locked(readline_state_t *state, const char *text)
{
    if (state->buffer == ft_nullptr || state->buffer_size <= 0 || state->position < 0)
        return (FT_ERR_INTERNAL);
    const ft_size_t capacity = static_cast<ft_size_t>(state->buffer_size);
    const auto *terminator = static_cast<const char *>(ft_memchr(state->buffer, '\0', capacity));
    if (terminator == ft_nullptr)
        return (FT_ERR_INTERNAL);
    const ft_size_t line_length = static_cast<ft_size_t>(terminator - state->buffer);
    const ft_size_t position = static_cast<ft_size_t>(state->position);
    if (position > line_length)
        return (FT_ERR_OUT_OF_RANGE);

    const uintptr_t buffer_address = reinterpret_cast<uintptr_t>(state->buffer);
    const uintptr_t text_address = reinterpret_cast<uintptr_t>(text);
    ft_size_t source_offset = 0;
    ft_size_t text_length;
    const ft_bool borrowed = (text_address >= buffer_address && text_address - buffer_address < capacity);
    if (borrowed == FT_TRUE)
    {
        source_offset = static_cast<ft_size_t>(text_address - buffer_address);
        if (source_offset > line_length)
            return (FT_ERR_OUT_OF_RANGE);
        text_length = line_length - source_offset;
    }
    else
        text_length = ft_strlen_size_t(text);
    if (text_length >= static_cast<ft_size_t>(FT_INT32_MAX) - line_length)
        return (FT_ERR_OUT_OF_RANGE);
    const ft_size_t required_size = line_length + text_length + 1;
    if (required_size > capacity)
    {
        int32_t new_size = state->buffer_size;
        while (static_cast<ft_size_t>(new_size) < required_size)
        {
            if (new_size > FT_INT32_MAX / 2)
                return (FT_ERR_OUT_OF_RANGE);
            new_size *= 2;
        }
        const int32_t resize_error = rl_resize_buffer(&state->buffer, &state->buffer_size, new_size);
        if (resize_error != FT_ERR_SUCCESS)
            return (resize_error);
    }

    char *const insertion = state->buffer + position;
    if (position < line_length)
        (void)ft_memmove(insertion + text_length, insertion, line_length - position + 1);
    else
        insertion[text_length] = '\0';
    if (borrowed == FT_TRUE)
    {
        // Bytes before the cursor stay in place. The suffix now starts after the insertion.
        ft_size_t prefix_length = 0;
        if (source_offset < position)
        {
            prefix_length = position - source_offset;
            (void)ft_memcpy(insertion, state->buffer + source_offset, prefix_length);
        }
        if (prefix_length < text_length)
            (void)ft_memcpy(insertion + prefix_length,
                state->buffer + source_offset + prefix_length + text_length, text_length - prefix_length);
    }
    else
        (void)ft_memcpy(insertion, text, text_length);
    state->position += static_cast<int32_t>(text_length);
    return (FT_ERR_SUCCESS);
}

int32_t rl_state_insert_text(readline_state_t *state, const char *text)
{
    if (state == ft_nullptr || text == ft_nullptr)
        return (FT_ERR_INTERNAL);
    if (text[0] == '\0')
        return (FT_ERR_SUCCESS);
    ft_bool lock_acquired = FT_FALSE;
    if (rl_state_lock(state, &lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    const int32_t result = rl_state_insert_text_locked(state, text);
    (void)rl_state_unlock(state, lock_acquired);
    return (result);
}

int32_t rl_state_delete_previous_grapheme(readline_state_t *state)
{
    ft_bool lock_acquired;
    int32_t result;

    if (state == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    lock_acquired = FT_FALSE;
    result = FT_ERR_SUCCESS;
    if (rl_state_lock(state, &lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (state->position > 0)
    {
        int32_t grapheme_start;
        int32_t grapheme_end;
        int32_t grapheme_columns;
        int32_t tail_length;

        if (rl_utf8_find_previous_grapheme(state->buffer, state->position, &grapheme_start, &grapheme_end, &grapheme_columns) != FT_ERR_SUCCESS)
        {
            result = FT_ERR_INTERNAL;
            goto cleanup;
        }
        tail_length = ft_strlen(&state->buffer[grapheme_end]) + 1;
        ft_memmove(&state->buffer[grapheme_start], &state->buffer[grapheme_end], tail_length);
        state->position = grapheme_start;
    }
cleanup:
    rl_state_unlock(state, lock_acquired);
    return (result);
}

int32_t rl_state_set_cursor(readline_state_t *state, int32_t new_position)
{
    ft_bool lock_acquired;
    int32_t buffer_length;

    if (state == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (new_position < 0)
    {
        return (FT_ERR_OUT_OF_RANGE);
    }
    lock_acquired = FT_FALSE;
    if (rl_state_lock(state, &lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    buffer_length = ft_strlen(state->buffer);
    if (new_position > buffer_length)
    {
        rl_state_unlock(state, lock_acquired);
        return (FT_ERR_OUT_OF_RANGE);
    }
    state->position = new_position;
    rl_state_unlock(state, lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t rl_state_get_cursor(readline_state_t *state, int32_t *out_position)
{
    ft_bool lock_acquired;

    if (state == ft_nullptr || out_position == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    lock_acquired = FT_FALSE;
    if (rl_state_lock(state, &lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    *out_position = state->position;
    rl_state_unlock(state, lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t rl_state_get_buffer(readline_state_t *state, const char **out_buffer)
{
    ft_bool lock_acquired;

    if (state == ft_nullptr || out_buffer == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    lock_acquired = FT_FALSE;
    if (rl_state_lock(state, &lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    *out_buffer = state->buffer;
    rl_state_unlock(state, lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t rl_state_refresh_display(readline_state_t *state, const char *prompt)
{
    ft_bool lock_acquired;
    int32_t result;
    int32_t columns_after_cursor;

    if (state == ft_nullptr || prompt == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    lock_acquired = FT_FALSE;
    result = FT_ERR_SUCCESS;
    if (rl_state_lock(state, &lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (rl_update_display_metrics(state) != FT_ERR_SUCCESS)
    {
        result = FT_ERR_INTERNAL;
        goto cleanup;
    }
    if (rl_clear_line_columns(prompt, state->prev_display_columns) != FT_ERR_SUCCESS)
    {
        result = FT_ERR_INTERNAL;
        goto cleanup;
    }
    pf_printf("%s%s", prompt, state->buffer);
    columns_after_cursor = state->prev_display_columns - state->display_pos;
    if (columns_after_cursor > 0)
        pf_printf("\033[%dD", columns_after_cursor);
    fflush(stdout);
cleanup:
    rl_state_unlock(state, lock_acquired);
    return (result);
}

int32_t rl_set_completion_callback(t_rl_completion_callback callback, void *user_data)
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    g_completion_callback = callback;
    g_completion_user_data = user_data;
    rl_completion_reset_dynamic_matches_locked();
    rl_customization_unlock(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t rl_completion_add_candidate(const char *candidate)
{
    ft_bool lock_acquired;
    char *duplicated;

    if (candidate == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (g_dynamic_suggestion_count >= MAX_SUGGESTIONS)
    {
        rl_customization_unlock(lock_acquired);
        return (FT_ERR_INTERNAL);
    }
    duplicated = adv_strdup(candidate);
    if (duplicated == ft_nullptr)
    {
        rl_customization_unlock(lock_acquired);
        return (FT_ERR_INTERNAL);
    }
    g_dynamic_suggestions[g_dynamic_suggestion_count] = duplicated;
    g_dynamic_suggestion_count += 1;
    rl_customization_unlock(lock_acquired);
    return (FT_ERR_SUCCESS);
}

void rl_completion_reset_dynamic_matches(void)
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    rl_completion_reset_dynamic_matches_locked();
    rl_customization_unlock(lock_acquired);
    return ;
}

int32_t rl_completion_prepare_candidates(const char *buffer, int32_t cursor_position,
        const char *prefix, int32_t prefix_length)
{
    ft_bool lock_acquired;
    t_rl_completion_callback callback;
    void *user_data;
    int32_t callback_result;

    (void)prefix_length;
    if (buffer == ft_nullptr || prefix == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    rl_completion_reset_dynamic_matches_locked();
    callback = g_completion_callback;
    user_data = g_completion_user_data;
    rl_customization_unlock(lock_acquired);
    if (callback == ft_nullptr)
    {
        return (FT_ERR_SUCCESS);
    }
    callback_result = callback(buffer, cursor_position, prefix, user_data);
    if (callback_result != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

int32_t rl_completion_get_dynamic_count(int32_t *count_out)
{
    ft_bool lock_acquired;

    if (count_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    *count_out = g_dynamic_suggestion_count;
    rl_customization_unlock(lock_acquired);
    return (FT_ERR_SUCCESS);
}

char *rl_completion_get_dynamic_match(int32_t index)
{
    ft_bool lock_acquired;
    char *result;

    lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (index < 0 || index >= g_dynamic_suggestion_count)
    {
        rl_customization_unlock(lock_acquired);
        return (ft_nullptr);
    }
    result = g_dynamic_suggestions[index];
    rl_customization_unlock(lock_acquired);
    return (result);
}

int32_t rl_history_set_backend(const char *backend_name, const char *location)
{
    ft_bool lock_acquired;
    const rl_history_backend *backend_pointer;
    int32_t configure_result;

    lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    backend_pointer = rl_history_find_backend(backend_name);
    if (backend_pointer == ft_nullptr)
    {
        rl_customization_unlock(lock_acquired);
        return (FT_ERR_INTERNAL);
    }
    configure_result = rl_history_configure_backend_locked(backend_pointer, location);
    rl_customization_unlock(lock_acquired);
    if (configure_result != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

const char *rl_history_get_backend(void)
{
    ft_bool lock_acquired;
    const char *backend_name;

    lock_acquired = FT_FALSE;
    backend_name = ft_nullptr;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (g_history_backend_state.backend == ft_nullptr
        || g_history_backend_state.backend->name == ft_nullptr)
    {
        rl_customization_unlock(lock_acquired);
        return (ft_nullptr);
    }
    backend_name = g_history_backend_state.backend->name;
    rl_customization_unlock(lock_acquired);
    return (backend_name);
}

int32_t rl_history_set_storage_path(const char *file_path)
{
    return (rl_history_set_backend("plain-text", file_path));
}

int32_t rl_history_enable_auto_save(ft_bool enabled)
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    g_history_auto_save_enabled.store(enabled, std::memory_order_relaxed);
    rl_customization_unlock(lock_acquired);
    return (FT_ERR_SUCCESS);
}

static int32_t rl_history_run(ft_bool load, ft_bool automatic)
{
    ft_bool lock_acquired = FT_FALSE;
    if (rl_customization_lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    const rl_history_backend *backend = g_history_backend_state.backend;
    int32_t result = FT_ERR_INTERNAL;
    if (automatic == FT_TRUE && !g_history_auto_save_enabled.load(std::memory_order_relaxed))
        result = FT_ERR_SUCCESS;
    else if (backend != ft_nullptr)
    {
        if (load == FT_TRUE)
            result = backend->load(g_history_backend_state.backend_context);
        else
            result = backend->save(g_history_backend_state.backend_context);
    }
    (void)rl_customization_unlock(lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

int32_t rl_history_load(void)
{
    return (rl_history_run(FT_TRUE, FT_FALSE));
}

int32_t rl_history_save(void)
{
    return (rl_history_run(FT_FALSE, FT_FALSE));
}

void rl_history_notify_updated(void)
{
    if (g_history_auto_save_enabled.load(std::memory_order_relaxed))
        (void)rl_history_run(FT_FALSE, FT_TRUE);
    return ;
}
