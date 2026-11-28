#include "logger_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "../Template/move.hpp"
#include <new>

static pthread_key_t g_context_key;
static pthread_once_t g_context_key_once = PTHREAD_ONCE_INIT;
static int32_t g_context_key_error = FT_ERR_INTERNAL;
static std::atomic<uint64_t> g_context_cleanup_generation{0U};
static thread_local uint64_t g_context_pruned_generation = 0U;

static void logger_context_release_group(s_log_context_group *group)
{
    if (group != ft_nullptr
        && group->references.fetch_sub(1U, std::memory_order_acq_rel) == 1U)
        delete group;
    return ;
}

static void logger_context_attach_entry(s_log_context_entry &entry, s_log_context_group *group)
{
    entry.group = group;
    if (group != ft_nullptr)
    {
        group->references.fetch_add(1U, std::memory_order_relaxed);
        group->entries.fetch_add(1U, std::memory_order_relaxed);
    }
    return ;
}

static void logger_context_detach_entry(s_log_context_entry &entry)
{
    s_log_context_group *group = entry.group;
    entry.group = ft_nullptr;
    if (group != ft_nullptr)
    {
        group->entries.fetch_sub(1U, std::memory_order_release);
        logger_context_release_group(group);
    }
    return ;
}

s_log_context_entry::~s_log_context_entry() noexcept
{
    logger_context_detach_entry(*this);
    return ;
}

int32_t s_log_context_entry::initialize(
    const s_log_context_entry &other) noexcept
{
    int32_t error_code;

    error_code = this->key.initialize(other.key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->value.initialize(other.value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->has_value = other.has_value;
    logger_context_attach_entry(*this, other.group);
    return (FT_ERR_SUCCESS);
}

int32_t s_log_context_entry::initialize(s_log_context_entry &&other) noexcept
{
    int32_t error = this->key.move(other.key);
    if (error == FT_ERR_SUCCESS)
        error = this->value.move(other.value);
    if (error != FT_ERR_SUCCESS)
        return (error);
    this->has_value = other.has_value;
    logger_context_detach_entry(*this);
    this->group = other.group;
    other.group = ft_nullptr;
    other.has_value = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t s_log_context_entry::destroy() noexcept
{
    int32_t first_error;
    int32_t error_code;

    first_error = this->key.destroy();
    error_code = this->value.destroy();
    if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    this->has_value = FT_FALSE;
    logger_context_detach_entry(*this);
    return (first_error);
}

static void logger_context_destroy_entries(void *data)
{
    delete static_cast<ft_vector<s_log_context_entry> *>(data);
    return ;
}

static void logger_context_create_key()
{
    if (pthread_key_create(&g_context_key, logger_context_destroy_entries) == 0)
        g_context_key_error = FT_ERR_SUCCESS;
    return ;
}

static ft_vector<s_log_context_entry> *logger_context_entries()
{
    if (pthread_once(&g_context_key_once, logger_context_create_key) != 0
        || g_context_key_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    return (static_cast<ft_vector<s_log_context_entry> *>(pthread_getspecific(g_context_key)));
}

static int32_t logger_context_ensure_entries_ready(ft_vector<s_log_context_entry> *&entries)
{
    entries = logger_context_entries();
    if (entries != ft_nullptr)
        return (FT_ERR_SUCCESS);
    if (g_context_key_error != FT_ERR_SUCCESS)
        return (g_context_key_error);
    entries = new (std::nothrow) ft_vector<s_log_context_entry>();
    if (entries == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    int32_t error = entries->initialize();
    if (error == FT_ERR_SUCCESS && pthread_setspecific(g_context_key, entries) != 0)
        error = FT_ERR_INTERNAL;
    if (error != FT_ERR_SUCCESS)
        delete entries;
    return (error);
}

static int32_t logger_context_rollback(ft_vector<s_log_context_entry> &entries,
    ft_size_t count)
{
    while (count != 0U)
    {
        entries.pop_back();
        const int32_t error = entries.get_error();
        if (error != FT_ERR_SUCCESS)
            return (error);
        count -= 1U;
    }
    return (FT_ERR_SUCCESS);
}

static ft_bool logger_context_entry_visible(const s_log_context_entry &entry)
{
    return (entry.group == ft_nullptr
        || entry.group->owners.load(std::memory_order_acquire) != 0U
        || entry.group->released.load(std::memory_order_acquire) != FT_FALSE);
}

static void logger_context_prune(ft_vector<s_log_context_entry> &context)
{
    const uint64_t generation = g_context_cleanup_generation.load(std::memory_order_acquire);
    if (g_context_pruned_generation == generation)
        return ;
    g_context_pruned_generation = generation;
    const ft_size_t count = context.size();
    auto entries = context.begin();
    ft_size_t read_index = 0U;
    ft_size_t write_index = 0U;
    while (read_index < count)
    {
        if (logger_context_entry_visible(entries[read_index]) != FT_FALSE)
        {
            if (write_index != read_index)
            {
                if (entries[write_index].initialize(ft_move(entries[read_index])) != FT_ERR_SUCCESS)
                    return ;
            }
            write_index += 1U;
        }
        read_index += 1U;
    }
    (void)logger_context_rollback(context, count - write_index);
    return ;
}

void logger_context_retain_group(s_log_context_group *group)
{
    if (group != ft_nullptr)
    {
        group->references.fetch_add(1U, std::memory_order_relaxed);
        group->owners.fetch_add(1U, std::memory_order_relaxed);
    }
    return ;
}

void logger_context_drop_group(s_log_context_group *group, ft_bool keep_fields)
{
    if (group != ft_nullptr)
    {
        if (keep_fields != FT_FALSE)
            group->released.store(FT_TRUE, std::memory_order_release);
        if (group->owners.fetch_sub(1U, std::memory_order_acq_rel) == 1U
            && group->released.load(std::memory_order_acquire) == FT_FALSE)
            g_context_cleanup_generation.fetch_add(1U, std::memory_order_release);
        logger_context_release_group(group);
    }
    return ;
}

static int32_t logger_context_capture_field(s_log_context_entry &entry,
    const s_log_field &field)
{
    ft_bool acquired = FT_FALSE;
    int32_t error = log_field_lock(&field, &acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    if (field.key == ft_nullptr)
        error = FT_ERR_INVALID_ARGUMENT;
    else
    {
        error = entry.key.initialize(field.key);
        if (error == FT_ERR_SUCCESS)
            error = entry.value.initialize(field.value);
        entry.has_value = field.value != ft_nullptr;
    }
    log_field_unlock(&field, acquired);
    return (error);
}

int32_t logger_context_push(const s_log_field *fields, ft_size_t field_count,
    s_log_context_group *group)
{
    if (field_count == 0U)
        return (FT_ERR_SUCCESS);
    if (fields == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_vector<s_log_context_entry> *entries;
    int32_t error = logger_context_ensure_entries_ready(entries);
    if (error != FT_ERR_SUCCESS)
        return (error);
    logger_context_prune(*entries);
    ft_size_t index = 0U;
    while (index < field_count)
    {
        s_log_context_entry entry;
        error = logger_context_capture_field(entry, fields[index]);
        if (error == FT_ERR_SUCCESS)
        {
            logger_context_attach_entry(entry, group);
            error = entries->push_back(ft_move(entry));
        }
        if (error != FT_ERR_SUCCESS)
        {
            (void)logger_context_rollback(*entries, index);
            return (error);
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

void logger_context_pop(ft_size_t entry_count)
{
    if (entry_count != 0U)
    {
        auto *entries = logger_context_entries();
        if (entries != ft_nullptr)
        {
            logger_context_prune(*entries);
            const ft_size_t available = entries->size();
            if (entry_count > available)
                entry_count = available;
            (void)logger_context_rollback(*entries, entry_count);
        }
    }
    return ;
}

static ft_bool logger_context_value_needs_quotes(const char *value)
{
    ft_size_t entry_index;

    if (!value)
        return (FT_FALSE);
    entry_index = 0;
    while (value[entry_index] != '\0')
    {
        unsigned char character;

        character = static_cast<unsigned char>(value[entry_index]);
        if (character <= ' ' || character == '"' || character == '\\' || character == '=')
            return (FT_TRUE);
        entry_index += 1;
    }
    return (FT_FALSE);
}

static int32_t logger_context_append_flat_value(ft_string &output, const char *value)
{
    if (value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (logger_context_value_needs_quotes(value) == FT_FALSE)
        return (output.append(value));
    return (logger_append_quoted_value(output, value, FT_FALSE));
}

int32_t logger_context_format_flat(ft_string &output, const s_log_redaction_snapshot *snapshot)
{
    ft_size_t entry_count;
    ft_size_t entry_index;
    ft_bool first_entry;
    int32_t error_code_value;

    error_code_value = output.clear();
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    auto *context = logger_context_entries();
    if (context == ft_nullptr)
        return (FT_ERR_SUCCESS);
    logger_context_prune(*context);
    entry_count = context->size();
    error_code_value = context->get_error();
    if (error_code_value != FT_ERR_SUCCESS)
    {
        return (error_code_value);
    }
    if (entry_count == 0)
    {
        return (FT_ERR_SUCCESS);
    }
    entry_index = 0;
    first_entry = FT_TRUE;
    while (entry_index < entry_count)
    {
        const s_log_context_entry &entry = (*context)[entry_index];

        if (logger_context_entry_visible(entry) == FT_FALSE)
        {
            entry_index += 1U;
            continue ;
        }

        error_code_value = context->get_error();
        if (error_code_value != FT_ERR_SUCCESS)
        {
            return (error_code_value);
        }
        ft_string key_storage;
        ft_string value_storage;
        const char *original_key = entry.key.c_str();
        const char *key = original_key;
        const char *value = ft_nullptr;
        if (entry.has_value != FT_FALSE)
            value = entry.value.c_str();
        if (snapshot != ft_nullptr)
        {
            error_code_value = logger_redact_log_value(snapshot, key, key_storage);
            if (error_code_value == FT_ERR_SUCCESS)
                error_code_value = logger_redact_log_value(snapshot, value, value_storage);
            if (error_code_value != FT_ERR_SUCCESS)
                return (error_code_value);
        }
        if (!first_entry)
        {
            error_code_value = output.append(' ');
            if (error_code_value != FT_ERR_SUCCESS)
            {
                return (error_code_value);
            }
        }
        else
            first_entry = FT_FALSE;
        if (key == original_key)
            error_code_value = output.append(entry.key);
        else
            error_code_value = logger_context_append_flat_value(output, key);
        if (error_code_value != FT_ERR_SUCCESS)
        {
            return (error_code_value);
        }
        if (entry.has_value)
        {
            error_code_value = output.append('=');
            if (error_code_value != FT_ERR_SUCCESS)
            {
                return (error_code_value);
            }
            error_code_value = logger_context_append_flat_value(output, value);
            if (error_code_value != FT_ERR_SUCCESS)
                return (error_code_value);
        }
        entry_index += 1;
    }
    return (FT_ERR_SUCCESS);
}

int32_t logger_context_append_json(ft_string &output, ft_bool &first_field,
    const s_log_redaction_snapshot *snapshot)
{
    auto *context = logger_context_entries();
    if (context == ft_nullptr)
        return (FT_ERR_SUCCESS);
    logger_context_prune(*context);
    const ft_size_t count = context->size();
    const auto entries = context->begin();
    ft_size_t index = 0U;
    while (index < count)
    {
        const s_log_context_entry &entry = entries[index];

        if (logger_context_entry_visible(entry) == FT_FALSE)
        {
            index += 1U;
            continue ;
        }
        const char *value = ft_nullptr;
        if (entry.has_value != FT_FALSE)
            value = entry.value.c_str();
        const int32_t error = logger_append_json_field(output, entry.key.c_str(),
            value, first_field, snapshot);
        if (error != FT_ERR_SUCCESS)
            return (error);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

void logger_context_clear()
{
    auto *context = logger_context_entries();
    if (context != ft_nullptr)
    {
        if (pthread_setspecific(g_context_key, ft_nullptr) == 0)
            delete context;
        else
            context->clear();
    }
    return ;
}

int32_t ft_log_context_push(const s_log_field *fields, ft_size_t field_count)
{
    if (logger_context_push(fields, field_count, ft_nullptr) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

int32_t ft_log_context_push_field(const char *key, const char *value)
{
    s_log_field field;

    if (key == ft_nullptr || value == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    field.key = key;
    field.value = value;
    return (ft_log_context_push(&field, 1U));
}

void ft_log_context_pop(ft_size_t field_count)
{
    logger_context_pop(field_count);
    return ;
}

void ft_log_context_clear()
{
    logger_context_clear();
    return ;
}
