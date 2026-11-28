#include "system_utils.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno.hpp"
#include "../Template/move.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <string_view>
#include <vector>
#include "../Basic/limits.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Template/vector.hpp"

static pthread_mutex_t g_env_mutex = PTHREAD_MUTEX_INITIALIZER;

static int32_t su_environment_lock_mutex(void)
{
    int32_t pthread_error;

    pthread_error = pthread_mutex_lock(&g_env_mutex);
    if (pthread_error != 0)
        return (cmp_map_system_error_to_ft(pthread_error));
    return (FT_ERR_SUCCESS);
}

static int32_t su_environment_unlock_mutex(void)
{
    int32_t pthread_error;

    pthread_error = pthread_mutex_unlock(&g_env_mutex);
    if (pthread_error != 0)
        return (cmp_map_system_error_to_ft(pthread_error));
    return (FT_ERR_SUCCESS);
}

int32_t su_environment_enable_thread_safety(void)
{
    return (FT_ERR_SUCCESS);
}

void su_environment_disable_thread_safety(void)
{
    return ;
}

static char **su_environment_entries(void)
{
    return (cmp_get_environ_entries());
}

char *su_getenv(const char *name)
{
    int32_t     lock_error;
    int32_t     unlock_error;
    char        *result;

    if (name == ft_nullptr || *name == '\0')
        return (ft_nullptr);
    lock_error = su_environment_lock_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    result = getenv(name);
    unlock_error = su_environment_unlock_mutex();
    if (unlock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    return (result);
}

int32_t su_setenv(const char *name, const char *value, int32_t overwrite)
{
    int32_t lock_error;
    int32_t unlock_error;
    int32_t result;

    if (name == ft_nullptr || value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (*name == '\0' || ft_strchr(name, '=') != ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    lock_error = su_environment_lock_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    result = cmp_setenv(name, value, overwrite);
    unlock_error = su_environment_unlock_mutex();
    if (unlock_error != FT_ERR_SUCCESS)
        return (-1);
    return (result);
}

int32_t su_unsetenv(const char *name)
{
    int32_t lock_error;
    int32_t unlock_error;
    int32_t result;

    if (name == ft_nullptr || *name == '\0' || ft_strchr(name, '=') != ft_nullptr)
        return (-1);
    lock_error = su_environment_lock_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    result = cmp_unsetenv(name);
    unlock_error = su_environment_unlock_mutex();
    if (unlock_error != FT_ERR_SUCCESS)
        return (-1);
    if (result == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    if (result > FT_ERR_SUCCESS)
        return (result);
#if defined(_WIN32) || defined(_WIN64)
    if (GetLastError() != 0 || WSAGetLastError() != 0)
        return (-1);
#endif
    if (errno == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    return (-1);
}

int32_t su_putenv(char *string)
{
    int32_t result;
    int32_t lock_error;
    int32_t unlock_error;

    if (string == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_error = su_environment_lock_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    result = cmp_putenv(string);
    unlock_error = su_environment_unlock_mutex();
    if (unlock_error != FT_ERR_SUCCESS)
        return (-1);
    return (result);
}

static ft_bool su_environment_snapshot_matches(const ft_vector<ft_string> &snapshot,
    char *const *entries, ft_size_t count)
{
    if (snapshot.is_initialised() != FT_CLASS_STATE_INITIALISED || snapshot.size() != count)
        return (FT_FALSE);
    ft_size_t index = 0U;
    while (index < count)
    {
        const ft_string &saved = snapshot[index];
        const ft_size_t length = ft_strlen_size_t(entries[index]);
        if (saved.size() != length || ft_memcmp(saved.c_str(), entries[index], length) != 0)
            return (FT_FALSE);
        ++index;
    }
    return (FT_TRUE);
}

static int32_t su_environment_capture_entries(ft_vector<ft_string> &snapshot,
    char *const *entries)
{
    ft_size_t count = 0U;
    while (entries != ft_nullptr && entries[count] != ft_nullptr)
        ++count;
    if (su_environment_snapshot_matches(snapshot, entries, count) == FT_TRUE)
        return (FT_ERR_SUCCESS);

    ft_vector<ft_string> replacement;
    int32_t status = replacement.initialize();
    if (status != FT_ERR_SUCCESS)
        return (status);
    if (count != 0U)
    {
        // Heap storage transfers to the snapshot without re-copying strings.
        replacement.reserve(std::max(count, replacement.capacity() + 1U));
        status = replacement.get_error();
        if (status != FT_ERR_SUCCESS)
            return (status);
    }
    ft_size_t index = 0U;
    while (index < count)
    {
        ft_string entry_copy;
        status = entry_copy.initialize(entries[index]);
        if (status != FT_ERR_SUCCESS)
            return (status);
        status = replacement.push_back(ft_move(entry_copy));
        if (status != FT_ERR_SUCCESS)
            return (status);
        ++index;
    }
    return (snapshot.move(replacement));
}

int32_t su_environment_snapshot_capture(t_su_environment_snapshot *snapshot)
{
    if (snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const int32_t lock_error = su_environment_lock_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t status = su_environment_capture_entries(snapshot->entries,
        su_environment_entries());
    const int32_t unlock_error = su_environment_unlock_mutex();
    if (status != FT_ERR_SUCCESS)
        return (status);
    return (unlock_error);
}

static ft_bool su_environment_name_less(std::string_view left, std::string_view right)
{
#if defined(_WIN32) || defined(_WIN64)
    if (left.size() != right.size())
        return (left.size() < right.size());
    return (_strnicoll(left.data(), right.data(), left.size()) < 0);
#else
    return (left < right);
#endif
}

struct s_environment_saved_value
{
    const char *value;
    ft_bool matches;
};

struct s_environment_change
{
    std::string name;
    const char *value;
};

static int32_t su_environment_apply_change(const s_environment_change &change)
{
    if (change.value != ft_nullptr)
        return (cmp_setenv(change.name.c_str(), change.value, 1));
    const int32_t result = cmp_unsetenv(change.name.c_str());
    if (result != -1)
        return (result);
#if defined(_WIN32) || defined(_WIN64)
    const DWORD native_error = GetLastError();
    if (native_error != 0)
        return (cmp_map_system_error_to_ft(static_cast<int32_t>(native_error)));
#endif
    if (errno != 0)
        return (cmp_map_system_error_to_ft(errno));
    return (FT_ERR_IO);
}

static int32_t su_environment_restore_entries(const ft_vector<ft_string> &snapshot,
    char *const *current)
{
    ft_size_t current_count = 0U;
    while (current != ft_nullptr && current[current_count] != ft_nullptr)
        ++current_count;
    if (su_environment_snapshot_matches(snapshot, current, current_count) == FT_TRUE)
        return (FT_ERR_SUCCESS);

    std::map<std::string_view, s_environment_saved_value,
        decltype(&su_environment_name_less)> saved(su_environment_name_less);
    const ft_size_t snapshot_count = snapshot.size();
    ft_size_t index = 0U;
    while (index < snapshot_count)
    {
        const ft_string &entry = snapshot[index];
        const std::string_view text(entry.c_str(), FT_UINT64_TO_NATIVE_SIZE_CAST(entry.size()));
        ++index;
#if defined(_WIN32) || defined(_WIN64)
        // Drive working-directory records are managed by Windows.
        if (!text.empty() && text.front() == '=')
            continue ;
#endif
        const auto separator = text.find('=');
        if (text.empty() || separator == 0 || text.find('\0') != std::string_view::npos)
            return (FT_ERR_INVALID_ARGUMENT);
        const char *value = ft_nullptr;
        if (separator != std::string_view::npos)
            value = text.data() + separator + 1U;
#if defined(_WIN32) || defined(_WIN64)
        if (value != ft_nullptr && *value == '\0')
            value = ft_nullptr;
#endif
        saved.insert_or_assign(text.substr(0, separator), s_environment_saved_value{value, FT_FALSE});
    }

    std::vector<s_environment_change> changes;
    index = 0U;
    while (index < current_count)
    {
        const std::string_view text(current[index]);
        ++index;
        const auto separator = text.find('=');
        if (text.empty() || separator == 0)
            continue ;
        const std::string_view name = text.substr(0, separator);
        auto entry = saved.find(name);
        if (entry == saved.end() || entry->second.value == ft_nullptr)
            changes.push_back({std::string(name), ft_nullptr});
        else if (separator != std::string_view::npos)
        {
            entry->second.matches = text.substr(separator + 1U) == entry->second.value;
        }
    }
    auto entry = saved.begin();
    while (entry != saved.end())
    {
        if (entry->second.value != ft_nullptr && entry->second.matches == FT_FALSE)
            changes.push_back({std::string(entry->first), entry->second.value});
        ++entry;
    }

    auto change = changes.begin();
    while (change != changes.end())
    {
        const int32_t result = su_environment_apply_change(*change);
        if (result != FT_ERR_SUCCESS)
            return (result);
        ++change;
    }
    return (FT_ERR_SUCCESS);
}

int32_t su_environment_snapshot_restore(const t_su_environment_snapshot *snapshot)
{
    if (snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot->entries.is_initialised() != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    const int32_t lock_error = su_environment_lock_mutex();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    int32_t status;
    try
    {
        status = su_environment_restore_entries(snapshot->entries, su_environment_entries());
    }
    catch (const std::bad_alloc &)
    {
        status = FT_ERR_NO_MEMORY;
    }
    catch (...)
    {
        status = FT_ERR_INTERNAL;
    }
    const int32_t unlock_error = su_environment_unlock_mutex();
    if (status != FT_ERR_SUCCESS)
        return (status);
    return (unlock_error);
}

void su_environment_snapshot_dispose(t_su_environment_snapshot *snapshot)
{
    if (snapshot == ft_nullptr)
        return ;
    if (snapshot->entries.is_initialised() == FT_CLASS_STATE_INITIALISED)
        snapshot->entries.clear();
    return ;
}

int32_t su_environment_sandbox_begin(t_su_environment_snapshot *snapshot)
{
    return (su_environment_snapshot_capture(snapshot));
}

int32_t su_environment_sandbox_end(t_su_environment_snapshot *snapshot)
{
    int32_t restore_error;

    restore_error = su_environment_snapshot_restore(snapshot);
    if (restore_error == FT_ERR_SUCCESS)
        su_environment_snapshot_dispose(snapshot);
    return (restore_error);
}
