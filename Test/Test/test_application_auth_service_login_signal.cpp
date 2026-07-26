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

static std::chrono::system_clock::time_point g_test_authentication_clock_time;

static std::chrono::system_clock::time_point
    test_authentication_clock_now(void)
{
    return (g_test_authentication_clock_time);
}

static ft_bool parse_login_signal_output(const char *output_buffer, ft_string &token_output)
{
    const char *separator_position;
    ft_size_t token_length;

    if (output_buffer == ft_nullptr)
        return (FT_FALSE);
    separator_position = std::strstr(output_buffer, ": ");
    if (separator_position == ft_nullptr)
        return (FT_FALSE);
    separator_position += 2;
    while (*separator_position == ' ')
        separator_position++;
    token_output.destroy();
    if (token_output.initialize(separator_position) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    token_length = token_output.size();
    if (token_length > 0 && token_output.c_str()[token_length - 1] == '\n')
    {
        if (token_output.erase(token_length - 1, 1) != FT_ERR_SUCCESS)
            return (FT_FALSE);
    }
    return (FT_TRUE);
}

FT_TEST(test_application_auth_service_login_signal_issues_one_time_password)
{
    application_auth_service service;
    ft_string *database_file_path;
    ft_string token_string;
    ft_string session_token_string;
    ft_bool authenticated;
    int64_t timeout_seconds;
    char root_template[] = "/tmp/libft_application_root_XXXXXX";
    char database_relative_path[] = "auth_db.json";
    char *database_root_path;
    int pipe_descriptors[2];
    int32_t error_code;
    char output_buffer[256];
    ssize_t read_result;

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
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pipe(pipe_descriptors));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.set_login_signal_output_file_descriptor(pipe_descriptors[1]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.set_manual_login_approval_enabled(FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.request_login_signal_one_time_password("alice"));
    timeout_seconds = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_login_signal_token_timeout_seconds(timeout_seconds));
    FT_ASSERT_EQ(300, timeout_seconds);
    FT_ASSERT_EQ(0, close(pipe_descriptors[1]));
    std::memset(output_buffer, 0, sizeof(output_buffer));
    read_result = read(pipe_descriptors[0], output_buffer, sizeof(output_buffer) - 1);
    FT_ASSERT(read_result > 0);
    if (read_result > 0)
        output_buffer[static_cast<size_t>(read_result)] = '\0';
    FT_ASSERT_EQ(0, close(pipe_descriptors[0]));
    FT_ASSERT_EQ(FT_TRUE, parse_login_signal_output(output_buffer, token_string));
    authenticated = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, service.login_with_signal("alice", token_string.c_str(), "127.0.0.1", session_token_string, authenticated));
    FT_ASSERT_EQ(FT_FALSE, authenticated);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.approve_login("alice"));
    authenticated = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.login_with_signal("alice", token_string.c_str(), "127.0.0.1", session_token_string, authenticated));
    FT_ASSERT_EQ(FT_TRUE, authenticated);
    authenticated = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, service.authenticate_login_signal_one_time_password("alice", token_string.c_str(), authenticated));
    FT_ASSERT_EQ(FT_FALSE, authenticated);
    authenticated = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, service.authenticate_login_signal_one_time_password("alice", token_string.c_str(), authenticated));
    FT_ASSERT_EQ(FT_FALSE, authenticated);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.destroy());
    FT_ASSERT_EQ(0, cleanup_application_database(database_file_path->c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, database_file_path->destroy());
    delete database_file_path;
    FT_ASSERT_EQ(0, rmdir(database_root_path));
    return (1);
}

FT_TEST(test_application_auth_service_login_signal_timeout_and_descriptor_change)
{
    application_auth_service service;
    ft_string *database_file_path;
    ft_string token_string_first;
    ft_string token_string_second;
    ft_bool authenticated;
    char root_template[] = "/tmp/libft_application_root_XXXXXX";
    char database_relative_path[] = "auth_db.json";
    char *database_root_path;
    int pipe_first[2];
    int pipe_second[2];
    int64_t timeout_seconds;
    int32_t error_code;
    char output_buffer_first[256];
    char output_buffer_second[256];
    ssize_t read_result;

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
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.register_user("bob", "hunter2"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.set_login_signal_token_timeout_seconds(1));
    timeout_seconds = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.get_login_signal_token_timeout_seconds(timeout_seconds));
    FT_ASSERT_EQ(1, timeout_seconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pipe(pipe_first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pipe(pipe_second));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.set_login_signal_output_file_descriptor(pipe_first[1]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.request_login_signal_one_time_password("bob"));
    FT_ASSERT_EQ(0, close(pipe_first[1]));
    std::memset(output_buffer_first, 0, sizeof(output_buffer_first));
    read_result = read(pipe_first[0], output_buffer_first, sizeof(output_buffer_first) - 1);
    FT_ASSERT(read_result > 0);
    if (read_result > 0)
        output_buffer_first[static_cast<size_t>(read_result)] = '\0';
    FT_ASSERT_EQ(0, close(pipe_first[0]));
    FT_ASSERT_EQ(FT_TRUE, parse_login_signal_output(output_buffer_first, token_string_first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.set_login_signal_output_file_descriptor(pipe_second[1]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.request_login_signal_one_time_password("bob"));
    std::memset(output_buffer_second, 0, sizeof(output_buffer_second));
    read_result = read(pipe_second[0], output_buffer_second, sizeof(output_buffer_second) - 1);
    FT_ASSERT(read_result > 0);
    if (read_result > 0)
        output_buffer_second[static_cast<size_t>(read_result)] = '\0';
    FT_ASSERT_EQ(FT_TRUE, parse_login_signal_output(output_buffer_second, token_string_second));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.authenticate_login_signal_one_time_password("bob", token_string_second.c_str(), authenticated));
    FT_ASSERT_EQ(FT_TRUE, authenticated);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.request_login_signal_one_time_password("bob"));
    std::memset(output_buffer_second, 0, sizeof(output_buffer_second));
    read_result = read(pipe_second[0], output_buffer_second, sizeof(output_buffer_second) - 1);
    FT_ASSERT(read_result > 0);
    if (read_result > 0)
        output_buffer_second[static_cast<size_t>(read_result)] = '\0';
    FT_ASSERT_EQ(FT_TRUE, parse_login_signal_output(output_buffer_second, token_string_second));
    g_test_authentication_clock_time = std::chrono::system_clock::now();
    time_set_clock_now_hook(test_authentication_clock_now);
    g_test_authentication_clock_time += std::chrono::seconds(2);
    authenticated = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, service.authenticate_login_signal_one_time_password("bob", token_string_second.c_str(), authenticated));
    FT_ASSERT_EQ(FT_FALSE, authenticated);
    time_reset_clock_now_hook();
    FT_ASSERT_EQ(0, close(pipe_second[1]));
    FT_ASSERT_EQ(0, close(pipe_second[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.destroy());
    FT_ASSERT_EQ(0, cleanup_application_database(database_file_path->c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, database_file_path->destroy());
    delete database_file_path;
    FT_ASSERT_EQ(0, rmdir(database_root_path));
    return (1);
}
