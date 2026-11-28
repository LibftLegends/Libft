#include "system_utils.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <vector>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

typedef struct s_su_health_check_entry
{
    ft_string           name;
    t_su_health_check   callback;
    void                *context;
}   t_su_health_check_entry;

using su_health_entry = std::shared_ptr<const s_su_health_check_entry>;

static std::vector<su_health_entry> g_su_health_checks;
static std::mutex                  g_su_health_checks_mutex;

static int32_t su_health_execute_entry(const s_su_health_check_entry &entry,
    t_su_health_check_result *result)
{
    ft_string    detail;
    ft_bool         healthy;
    int32_t          callback_result;
    int32_t          error_code;

    healthy = FT_TRUE;
    error_code = FT_ERR_SUCCESS;
    if (detail.initialize() != FT_ERR_SUCCESS)
    {
        healthy = FT_FALSE;
        error_code = FT_ERR_NO_MEMORY;
    }
    if (entry.callback == ft_nullptr)
    {
        healthy = FT_FALSE;
        error_code = FT_ERR_INVALID_POINTER;
    }
    else if (error_code == FT_ERR_SUCCESS)
    {
        callback_result = entry.callback(entry.context, detail);
        if (callback_result != 0)
        {
            healthy = FT_FALSE;
            error_code = FT_ERR_INTERNAL;
        }
    }
    if (result != ft_nullptr)
    {
        if (result->name.is_initialised() != FT_CLASS_STATE_INITIALISED)
            (void)result->name.initialize();
        if (result->detail.is_initialised() != FT_CLASS_STATE_INITIALISED)
            (void)result->detail.initialize();
        result->name = entry.name;
        result->healthy = healthy;
        result->detail = detail;
        result->error_code = error_code;
    }
    if (healthy == FT_FALSE)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t su_health_register_check(const char *name, t_su_health_check check, void *context)
{
    if (name == ft_nullptr || name[0] == '\0')
        return (-1);
    if (check == ft_nullptr)
        return (-1);
    g_su_health_checks_mutex.lock();
    ft_size_t index = 0;
    while (index < g_su_health_checks.size())
    {
        if (g_su_health_checks[index]->name == name)
        {
            g_su_health_checks_mutex.unlock();
            return (-1);
        }
        index += 1;
    }
    try
    {
        auto stored_entry = std::make_shared<s_su_health_check_entry>();
        if (stored_entry->name.initialize(name) != FT_ERR_SUCCESS)
        {
            g_su_health_checks_mutex.unlock();
            return (-1);
        }
        stored_entry->callback = check;
        stored_entry->context = context;
        g_su_health_checks.push_back(std::move(stored_entry));
    }
    catch (const std::bad_alloc &)
    {
        g_su_health_checks_mutex.unlock();
        return (-1);
    }
    g_su_health_checks_mutex.unlock();
    return (0);
}

int32_t su_health_unregister_check(const char *name)
{
    if (name == ft_nullptr)
        return (-1);
    su_health_entry removed;
    g_su_health_checks_mutex.lock();
    ft_size_t index = 0;
    while (index < g_su_health_checks.size())
    {
        if (g_su_health_checks[index]->name == name)
        {
            removed = std::move(g_su_health_checks[index]);
            g_su_health_checks.erase(g_su_health_checks.begin() + index);
            g_su_health_checks_mutex.unlock();
            return (0);
        }
        ++index;
    }
    g_su_health_checks_mutex.unlock();
    return (-1);
}

void su_health_clear_checks(void)
{
    std::vector<su_health_entry> removed;
    g_su_health_checks_mutex.lock();
    removed.swap(g_su_health_checks);
    g_su_health_checks_mutex.unlock();
    return ;
}

int32_t su_health_run_checks(t_su_health_check_result *results,
    ft_size_t capacity, ft_size_t *count)
{
    constexpr ft_size_t INLINE_CHECK_COUNT = 8;
    if (count == ft_nullptr)
        return (-1);

    su_health_entry inline_checks[INLINE_CHECK_COUNT];
    std::vector<su_health_entry> checks;
    const su_health_entry *snapshot = inline_checks;
    g_su_health_checks_mutex.lock();
    const ft_size_t registered_count = g_su_health_checks.size();
    if (results != ft_nullptr && capacity < registered_count)
    {
        g_su_health_checks_mutex.unlock();
        *count = registered_count;
        return (-1);
    }
    if (registered_count <= INLINE_CHECK_COUNT)
    {
        ft_size_t index = 0;
        while (index < registered_count)
        {
            inline_checks[index] = g_su_health_checks[index];
            ++index;
        }
    }
    else
    {
        try
        {
            checks = g_su_health_checks;
        }
        catch (const std::bad_alloc &)
        {
            g_su_health_checks_mutex.unlock();
            return (-1);
        }
        snapshot = checks.data();
    }
    g_su_health_checks_mutex.unlock();

    *count = registered_count;
    ft_size_t index = 0;
    while (index < registered_count)
    {
        t_su_health_check_result *result_slot = ft_nullptr;
        if (results != ft_nullptr)
            result_slot = &results[index];
        if (su_health_execute_entry(*snapshot[index], result_slot) != FT_ERR_SUCCESS)
            return (-1);
        ++index;
    }
    return (0);
}

int32_t su_health_run_check(const char *name, t_su_health_check_result *result)
{
    if (name == ft_nullptr)
        return (-1);
    su_health_entry found;
    g_su_health_checks_mutex.lock();
    ft_size_t index = 0;
    while (index < g_su_health_checks.size())
    {
        if (g_su_health_checks[index]->name == name)
        {
            found = g_su_health_checks[index];
            break ;
        }
        ++index;
    }
    g_su_health_checks_mutex.unlock();
    if (!found || su_health_execute_entry(*found, result) != FT_ERR_SUCCESS)
        return (-1);
    return (0);
}
