#include "../test_internal.hpp"
#include "../../Modules/API/api.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

struct api_auth_header_expectation
{
    const char *headers;
    ft_bool matched;
    uint32_t calls;
};

static char *test_api_capture_auth_header(const char *, uint16_t, const char *,
    const char *, json_group *, const char *headers, int32_t *, int32_t,
    const api_retry_policy *, void *context)
{
    auto &expectation = *static_cast<api_auth_header_expectation *>(context);
    expectation.calls++;
    if (headers && expectation.headers)
        expectation.matched = (ft_strcmp(headers, expectation.headers) == 0);
    else
        expectation.matched = (headers == expectation.headers);
    return (ft_nullptr);
}

FT_TEST(test_api_request_authorization_headers_across_transports)
{
    struct authorization_case
    {
        decltype(&api_request_string_bearer) request;
        const char *bare_header;
        const char *combined_headers;
    };

    const authorization_case cases[] = {
        {api_request_string_bearer, "Authorization: Bearer sample",
            "X-Test: 1\r\nAuthorization: Bearer sample"},
        {api_request_string_basic, "Authorization: Basic sample",
            "X-Test: 1\r\nAuthorization: Basic sample"},
        {api_request_string_host_bearer, "Authorization: Bearer sample",
            "X-Test: 1\r\nAuthorization: Bearer sample"},
        {api_request_string_host_basic, "Authorization: Basic sample",
            "X-Test: 1\r\nAuthorization: Basic sample"},
#if NETWORKING_HAS_OPENSSL
        {api_request_string_tls_bearer, "Authorization: Bearer sample",
            "X-Test: 1\r\nAuthorization: Bearer sample"},
        {api_request_string_tls_basic, "Authorization: Basic sample",
            "X-Test: 1\r\nAuthorization: Basic sample"},
#endif
    };
    const char *const inputs[] = {ft_nullptr, "", "X-Test: 1"};
    ft_size_t case_index = 0;
    while (case_index < sizeof(cases) / sizeof(cases[0]))
    {
        const authorization_case &test_case = cases[case_index];
        ft_size_t input_index = 0;
        while (input_index < sizeof(inputs) / sizeof(inputs[0]))
        {
            api_auth_header_expectation expectation{test_case.bare_header, FT_FALSE, 0};
            if (input_index == 2)
                expectation.headers = test_case.combined_headers;
            api_transport_hooks hooks{};
            hooks.request_string = test_api_capture_auth_header;
            hooks.request_string_host = test_api_capture_auth_header;
            hooks.request_string_tls = test_api_capture_auth_header;
            hooks.user_data = &expectation;
            api_set_transport_hooks(&hooks);
            test_case.request("127.0.0.1", 443, "GET", "/", "sample", ft_nullptr,
                inputs[input_index], ft_nullptr, 500, ft_nullptr);
            api_clear_transport_hooks();
            FT_ASSERT_EQ(1, expectation.calls);
            FT_ASSERT(expectation.matched);
            ++input_index;
        }
        ++case_index;
    }
    return (1);
}

FT_TEST(test_api_request_authorization_headers_passthrough_and_allocation_failure)
{
    api_auth_header_expectation expectation{"X-Test: 1", FT_FALSE, 0};
    api_transport_hooks hooks{};
    hooks.request_string = test_api_capture_auth_header;
    hooks.user_data = &expectation;
    api_set_transport_hooks(&hooks);
    api_request_string_bearer("127.0.0.1", 80, "GET", "/", ft_nullptr, ft_nullptr,
        expectation.headers, ft_nullptr, 500, ft_nullptr);
    api_clear_transport_hooks();
    FT_ASSERT_EQ(1, expectation.calls);
    FT_ASSERT(expectation.matched);

    expectation.calls = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1));
    api_set_transport_hooks(&hooks);
    const char *response = api_request_string_basic("127.0.0.1", 80, "GET", "/",
        "sample", ft_nullptr, ft_nullptr, ft_nullptr, 500, ft_nullptr);
    api_clear_transport_hooks();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0));
    FT_ASSERT(response == ft_nullptr);
    FT_ASSERT_EQ(0, expectation.calls);
    return (1);
}
