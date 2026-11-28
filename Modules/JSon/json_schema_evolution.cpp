#include "json_schema_evolution.hpp"
#include "document.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include <algorithm>
#include <atomic>
#include <memory>
#include <new>
#include <string_view>

using json_migration_callback = ft_function<int32_t(json_document &)>;

struct json_schema_migration_step
{
    int32_t from_version;
    int32_t to_version;
    json_migration_callback callback;
};

struct json_schema_migrations
{
    ft_string name;
    ft_vector<json_schema_migration_step *> steps;
    int32_t latest_version = 0;

    ~json_schema_migrations()
    {
        if (this->steps.is_initialised() == FT_CLASS_STATE_INITIALISED)
        {
            auto entry = this->steps.begin();
            const auto end = this->steps.end();
            while (entry != end)
                delete *entry++;
        }
        return ;
    }
};

struct json_schema_registry
{
    ft_vector<json_schema_migrations *> schemas;
    std::atomic<pt_recursive_mutex *> mutex{nullptr};

    ~json_schema_registry()
    {
        if (this->schemas.is_initialised() == FT_CLASS_STATE_INITIALISED)
        {
            auto entry = this->schemas.begin();
            const auto end = this->schemas.end();
            while (entry != end)
                delete *entry++;
        }
        delete this->mutex.load(std::memory_order_relaxed);
        return ;
    }
};

struct json_planned_migration
{
    const json_migration_callback *source;
    json_migration_callback callback;
};

static json_schema_registry &json_schema_get_registry()
{
    static json_schema_registry registry;
    return (registry);
}

static int32_t json_schema_registry_lock(json_schema_registry &registry,
    pt_recursive_mutex *&mutex) noexcept
{
    mutex = registry.mutex.load(std::memory_order_acquire);
    if (mutex == ft_nullptr)
    {
        pt_recursive_mutex *candidate = new (std::nothrow) pt_recursive_mutex();
        if (candidate == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        const int32_t error = candidate->initialize();
        if (error != FT_ERR_SUCCESS)
        {
            delete candidate;
            return (error);
        }
        if (registry.mutex.compare_exchange_strong(mutex, candidate,
                std::memory_order_acq_rel, std::memory_order_acquire))
            mutex = candidate;
        else
            delete candidate;
    }
    return (pt_recursive_mutex_lock_if_not_null(mutex));
}

static int32_t json_schema_get_identifier(const ft_string &schema_name,
    std::string_view &name) noexcept
{
    const ft_size_t length = schema_name.size();
    const char *characters = schema_name.data();
    if (length == 0U || characters == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_size_t index = 0U;
    while (index < length)
    {
        const char character = characters[index];
        if (!((character >= 'a' && character <= 'z')
                || (character >= 'A' && character <= 'Z')
                || (character >= '0' && character <= '9')
                || character == '_' || character == '-' || character == '.'))
            return (FT_ERR_INVALID_ARGUMENT);
        ++index;
    }
    name = std::string_view(characters, length);
    return (FT_ERR_SUCCESS);
}

static json_schema_migrations **json_schema_find_name(json_schema_registry &registry,
    std::string_view name)
{
    return (std::lower_bound(registry.schemas.begin(), registry.schemas.end(), name,
        [](const json_schema_migrations *entry, std::string_view match)
        {
            return (std::string_view(entry->name.c_str(), entry->name.size()) < match);
        }));
}

static json_schema_migrations *json_schema_find_registered(json_schema_registry &registry,
    std::string_view name)
{
    if (registry.schemas.is_initialised() != FT_CLASS_STATE_INITIALISED)
        return (ft_nullptr);
    auto entry = json_schema_find_name(registry, name);
    if (entry == registry.schemas.end() || (*entry)->name != name.data())
        return (ft_nullptr);
    return (*entry);
}

static json_schema_migration_step **json_schema_find_version(json_schema_migrations &schema,
    int32_t version)
{
    return (std::lower_bound(schema.steps.begin(), schema.steps.end(), version,
        [](const json_schema_migration_step *step, int32_t match)
        {
            return (step->from_version < match);
        }));
}

static int32_t json_schema_register_locked(json_schema_registry &registry, std::string_view name,
    std::unique_ptr<json_schema_migration_step> &step)
{
    if (registry.schemas.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        const int32_t error = registry.schemas.initialize();
        if (error != FT_ERR_SUCCESS)
            return (error);
    }
    auto entry = json_schema_find_name(registry, name);
    json_schema_migrations *schema;
    std::unique_ptr<json_schema_migrations> new_schema;
    if (entry != registry.schemas.end() && (*entry)->name == name.data())
        schema = *entry;
    else
    {
        new_schema.reset(new (std::nothrow) json_schema_migrations());
        if (!new_schema)
            return (FT_ERR_NO_MEMORY);
        schema = new_schema.get();
        int32_t error = schema->name.initialize(name.data());
        if (error == FT_ERR_SUCCESS)
            error = schema->steps.initialize();
        if (error != FT_ERR_SUCCESS)
            return (error);
    }
    auto insertion = json_schema_find_version(*schema, step->from_version);
    if (insertion != schema->steps.end() && (*insertion)->from_version == step->from_version)
        return (FT_ERR_ALREADY_EXISTS);
    if (insertion == schema->steps.end())
        schema->steps.push_back(step.get());
    else
        schema->steps.insert(insertion, step.get());
    const int32_t error = schema->steps.get_error();
    if (error != FT_ERR_SUCCESS)
        return (error);
    if (schema->latest_version < step->to_version)
        schema->latest_version = step->to_version;
    step.release();
    if (new_schema)
    {
        if (entry == registry.schemas.end())
            registry.schemas.push_back(schema);
        else
            registry.schemas.insert(entry, schema);
        const int32_t insert_error = registry.schemas.get_error();
        if (insert_error != FT_ERR_SUCCESS)
            return (insert_error);
        new_schema.release();
    }
    return (FT_ERR_SUCCESS);
}

int32_t json_register_schema_migration(const ft_string &schema_name,
    int32_t from_version, int32_t to_version, const json_migration_callback &callback) noexcept
{
    std::string_view name;
    const int32_t validate_error = json_schema_get_identifier(schema_name, name);
    if (validate_error != FT_ERR_SUCCESS)
        return (validate_error);
    if (from_version < 0 || to_version <= from_version || !callback)
        return (FT_ERR_INVALID_ARGUMENT);
    std::unique_ptr<json_schema_migration_step> step(new (std::nothrow)
        json_schema_migration_step{from_version, to_version, callback});
    if (!step || !step->callback)
        return (FT_ERR_NO_MEMORY);
    json_schema_registry &registry = json_schema_get_registry();
    pt_recursive_mutex *mutex;
    const int32_t lock_error = json_schema_registry_lock(registry, mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t error = json_schema_register_locked(registry, name, step);
    (void)pt_recursive_mutex_unlock_if_not_null(mutex);
    return (error);
}

static int32_t json_schema_plan_locked(json_schema_registry &registry, std::string_view name,
    int32_t version, int32_t target, ft_vector<json_planned_migration> &plan)
{
    json_schema_migrations *schema = json_schema_find_registered(registry, name);
    if (schema == ft_nullptr || target > schema->latest_version)
        return (FT_ERR_NOT_FOUND);
    while (version < target)
    {
        auto step = json_schema_find_version(*schema, version);
        if (step == schema->steps.end() || (*step)->from_version != version
            || (*step)->to_version > target)
            return (FT_ERR_NOT_FOUND);
        const int32_t error = plan.push_back(json_planned_migration{&(*step)->callback, {}});
        if (error != FT_ERR_SUCCESS)
            return (error);
        version = (*step)->to_version;
    }
    return (FT_ERR_SUCCESS);
}

int32_t json_apply_schema_migrations(json_document &document, const ft_string &schema_name,
    int32_t current_version, int32_t target_version) noexcept
{
    std::string_view name;
    const int32_t validate_error = json_schema_get_identifier(schema_name, name);
    if (validate_error != FT_ERR_SUCCESS)
        return (validate_error);
    if (current_version < 0 || target_version < current_version)
        return (FT_ERR_INVALID_ARGUMENT);
    if (current_version == target_version)
        return (FT_ERR_SUCCESS);
    ft_vector<json_planned_migration> plan;
    const int32_t initialize_error = plan.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    json_schema_registry &registry = json_schema_get_registry();
    pt_recursive_mutex *mutex;
    const int32_t lock_error = json_schema_registry_lock(registry, mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t plan_error = json_schema_plan_locked(registry, name, current_version,
        target_version, plan);
    (void)pt_recursive_mutex_unlock_if_not_null(mutex);
    if (plan_error != FT_ERR_SUCCESS)
        return (plan_error);

    // Registrations never erase steps. Copy callables outside the registry lock.
    auto entry = plan.begin();
    const auto end = plan.end();
    while (entry != end)
    {
        entry->callback = *entry->source;
        if (!entry->callback)
            return (FT_ERR_NO_MEMORY);
        ++entry;
    }
    entry = plan.begin();
    while (entry != end)
    {
        const int32_t error = entry->callback(document);
        if (error != FT_ERR_SUCCESS)
            return (error);
        ++entry;
    }
    return (FT_ERR_SUCCESS);
}

int32_t json_get_latest_schema_version(const ft_string &schema_name, int32_t *out_version) noexcept
{
    if (out_version == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    std::string_view name;
    const int32_t validate_error = json_schema_get_identifier(schema_name, name);
    if (validate_error != FT_ERR_SUCCESS)
        return (validate_error);
    json_schema_registry &registry = json_schema_get_registry();
    pt_recursive_mutex *mutex;
    const int32_t lock_error = json_schema_registry_lock(registry, mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const json_schema_migrations *schema = json_schema_find_registered(registry, name);
    int32_t error = FT_ERR_NOT_FOUND;
    if (schema != ft_nullptr)
    {
        *out_version = schema->latest_version;
        error = FT_ERR_SUCCESS;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(mutex);
    return (error);
}
