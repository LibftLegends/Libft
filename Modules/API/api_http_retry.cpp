#include "api_http_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno.hpp"

int32_t api_retry_get_max_attempts(const api_retry_policy *retry_policy)
{
    int32_t attempts;

    if (!retry_policy)
        return (1);
    attempts = retry_policy->get_max_attempts();
    if (attempts <= 0)
        return (1);
    return (attempts);
}

int32_t api_retry_get_initial_delay(const api_retry_policy *retry_policy)
{
    int32_t initial_delay;

    if (!retry_policy)
        return (0);
    initial_delay = retry_policy->get_initial_delay_ms();
    if (initial_delay <= 0)
        return (0);
    return (initial_delay);
}

int32_t api_retry_get_max_delay(const api_retry_policy *retry_policy)
{
    int32_t max_delay;

    if (!retry_policy)
        return (0);
    max_delay = retry_policy->get_max_delay_ms();
    if (max_delay <= 0)
        return (0);
    return (max_delay);
}

int32_t api_retry_get_multiplier(const api_retry_policy *retry_policy)
{
    int32_t multiplier;

    if (!retry_policy)
        return (2);
    multiplier = retry_policy->get_backoff_multiplier();
    if (multiplier <= 0)
        return (2);
    return (multiplier);
}

int32_t api_retry_get_circuit_threshold(const api_retry_policy *retry_policy)
{
    int32_t threshold;

    if (!retry_policy)
        return (0);
    threshold = retry_policy->get_circuit_breaker_threshold();
    if (threshold <= 0)
        return (0);
    return (threshold);
}

int32_t api_retry_get_circuit_cooldown(const api_retry_policy *retry_policy)
{
    int32_t cooldown;

    if (!retry_policy)
        return (0);
    cooldown = retry_policy->get_circuit_breaker_cooldown_ms();
    if (cooldown <= 0)
        return (0);
    return (cooldown);
}

int32_t api_retry_get_half_open_successes(const api_retry_policy *retry_policy)
{
    int32_t half_open_successes;

    if (!retry_policy)
        return (1);
    half_open_successes = retry_policy->get_circuit_breaker_half_open_successes();
    if (half_open_successes <= 0)
        return (1);
    return (half_open_successes);
}

int32_t api_retry_prepare_delay(int32_t delay, int32_t max_delay)
{
    if (delay <= 0)
        return (0);
    if (max_delay > 0 && delay > max_delay)
        return (max_delay);
    return (delay);
}

int32_t api_retry_next_delay(int32_t current_delay, int32_t max_delay,
    int32_t multiplier)
{
    int64_t next_delay;

    if (current_delay <= 0)
        return (current_delay);
    next_delay = static_cast<int64_t>(current_delay);
    if (multiplier > 1)
        next_delay *= static_cast<int64_t>(multiplier);
    if (max_delay > 0 && next_delay > max_delay)
        next_delay = max_delay;
    if (next_delay > INT32_MAX)
        next_delay = INT32_MAX;
    return (static_cast<int32_t>(next_delay));
}
