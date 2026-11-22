#include "../test_internal.hpp"
#include "../../Modules/JSon/document.hpp"
#include "../../Modules/JSon/json_schema_evolution.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_json_schema_migrations_index_versions_and_validate_the_route)
{
    ft_string name;
    ft_string missing;
    json_document document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name.initialize("test.schema.routes"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, missing.initialize("test.schema.unknown"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    int32_t latest = -1;
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, json_get_latest_schema_version(missing, &latest));
    FT_ASSERT_EQ(-1, latest);
    int32_t visits = 0;
    const ft_function<int32_t(json_document &)> first([&](json_document &) -> int32_t
    {
        int32_t callback_latest = 0;
        const int32_t error = json_get_latest_schema_version(name, &callback_latest);
        if (error != FT_ERR_SUCCESS || callback_latest != 8)
            return (FT_ERR_INTERNAL);
        visits = visits * 10 + 1;
        return (FT_ERR_SUCCESS);
    });
    const ft_function<int32_t(json_document &)> second([&](json_document &) -> int32_t
    {
        visits = visits * 10 + 2;
        return (FT_ERR_SUCCESS);
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_register_schema_migration(name, 6, 8, second));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_register_schema_migration(name, 0, 2, first));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, json_register_schema_migration(name, 0, 1, first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_get_latest_schema_version(name, &latest));
    FT_ASSERT_EQ(8, latest);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, json_apply_schema_migrations(document, name, 0, 1));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, json_apply_schema_migrations(document, name, 0, 8));
    FT_ASSERT_EQ(0, visits);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_register_schema_migration(name, 2, 6, second));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_apply_schema_migrations(document, name, 0, 8));
    FT_ASSERT_EQ(122, visits);
    return (1);
}

FT_TEST(test_json_schema_registry_retries_after_allocation_failure)
{
    ft_string name;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name.initialize("test.schema.allocation"));
    const ft_function<int32_t(json_document &)> callback([](json_document &) -> int32_t
    {
        return (FT_ERR_SUCCESS);
    });
    cma_set_alloc_limit(1U);
    const int32_t allocation_error = json_register_schema_migration(name, 0, 1, callback);
    cma_set_alloc_limit(0U);
    int32_t latest = -1;
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, allocation_error);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, json_get_latest_schema_version(name, &latest));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_register_schema_migration(name, 0, 1, callback));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_get_latest_schema_version(name, &latest));
    FT_ASSERT_EQ(1, latest);
    return (1);
}

FT_TEST(test_json_schema_migrations_keep_order_after_registry_growth)
{
    constexpr int32_t VERSION_COUNT = 96;
    ft_string name;
    json_document document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name.initialize("test.schema.growth"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    int32_t visits = 0;
    int32_t version = VERSION_COUNT;
    while (version > 0)
    {
        --version;
        const ft_function<int32_t(json_document &)> callback([&, version](json_document &) -> int32_t
        {
            if (visits != version)
                return (FT_ERR_INTERNAL);
            ++visits;
            return (FT_ERR_SUCCESS);
        });
        FT_ASSERT_EQ(FT_ERR_SUCCESS, json_register_schema_migration(name, version, version + 1, callback));
    }
    int32_t latest = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_get_latest_schema_version(name, &latest));
    FT_ASSERT_EQ(VERSION_COUNT, latest);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_apply_schema_migrations(document, name, 0, VERSION_COUNT));
    FT_ASSERT_EQ(VERSION_COUNT, visits);
    return (1);
}
