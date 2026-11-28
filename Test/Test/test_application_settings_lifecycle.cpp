#include "../test_internal.hpp"
#include "../../Modules/Application/application_settings.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_application_settings_move_transfers_storage_without_allocating)
{
    application_settings source;
    application_settings destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize("profile", "users.json", "test-value", FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.set_login_session_timeout_seconds(123));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.set_manual_login_approval_enabled(FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    const char *root_storage = source._database_root_path.c_str();
    const char *key_storage = source._encryption_key.c_str();
    cma_set_alloc_limit(1);
    const int32_t result = destination.move(source);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_FALSE, source.is_initialised());
    FT_ASSERT_EQ(root_storage, destination._database_root_path.c_str());
    FT_ASSERT_EQ(key_storage, destination._encryption_key.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(destination));
    ft_string root;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_database_root_path(root));
    FT_ASSERT(root == "profile");
    int64_t timeout = 0;
    ft_bool enabled = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_login_session_timeout_seconds(timeout));
    FT_ASSERT_EQ(123, timeout);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.is_manual_login_approval_enabled(enabled));
    FT_ASSERT_EQ(FT_TRUE, enabled);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_database_root_path(root));
    FT_ASSERT(root.empty());
    return (1);
}

FT_TEST(test_application_settings_location_preserves_both_paths_when_allocation_fails)
{
    application_settings settings;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, settings.initialize("old-root", "old.json"));
    cma_set_alloc_limit(3);
    const int32_t result = settings.set_database_location("x", "requires-more-memory.json");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    ft_string root;
    ft_string relative_path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, settings.get_database_root_path(root));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, settings.get_database_relative_path(relative_path));
    FT_ASSERT(root == "old-root");
    FT_ASSERT(relative_path == "old.json");
    return (1);
}
