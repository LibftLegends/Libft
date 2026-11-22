#include "../../../Modules/JSon/document.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>
#include <string>

struct json_serializer_benchmark_fixture
{
    const char *value;
    ft_size_t allocations_before;
    std::chrono::steady_clock::time_point start;
};

static int32_t prepare_document(json_document &document, void *user_data) noexcept
{
    auto *fixture = static_cast<json_serializer_benchmark_fixture *>(user_data);
    json_group *group = document.create_group("sample");
    if (group == ft_nullptr)
        return (document.get_error());
    document.append_group(group);
    json_item *item = document.create_item("value", fixture->value);
    if (item == ft_nullptr)
        return (document.get_error());
    document.add_item(group, item);
    const int32_t error = cma_get_stats(&fixture->allocations_before, ft_nullptr);
    fixture->start = std::chrono::steady_clock::now();
    return (error);
}

int32_t main()
{
    constexpr uint32_t ITERATIONS = 120U;
    const std::string value(1024U * 1024U, 'v');
    json_serializer_benchmark_fixture fixture{value.c_str(), 0U, {}};
    ft_size_t allocations = 0U;
    std::chrono::steady_clock::duration elapsed{};
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        ft_string output;
        const int32_t error = json_serialize_to_string(prepare_document, &fixture, output);
        elapsed += std::chrono::steady_clock::now() - fixture.start;
        if (error != FT_ERR_SUCCESS || output.size() <= value.size())
            return (1);
        ft_size_t allocations_after;
        if (cma_get_stats(&allocations_after, ft_nullptr) != FT_ERR_SUCCESS)
            return (1);
        allocations += allocations_after - fixture.allocations_before;
        ++iteration;
    }
    std::printf("json_serializer allocations=%" FT_PRIu64 " us/op=%.3f\n",
        static_cast<uint64_t>(allocations),
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}
