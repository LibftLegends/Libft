#include "api_http_internal.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Template/unordered_map.hpp"
#include "../PThread/mutex.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include "../Time/time.hpp"
#include "../Errno/errno.hpp"
#include "../System_utils/system_utils.hpp"
#include "api_internal.hpp"
#include <new>
#include <string_view>
#include <cstdio>
#include "../Basic/limits.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"

struct api_circuit_state
{
    int32_t failure_count = 0;
    int64_t open_until_ms = 0;
    ft_bool half_open = FT_FALSE;
    ft_bool probe_in_flight = FT_FALSE;
    int32_t half_open_success_count = 0;
};

using api_circuit_table = ft_unordered_map<std::string_view, api_circuit_state>;

struct api_circuit_registry
{
    api_circuit_table states;
    ~api_circuit_registry();
    void clear();
};

void api_circuit_registry::clear()
{
    if (this->states.is_initialised() != FT_CLASS_STATE_INITIALISED)
        return ;
    auto entry = this->states.begin();
    const auto end = this->states.end();
    while (entry != end)
    {
        cma_free(const_cast<char *>(entry->first.data()));
        ++entry;
    }
    this->states.clear();
    return ;
}

api_circuit_registry::~api_circuit_registry()
{
    this->clear();
    return ;
}

#ifdef DEBUG
static void api_retry_circuit_debug_log(const char *message)
{
    std::FILE *file_pointer;

    file_pointer = std::fopen("api_retry_debug.log", "a");
    if (!file_pointer)
        return ;
    std::fprintf(file_pointer, "%s\n", message);
    std::fclose(file_pointer);
    return ;
}
#else
#define api_retry_circuit_debug_log(message) do { } while (0)
#endif

static api_circuit_registry &api_retry_circuit_get_registry()
{
    static api_circuit_registry registry;
    return (registry);
}

static pt_mutex &api_retry_circuit_get_mutex()
{
    static pt_mutex *const circuit_mutex = []() noexcept
    {
        pt_mutex *const mutex = new (std::nothrow) pt_mutex();
        if (mutex == ft_nullptr || mutex->initialize() != FT_ERR_SUCCESS)
            su_abort();
#ifdef LIBFT_TEST_BUILD
        (void)cma_untrack_leak(mutex);
        if (mutex->_native_mutex != ft_nullptr)
            (void)cma_untrack_leak(mutex->_native_mutex);
#endif
        return (mutex);
    }();
    return (*circuit_mutex);
}

static int64_t api_retry_circuit_compute_deadline(int64_t now_ms, int32_t cooldown_ms)
{
    if (now_ms > INT64_MAX - cooldown_ms)
        return (INT64_MAX);
    return (now_ms + cooldown_ms);
}

static void api_retry_circuit_insert(api_circuit_table &states, std::string_view key,
    const api_circuit_state &state)
{
    char *const key_copy = static_cast<char *>(cma_malloc(key.size()));
    if (!key_copy)
        return ;
    ft_memcpy(key_copy, key.data(), key.size());
    states.insert(std::string_view(key_copy, key.size()), state);
    if (states.get_error() != FT_ERR_SUCCESS)
        cma_free(key_copy);
    return ;
}

ft_bool api_retry_circuit_allow(const api_connection_pool_handle &handle,
    const api_retry_policy *retry_policy, int32_t &error_code)
{
    if (api_retry_get_circuit_threshold(retry_policy) <= 0 || handle.key.empty())
        return (FT_TRUE);
    const std::string_view key(handle.key.c_str(), handle.key.size());
    pt_mutex &mutex = api_retry_circuit_get_mutex();
    if (mutex.lock() != FT_ERR_SUCCESS)
        return (FT_TRUE);
    api_circuit_table &states = api_retry_circuit_get_registry().states;
    ft_bool permit = FT_TRUE;
    if (states.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        const auto entry = states.find(key);
        if (entry != states.end())
        {
            api_circuit_state &state = entry->second;
            if (state.open_until_ms != 0)
            {
                if (state.open_until_ms > time_monotonic())
                {
                    permit = FT_FALSE;
                    api_retry_circuit_debug_log("allow: open");
                }
                else
                {
                    state = api_circuit_state{};
                    state.half_open = FT_TRUE;
                    api_retry_circuit_debug_log("allow: half_open");
                }
            }
            if (permit && state.half_open)
            {
                permit = !state.probe_in_flight;
                state.probe_in_flight = FT_TRUE;
            }
        }
    }
    if (!permit)
        error_code = FT_ERR_API_CIRCUIT_OPEN;
    (void)mutex.unlock();
    return (permit);
}

void api_retry_circuit_record_success(const api_connection_pool_handle &handle,
    const api_retry_policy *retry_policy)
{
    if (api_retry_get_circuit_threshold(retry_policy) <= 0 || handle.key.empty())
        return ;
    const std::string_view key(handle.key.c_str(), handle.key.size());
    pt_mutex &mutex = api_retry_circuit_get_mutex();
    if (mutex.lock() != FT_ERR_SUCCESS)
        return ;
    api_circuit_table &states = api_retry_circuit_get_registry().states;
    if (states.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        const auto entry = states.find(key);
        if (entry != states.end())
        {
            api_circuit_state &state = entry->second;
            if (state.half_open)
            {
                state.probe_in_flight = FT_FALSE;
                ++state.half_open_success_count;
            }
            if (!state.half_open || state.half_open_success_count
                >= api_retry_get_half_open_successes(retry_policy))
            {
                const char *const stored_key = entry->first.data();
                states.erase(key);
                if (states.get_error() == FT_ERR_SUCCESS)
                    cma_free(const_cast<char *>(stored_key));
            }
        }
    }
    (void)mutex.unlock();
    return ;
}

static void api_retry_circuit_apply_failure(api_circuit_state &state, int32_t threshold,
    const api_retry_policy *retry_policy)
{
    if (state.failure_count < threshold)
        ++state.failure_count;
    if (state.half_open || state.failure_count >= threshold)
    {
        state = api_circuit_state{};
        state.failure_count = threshold;
        state.open_until_ms = api_retry_circuit_compute_deadline(
            time_monotonic(), api_retry_get_circuit_cooldown(retry_policy));
    }
    return ;
}

void api_retry_circuit_record_failure(const api_connection_pool_handle &handle,
    const api_retry_policy *retry_policy)
{
    const int32_t threshold = api_retry_get_circuit_threshold(retry_policy);
    if (threshold <= 0 || handle.key.empty())
        return ;
    const std::string_view key(handle.key.c_str(), handle.key.size());
    pt_mutex &mutex = api_retry_circuit_get_mutex();
    if (mutex.lock() != FT_ERR_SUCCESS)
        return ;
    api_circuit_table &states = api_retry_circuit_get_registry().states;
    if (states.is_initialised() == FT_CLASS_STATE_INITIALISED
        || states.initialize() == FT_ERR_SUCCESS)
    {
        const auto entry = states.find(key);
        if (entry == states.end())
        {
            api_circuit_state state;
            api_retry_circuit_apply_failure(state, threshold, retry_policy);
            api_retry_circuit_insert(states, key, state);
        }
        else
            api_retry_circuit_apply_failure(entry->second, threshold, retry_policy);
    }
    (void)mutex.unlock();
    return ;
}

void api_retry_circuit_reset()
{
    pt_mutex &mutex = api_retry_circuit_get_mutex();
    if (mutex.lock() != FT_ERR_SUCCESS)
        return ;
    api_retry_circuit_get_registry().clear();
    (void)mutex.unlock();
    api_retry_circuit_debug_log("reset");
    api_connection_pool_flush();
    return ;
}

#ifdef LIBFT_TEST_BUILD
void api_retry_circuit_destroy_for_tests()
{
    pt_mutex &mutex = api_retry_circuit_get_mutex();
    if (mutex.lock() != FT_ERR_SUCCESS)
        return ;
    api_circuit_registry &registry = api_retry_circuit_get_registry();
    registry.clear();
    (void)registry.states.destroy();
    (void)mutex.unlock();
    return ;
}
#endif
