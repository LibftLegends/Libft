#include "../../Modules/API/api_http_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_api_retry_delay_limits_and_overflow)
{
    FT_ASSERT_EQ(0, api_retry_prepare_delay(-5, 100));
    FT_ASSERT_EQ(40, api_retry_prepare_delay(40, 0));
    FT_ASSERT_EQ(20, api_retry_prepare_delay(40, 20));
    FT_ASSERT_EQ(-5, api_retry_next_delay(-5, 100, 3));
    FT_ASSERT_EQ(60, api_retry_next_delay(20, 0, 3));
    FT_ASSERT_EQ(20, api_retry_next_delay(20, 0, 0));
    FT_ASSERT_EQ(50, api_retry_next_delay(INT32_MAX, 50, INT32_MAX));
    FT_ASSERT_EQ(INT32_MAX, api_retry_next_delay(INT32_MAX, 0, INT32_MAX));
    return (1);
}

FT_TEST(test_api_retry_policy_copy_and_move_release_owned_mutexes)
{
    api_retry_policy source;
    api_retry_policy copy;
    api_retry_policy destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    source.set_max_attempts(4);
    source.set_initial_delay_ms(10);
    source.set_max_delay_ms(100);
    source.set_backoff_multiplier(3);
    source.set_circuit_breaker_threshold(6);
    source.set_circuit_breaker_cooldown_ms(1000);
    source.set_circuit_breaker_half_open_successes(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, copy.is_thread_safe());
    FT_ASSERT_EQ(4, copy.get_max_attempts());
    FT_ASSERT_EQ(2, copy.get_circuit_breaker_half_open_successes());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    FT_ASSERT_EQ(0U, destination.move(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT_EQ(4, destination.get_max_attempts());
    FT_ASSERT_EQ(10, destination.get_initial_delay_ms());
    FT_ASSERT_EQ(100, destination.get_max_delay_ms());
    FT_ASSERT_EQ(3, destination.get_backoff_multiplier());
    FT_ASSERT_EQ(6, destination.get_circuit_breaker_threshold());
    FT_ASSERT_EQ(1000, destination.get_circuit_breaker_cooldown_ms());
    FT_ASSERT_EQ(2, destination.get_circuit_breaker_half_open_successes());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0, source.get_max_attempts());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    source.set_initial_delay_ms(11);
    source.reset();
    FT_ASSERT_EQ(0, source.get_initial_delay_ms());
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());
    return (1);
}

FT_TEST(test_api_circuit_healthy_requests_do_not_allocate_state)
{
    api_retry_circuit_reset();
    api_connection_pool_handle handle;
    api_retry_policy policy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.key.assign("healthy", 7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, policy.initialize());
    policy.set_circuit_breaker_threshold(2);
    ft_size_t allocations_before = 0;
    ft_size_t allocations_after = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&allocations_before, ft_nullptr));
    int32_t error_code = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_TRUE, api_retry_circuit_allow(handle, &policy, error_code));
    api_retry_circuit_record_success(handle, &policy);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&allocations_after, ft_nullptr));
    FT_ASSERT_EQ(allocations_before, allocations_after);
    return (1);
}

FT_TEST(test_api_circuit_half_open_allows_each_required_success)
{
    api_retry_circuit_reset();
    api_connection_pool_handle handle;
    api_retry_policy policy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.key.assign("probe", 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, policy.initialize());
    policy.set_circuit_breaker_threshold(1);
    policy.set_circuit_breaker_half_open_successes(2);
    api_retry_circuit_record_failure(handle, &policy);
    int32_t error_code = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_TRUE, api_retry_circuit_allow(handle, &policy, error_code));
    FT_ASSERT_EQ(FT_FALSE, api_retry_circuit_allow(handle, &policy, error_code));
    FT_ASSERT_EQ(FT_ERR_API_CIRCUIT_OPEN, error_code);
    api_retry_circuit_record_success(handle, &policy);
    FT_ASSERT_EQ(FT_TRUE, api_retry_circuit_allow(handle, &policy, error_code));
    FT_ASSERT_EQ(FT_FALSE, api_retry_circuit_allow(handle, &policy, error_code));
    api_retry_circuit_record_success(handle, &policy);
    FT_ASSERT_EQ(FT_TRUE, api_retry_circuit_allow(handle, &policy, error_code));
    FT_ASSERT_EQ(FT_TRUE, api_retry_circuit_allow(handle, &policy, error_code));
    return (1);
}

FT_TEST(test_api_circuit_keeps_keys_independent_after_growth_and_recovery)
{
    api_retry_circuit_reset();
    api_connection_pool_handle handle;
    api_retry_policy policy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, policy.initialize());
    policy.set_circuit_breaker_threshold(1);
    policy.set_circuit_breaker_cooldown_ms(60000);
    uint32_t index = 0;
    while (index < 128U)
    {
        const char key[] = {'k', static_cast<char>(index)};
        FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.key.assign(key, sizeof(key)));
        api_retry_circuit_record_failure(handle, &policy);
        ++index;
    }
    int32_t error_code = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.key.assign("k\0", 2U));
    FT_ASSERT_EQ(FT_FALSE, api_retry_circuit_allow(handle, &policy, error_code));
    FT_ASSERT_EQ(FT_ERR_API_CIRCUIT_OPEN, error_code);
    api_retry_circuit_record_success(handle, &policy);
    FT_ASSERT_EQ(FT_TRUE, api_retry_circuit_allow(handle, &policy, error_code));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.key.assign("k\x7f", 2U));
    FT_ASSERT_EQ(FT_FALSE, api_retry_circuit_allow(handle, &policy, error_code));
    api_retry_circuit_reset();
    FT_ASSERT_EQ(FT_TRUE, api_retry_circuit_allow(handle, &policy, error_code));
    return (1);
}

FT_TEST(test_api_circuit_key_allocation_can_be_retried)
{
    api_retry_circuit_reset();
    api_connection_pool_handle handle;
    api_retry_policy policy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, policy.initialize());
    policy.set_circuit_breaker_threshold(1);
    policy.set_circuit_breaker_cooldown_ms(60000);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.key.assign("first", 5U));
    api_retry_circuit_record_failure(handle, &policy);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.key.assign("second", 6U));
    cma_set_alloc_limit(1U);
    api_retry_circuit_record_failure(handle, &policy);
    cma_set_alloc_limit(0U);
    int32_t error_code = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_TRUE, api_retry_circuit_allow(handle, &policy, error_code));
    api_retry_circuit_record_failure(handle, &policy);
    FT_ASSERT_EQ(FT_FALSE, api_retry_circuit_allow(handle, &policy, error_code));
    return (1);
}
