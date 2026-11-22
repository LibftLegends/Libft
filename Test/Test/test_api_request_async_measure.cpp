#include "../../Modules/API/api_internal.hpp"
#include "../../Modules/API/api.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Time/time.hpp"
#include <atomic>

struct api_async_measure_test_state
{
    std::atomic<ft_bool> inputs_changed{FT_FALSE};
    std::atomic<ft_bool> complete{FT_FALSE};
    ft_bool inputs_match = FT_FALSE;
    ft_bool response_matches = FT_FALSE;
};

static char *api_async_measure_test_transport(const char *host, uint16_t port,
    const char *method, const char *path, json_group *payload, const char *headers,
    int32_t *status, int32_t timeout, const api_retry_policy *, void *context)
{
    auto &state = *static_cast<api_async_measure_test_state *>(context);
    while (!state.inputs_changed.load(std::memory_order_acquire))
        time_sleep_ms(1);
    state.inputs_match = ft_strcmp(host, "fixture.local") == 0
        && ft_strcmp(method, "POST") == 0 && ft_strcmp(path, "/submit") == 0
        && ft_strcmp(headers, "\r\n\r\nX-Test: yes\r\n\n") == 0
        && port == 1234U && payload != ft_nullptr && timeout == 16065;
    *status = 200;
    return (adv_strdup("ok"));
}

static void api_async_measure_test_complete(char *body, int32_t status, void *context)
{
    auto &state = *static_cast<api_async_measure_test_state *>(context);
    state.response_matches = body && ft_strcmp(body, "ok") == 0 && status == 200;
    cma_free(body);
    state.complete.store(FT_TRUE, std::memory_order_release);
    return ;
}

FT_TEST(test_api_async_measures_large_body_and_keeps_submitted_text)
{
    static api_async_measure_test_state state;
    static char payload_value[65537] = {};
    static char name[] = "payload";
    static char key[] = "value";
    static json_item item{};
    static json_group payload{};
    state.inputs_changed.store(FT_FALSE, std::memory_order_relaxed);
    state.complete.store(FT_FALSE, std::memory_order_relaxed);
    state.inputs_match = FT_FALSE;
    state.response_matches = FT_FALSE;
    ft_memset(payload_value, 'v', sizeof(payload_value) - 1U);
    item.key = key;
    item.value = payload_value;
    payload.name = name;
    payload.items = &item;
    char host[] = "fixture.local";
    char method[] = "POST";
    char path[] = "/submit";
    char headers[] = "\r\n\r\nX-Test: yes\r\n\n";
    api_transport_hooks hooks{};
    hooks.request_string_host = api_async_measure_test_transport;
    hooks.user_data = &state;
    api_set_transport_hooks(&hooks);
    cma_set_alloc_limit(1024U);
    const ft_bool submitted = api_request_string_async(host, 1234U, method, path,
        api_async_measure_test_complete, &state, &payload, headers, 16000);
    host[0] = 'x';
    method[0] = 'x';
    path[0] = 'x';
    headers[0] = 'x';
    state.inputs_changed.store(FT_TRUE, std::memory_order_release);
    uint32_t attempts = 0;
    while (submitted && !state.complete.load(std::memory_order_acquire) && attempts < 1000)
    {
        time_sleep_ms(1);
        ++attempts;
    }
    const ft_bool complete = state.complete.load(std::memory_order_acquire);
    cma_set_alloc_limit(0U);
    api_clear_transport_hooks();
    FT_ASSERT_EQ(FT_TRUE, submitted);
    FT_ASSERT_EQ(FT_TRUE, complete);
    FT_ASSERT_EQ(FT_TRUE, state.inputs_match);
    FT_ASSERT_EQ(FT_TRUE, state.response_matches);
    constexpr ft_size_t EXPECTED = sizeof("POST /submit HTTP/1.1\r\nHost: fixture.local"
        "\r\nX-Test: yes\r\nContent-Type: application/json\r\nContent-Length: 12345"
        "\r\nConnection: keep-alive\r\n\r\n{\n  \"payload\": {\n    \"value\": \"\"\n  }\n}\n")
        + sizeof(payload_value) - 2U;
    FT_ASSERT_EQ(EXPECTED, api_debug_get_last_async_request_size());
    FT_ASSERT_EQ(EXPECTED, api_debug_get_last_async_bytes_sent());
    FT_ASSERT_EQ(2U, api_debug_get_last_async_bytes_received());
    FT_ASSERT_EQ(16000, api_debug_get_last_async_send_timeout());
    FT_ASSERT_EQ(16065, api_debug_get_last_async_receive_timeout());
    return (1);
}
