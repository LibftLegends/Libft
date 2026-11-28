#include "../../../Modules/API/api.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static char *count_request(const char *, uint16_t, const char *,
    const char *, json_group *, const char *headers, int32_t *, int32_t,
    const api_retry_policy *, void *context)
{
    if (headers && headers[0] == 'X')
        ++*static_cast<uint32_t *>(context);
    return (ft_nullptr);
}

int main()
{
    constexpr uint32_t ITERATIONS = 100000;
    const std::string headers = "X-Test: " + std::string(2048, 'x');
    const std::string credentials(256, 'a');
    uint32_t completed = 0;
    api_transport_hooks hooks{};
    hooks.request_string = count_request;
    hooks.user_data = &completed;
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    api_set_transport_hooks(&hooks);
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        api_request_string_bearer("127.0.0.1", 80, "GET", "/", credentials.c_str(),
            ft_nullptr, headers.c_str(), ft_nullptr, 500, ft_nullptr);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    api_clear_transport_hooks();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS
        || completed != ITERATIONS)
        return (1);
    std::printf("%" FT_PRIu64 " CMA allocations in %u requests, %.3f us/request\n",
        allocations_after - allocations_before, ITERATIONS, elapsed / ITERATIONS);
    return (0);
}
