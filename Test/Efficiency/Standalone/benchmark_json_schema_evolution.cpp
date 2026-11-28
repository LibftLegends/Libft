#include "../../../Modules/JSon/document.hpp"
#include "../../../Modules/JSon/json_schema_evolution.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_migrations(const char *name, int32_t version_count)
{
    constexpr int32_t LOOKUP_COUNT = 100000;
    constexpr int32_t APPLY_COUNT = 2000;
    constexpr int32_t ROUTE_LENGTH = 16;
    ft_string schema_name;
    json_document document;
    if (schema_name.initialize(name) != FT_ERR_SUCCESS || document.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t visits = 0;
    const ft_function<int32_t(json_document &)> callback([&](json_document &) -> int32_t
    {
        ++visits;
        return (FT_ERR_SUCCESS);
    });
    int32_t version = version_count;
    while (version > 0)
    {
        --version;
        if (json_register_schema_migration(schema_name, version, version + 1, callback) != FT_ERR_SUCCESS)
            return (1);
    }
    int32_t lookup = 0;
    const auto lookup_start = std::chrono::steady_clock::now();
    while (lookup < LOOKUP_COUNT)
    {
        int32_t latest = 0;
        if (json_get_latest_schema_version(schema_name, &latest) != FT_ERR_SUCCESS || latest != version_count)
            return (1);
        ++lookup;
    }
    const auto lookup_elapsed = std::chrono::steady_clock::now() - lookup_start;
    int32_t application = 0;
    const auto apply_start = std::chrono::steady_clock::now();
    while (application < APPLY_COUNT)
    {
        const int32_t from_version = application % (version_count - ROUTE_LENGTH + 1);
        if (json_apply_schema_migrations(document, schema_name, from_version, from_version + ROUTE_LENGTH) != FT_ERR_SUCCESS)
            return (1);
        ++application;
    }
    const auto apply_elapsed = std::chrono::steady_clock::now() - apply_start;
    if (visits != APPLY_COUNT * ROUTE_LENGTH)
        return (1);
    std::printf("schema_migration versions=%d lookup_ns/op=%.1f apply_route_%d_us/op=%.3f\n",
        version_count, std::chrono::duration<double, std::nano>(lookup_elapsed).count() / LOOKUP_COUNT,
        ROUTE_LENGTH, std::chrono::duration<double, std::micro>(apply_elapsed).count() / APPLY_COUNT);
    return (0);
}

int main()
{
    if (measure_migrations("bench.schema.small", 32) != 0)
        return (1);
    return (measure_migrations("bench.schema.large", 1024));
}
