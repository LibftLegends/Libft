#include "../test_internal.hpp"
#include "../../Modules/Application/application.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Time/time.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/vector.hpp"

static int32_t cleanup_application_database(const char *file_path)
{
    return (std::remove(file_path));
}

static int32_t create_application_database(const char *file_path)
{
    FILE *file_pointer;
    int32_t write_result;

    file_pointer = std::fopen(file_path, "w");
    if (file_pointer == ft_nullptr)
        return (FT_ERR_IO);
    write_result = std::fputs("{\n  \"kv_store\": {\n  }\n}\n", file_pointer);
    if (write_result < 0)
    {
        (void)std::fclose(file_pointer);
        return (FT_ERR_IO);
    }
    if (std::fclose(file_pointer) != 0)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

static std::chrono::system_clock::time_point g_session_timeout_test_time;

static std::chrono::system_clock::time_point session_timeout_test_clock_now(void)
{
    return (g_session_timeout_test_time);
}

static int32_t session_timeout_get_usernames_at(
    application_auth_service &service, ft_vector<ft_string> &usernames,
    int64_t timestamp)
{
    int32_t error_code;

    g_session_timeout_test_time = std::chrono::system_clock::time_point(
        std::chrono::seconds(timestamp));
    time_set_clock_now_hook(session_timeout_test_clock_now);
    error_code = service.get_logged_in_usernames(usernames);
    time_reset_clock_now_hook();
    return (error_code);
}

static ft_bool vector_contains_string(const ft_vector<ft_string> &strings, const char *expected_string)
{
    ft_size_t index;
    ft_size_t string_count;

    if (expected_string == ft_nullptr)
        return (FT_FALSE);
    string_count = strings.size();
    index = 0;
    while (index < string_count)
    {
        if (ft_strcmp(strings[index].c_str(), expected_string) == FT_ERR_SUCCESS)
            return (FT_TRUE);
        index++;
    }
    return (FT_FALSE);
}

FT_TEST(test_application_auth_service_failed_login_audit_and_session_lifecycle)
{
    application_auth_service service;
    ft_string *database_file_path;
    ft_string session_token_string;
    ft_string string_output;
    ft_vector<ft_string> usernames;
    ft_bool authenticated;
    ft_bool logged_in;
    int64_t failed_login_count;
    int64_t consecutive_failed_login_count;
    int64_t timestamp_output;
    char root_template[] = "/tmp/libft_application_root_XXXXXX";
    char database_relative_path[] = "auth_db.json";
    char *database_root_path;
    int32_t error_code;

    database_root_path = mkdtemp(root_template);
    FT_ASSERT(database_root_path != ft_nullptr);
    if (database_root_path == ft_nullptr)
        return (1);
    database_file_path = filesystem_safe_join_path(database_root_path, database_relative_path);
    FT_ASSERT(database_file_path != ft_nullptr);
    if (database_file_path == ft_nullptr)
    {
        FT_ASSERT_EQ(0, rmdir(database_root_path));
        return (1);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, create_application_database(database_file_path->c_str()));
    error_code = service.initialize(database_root_path, database_relative_path, "app-auth-key-123", FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    if (error_code != FT_ERR_SUCCESS)
    {
        FT_ASSERT_EQ(0, cleanup_application_database(database_file_path->c_str()));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, database_file_path->destroy());
        delete database_file_path;
        FT_ASSERT_EQ(0, rmdir(database_root_path));
        return (1);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.register_user("alice", "s3cret"));
    authenticated = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, service.login_user("alice", "wrong", "10.0.0.1", session_token_string, authenticated));
    FT_ASSERT_EQ(FT_FALSE, authenticated);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_failed_login_count("alice", failed_login_count));
    FT_ASSERT_EQ(1, failed_login_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_consecutive_failed_login_count("alice", consecutive_failed_login_count));
    FT_ASSERT_EQ(1, consecutive_failed_login_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_last_failed_login_ip_address("alice", string_output));
    FT_ASSERT_EQ(FT_TRUE, string_output == "10.0.0.1");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_last_failed_login_timestamp("alice", timestamp_output));
    FT_ASSERT(timestamp_output > 0);
    authenticated = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, service.login_user("alice", "still-wrong", "10.0.0.2", session_token_string, authenticated));
    FT_ASSERT_EQ(FT_FALSE, authenticated);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_failed_login_count("alice", failed_login_count));
    FT_ASSERT_EQ(2, failed_login_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_consecutive_failed_login_count("alice", consecutive_failed_login_count));
    FT_ASSERT_EQ(2, consecutive_failed_login_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_last_failed_login_ip_address("alice", string_output));
    FT_ASSERT_EQ(FT_TRUE, string_output == "10.0.0.2");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.login_user("alice", "s3cret", "10.0.0.3", session_token_string, authenticated));
    FT_ASSERT_EQ(FT_TRUE, authenticated);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.is_user_logged_in("alice", logged_in));
    FT_ASSERT_EQ(FT_TRUE, logged_in);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_login_session_client_ip_address("alice", string_output));
    FT_ASSERT_EQ(FT_TRUE, string_output == "10.0.0.3");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_login_session_timestamp("alice", timestamp_output));
    FT_ASSERT(timestamp_output > 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_last_successful_login_ip_address("alice", string_output));
    FT_ASSERT_EQ(FT_TRUE, string_output == "10.0.0.3");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_last_successful_login_timestamp("alice", timestamp_output));
    FT_ASSERT(timestamp_output > 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_consecutive_failed_login_count("alice", consecutive_failed_login_count));
    FT_ASSERT_EQ(0, consecutive_failed_login_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_failed_login_count("alice", failed_login_count));
    FT_ASSERT_EQ(2, failed_login_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, usernames.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_logged_in_usernames(usernames));
    FT_ASSERT_EQ(1U, usernames.size());
    FT_ASSERT_EQ(FT_TRUE, vector_contains_string(usernames, "alice"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.end_user_session("alice"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.is_user_logged_in("alice", logged_in));
    FT_ASSERT_EQ(FT_FALSE, logged_in);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, service.get_login_session_client_ip_address("alice", string_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_logged_in_usernames(usernames));
    FT_ASSERT_EQ(0U, usernames.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, usernames.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.destroy());
    FT_ASSERT_EQ(0, cleanup_application_database(database_file_path->c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, database_file_path->destroy());
    delete database_file_path;
    FT_ASSERT_EQ(0, rmdir(database_root_path));
    return (1);
}

FT_TEST(test_application_auth_service_session_timeout_expires_and_list_updates)
{
    application_auth_service service;
    ft_string *database_file_path;
    ft_vector<ft_string> usernames;
    ft_bool logged_in;
    ft_string string_output;
    int64_t timestamp_output;
    char root_template[] = "/tmp/libft_application_root_XXXXXX";
    char database_relative_path[] = "auth_db.json";
    char *database_root_path;
    int32_t error_code;

    database_root_path = mkdtemp(root_template);
    FT_ASSERT(database_root_path != ft_nullptr);
    if (database_root_path == ft_nullptr)
        return (1);
    database_file_path = filesystem_safe_join_path(database_root_path, database_relative_path);
    FT_ASSERT(database_file_path != ft_nullptr);
    if (database_file_path == ft_nullptr)
    {
        FT_ASSERT_EQ(0, rmdir(database_root_path));
        return (1);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, create_application_database(database_file_path->c_str()));
    error_code = service.initialize(database_root_path, database_relative_path, "app-auth-key-123", FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    if (error_code != FT_ERR_SUCCESS)
    {
        FT_ASSERT_EQ(0, cleanup_application_database(database_file_path->c_str()));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, database_file_path->destroy());
        delete database_file_path;
        FT_ASSERT_EQ(0, rmdir(database_root_path));
        return (1);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.register_user("alice", "s3cret"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.register_user("bob", "hunter2"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.set_login_session_timeout_seconds(5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.login_user("alice", "s3cret", "192.168.1.10", string_output, logged_in));
    FT_ASSERT_EQ(FT_TRUE, logged_in);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.login_user("bob", "hunter2", "192.168.1.11", string_output, logged_in));
    FT_ASSERT_EQ(FT_TRUE, logged_in);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, usernames.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_logged_in_usernames(usernames));
    FT_ASSERT_EQ(2U, usernames.size());
    FT_ASSERT_EQ(FT_TRUE, vector_contains_string(usernames, "alice"));
    FT_ASSERT_EQ(FT_TRUE, vector_contains_string(usernames, "bob"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        service.get_login_session_timestamp("alice", timestamp_output));
    error_code = session_timeout_get_usernames_at(service, usernames,
        timestamp_output - 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(2U, usernames.size());
    error_code = session_timeout_get_usernames_at(service, usernames,
        timestamp_output + 4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(2U, usernames.size());
    error_code = session_timeout_get_usernames_at(service, usernames,
        timestamp_output + 5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(0U, usernames.size());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, service.get_login_session_timestamp("alice", timestamp_output));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, service.get_login_session_client_ip_address("bob", string_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.is_user_logged_in("alice", logged_in));
    FT_ASSERT_EQ(FT_FALSE, logged_in);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.is_user_logged_in("bob", logged_in));
    FT_ASSERT_EQ(FT_FALSE, logged_in);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, usernames.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.destroy());
    FT_ASSERT_EQ(0, cleanup_application_database(database_file_path->c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, database_file_path->destroy());
    delete database_file_path;
    FT_ASSERT_EQ(0, rmdir(database_root_path));
    return (1);
}
