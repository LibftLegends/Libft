#include "../test_internal.hpp"
#include "../../Modules/Application/application_auth_service.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Template/vector.hpp"

static char g_auth_signal_bytes[256];
static ft_size_t g_auth_signal_length;

static int64_t capture_auth_signal_short_write(int32_t descriptor, const void *buffer, ft_size_t count)
{
    if (descriptor != 17 || count == 0 || g_auth_signal_length >= sizeof(g_auth_signal_bytes))
        return (-1);
    g_auth_signal_bytes[g_auth_signal_length++] = *static_cast<const char *>(buffer);
    return (1);
}

static int64_t stall_auth_signal_write(int32_t, const void *, ft_size_t)
{
    return (0);
}

FT_TEST(test_application_auth_service_signal_output_handles_short_writes_without_allocating)
{
    application_auth_service service;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service._settings.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service._settings.set_login_signal_output_file_descriptor(17));
    const char expected[] = "fixture-user: fixture-code\n";
    g_auth_signal_length = 0;
    su_set_write_syscall_hook(capture_auth_signal_short_write);
    cma_set_alloc_limit(1);
    const int32_t result = service.write_login_signal_one_time_password("fixture-user", "fixture-code");
    cma_set_alloc_limit(0);
    su_reset_write_syscall_hook();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(sizeof(expected) - 1, g_auth_signal_length);
    FT_ASSERT_EQ(0, ft_memcmp(g_auth_signal_bytes, expected, sizeof(expected) - 1));
    su_set_write_syscall_hook(stall_auth_signal_write);
    const int32_t stalled = service.write_login_signal_one_time_password("fixture-user", "fixture-code");
    su_reset_write_syscall_hook();
    FT_ASSERT_EQ(FT_ERR_IO, stalled);
    return (1);
}

FT_TEST(test_application_auth_service_long_signal_output_fits_exact_heap_buffer)
{
    application_auth_service service;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service._settings.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service._settings.set_login_signal_output_file_descriptor(17));
    ft_string username;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, username.initialize(160, 'u'));
    g_auth_signal_length = 0;
    su_set_write_syscall_hook(capture_auth_signal_short_write);
    cma_set_alloc_limit(167);
    const int32_t result = service.write_login_signal_one_time_password(username.c_str(), "code");
    cma_set_alloc_limit(0);
    su_reset_write_syscall_hook();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(167U, g_auth_signal_length);
    FT_ASSERT_EQ(0, ft_memcmp(g_auth_signal_bytes, username.c_str(), 160));
    FT_ASSERT_EQ(0, ft_memcmp(g_auth_signal_bytes + 160, ": code\n", 7));
    return (1);
}

FT_TEST(test_application_auth_service_hex_reuses_the_encoder_allocation)
{
    application_auth_service service;
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("previous"));
    cma_set_alloc_limit(65);
    const int32_t result = service.build_login_signal_one_time_password_hash("fixture-value", output);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT(output == "0248f56577e62962fd6ae2f4d93b4ae9323f4976469d53ffa6953d3bd0f1664d");
    return (1);
}

FT_TEST(test_application_auth_service_credential_record_uses_one_exact_allocation)
{
    application_auth_service service;
    ft_string record;
    cma_set_alloc_limit(98);
    const int32_t result = service.build_credential_record("fixture-password", record);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(97U, record.size());
    FT_ASSERT_EQ('|', record.c_str()[32]);
    FT_ASSERT_EQ('\0', record.c_str()[97]);
    cma_set_alloc_limit(1);
    const int32_t failed_build = service.build_credential_record("another-fixture", record);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_build);
    FT_ASSERT_EQ(97U, record.size());
    FT_ASSERT_EQ('|', record.c_str()[32]);
    return (1);
}

FT_TEST(test_application_auth_service_uses_validated_path_and_preserves_self_move)
{
    ft_string path;
    ft_string root;
    ft_string name;
    application_auth_service service;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path("libft_auth_ownership", "json", &path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, root.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_split_path(path.c_str(), &root, &name));
    const char empty_database[] = "{\"kv_store\":{}}";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path.c_str(), empty_database,
        sizeof(empty_database) - 1));
    const int32_t init_error = service.initialize(root.c_str(), name.c_str());
    if (init_error != FT_ERR_SUCCESS)
        (void)file_delete(path.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, init_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.move(service));
    FT_ASSERT_EQ(FT_TRUE, service.is_initialised());
    ft_bool exists = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.user_exists("fixture-user", exists));
    FT_ASSERT_EQ(FT_FALSE, exists);
    const int32_t store_result = service._credential_store.kv_set(
        "application/auth/sessions/fixture-user", "fixture-session|192.0.2.7|42");
    ft_string address;
    int64_t timestamp = 0;
    const int32_t address_result = service.get_login_session_client_ip_address("fixture-user", address);
    const int32_t timestamp_result = service.get_login_session_timestamp("fixture-user", timestamp);
    ft_vector<ft_string> usernames;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, usernames.initialize());
    ft_string unrelated_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, unrelated_value.initialize(4096, 'x'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service._credential_store.kv_set(
        "fixture/unrelated", unrelated_value.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service._credential_store.kv_set(
        "application/auth/sessions/", "fixture"));
    cma_set_alloc_limit(256);
    const int32_t listing_result = service.get_logged_in_usernames(usernames);
    cma_set_alloc_limit(0);
    cma_set_alloc_limit(1);
    const int32_t failed_listing = service.get_logged_in_usernames(usernames);
    cma_set_alloc_limit(0);
    application_auth_service moved;
    const char *previous_path = service._database_path.c_str();
    const char *session_key = "application/auth/sessions/fixture-user";
    const char *previous_session = service._credential_store.kv_get(session_key);
    cma_set_alloc_limit(1);
    const int32_t failed_move = moved.move(service);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_move);
    FT_ASSERT_EQ(FT_TRUE, service.is_initialised());
    FT_ASSERT_EQ(FT_FALSE, moved.is_initialised());
    FT_ASSERT_EQ(previous_path, service._database_path.c_str());
    FT_ASSERT(previous_session != ft_nullptr);
    FT_ASSERT_EQ(previous_session, service._credential_store.kv_get(session_key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.move(service));
    FT_ASSERT_EQ(FT_FALSE, service.is_initialised());
    FT_ASSERT_EQ(previous_path, moved._database_path.c_str());
    FT_ASSERT_EQ(previous_session, moved._credential_store.kv_get(session_key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_login_session_timestamp("fixture-user", timestamp));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.record_login_failure("fixture-user", "192.0.2.1"));
    cma_set_alloc_limit(1024);
    const int32_t audit_result = moved.record_login_failure("fixture-user", unrelated_value.c_str());
    cma_set_alloc_limit(0);
    int64_t failed_count = 0;
    ft_string failed_address;
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, audit_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_failed_login_count("fixture-user", failed_count));
    FT_ASSERT_EQ(1, failed_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_last_failed_login_ip_address("fixture-user", failed_address));
    FT_ASSERT(failed_address == "192.0.2.1");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.record_login_success("fixture-user", "192.0.2.2"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_consecutive_failed_login_count("fixture-user", failed_count));
    FT_ASSERT_EQ(0, failed_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved._credential_store.kv_set(
        "application/auth/users/legacy-fixture",
        "legacy-salt|4330d139552edf04970922fb9ade2927a9b07a3840c1428ade18080c51eaca61"));
    ft_bool authenticated = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.authenticate_user(
        "legacy-fixture", "fixture-password", authenticated));
    FT_ASSERT_EQ(FT_TRUE, authenticated);
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, moved.authenticate_user(
        "legacy-fixture", "wrong-password", authenticated));
    FT_ASSERT_EQ(FT_FALSE, authenticated);
    ft_string long_password;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, long_password.initialize(256, 'p'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.register_user("long-fixture", long_password.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.authenticate_user(
        "long-fixture", long_password.c_str(), authenticated));
    FT_ASSERT_EQ(FT_TRUE, authenticated);
    cma_set_alloc_limit(1);
    const int32_t missing_delete = moved._credential_store.kv_delete("fixture/missing-key");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, missing_delete);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.remove_user("legacy-fixture"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.remove_user("legacy-fixture"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.user_exists("legacy-fixture", exists));
    FT_ASSERT_EQ(FT_FALSE, exists);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_login_session_timestamp("fixture-user", timestamp));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.remove_user("fixture-user"));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, moved.get_login_session_timestamp("fixture-user", timestamp));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, address_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, timestamp_result);
    FT_ASSERT(address == "192.0.2.7");
    FT_ASSERT_EQ(42, timestamp);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, listing_result);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_listing);
    FT_ASSERT_EQ(1U, usernames.size());
    FT_ASSERT(usernames[0] == "fixture-user");
    return (1);
}

FT_TEST(test_application_auth_service_builds_keys_with_one_exact_allocation)
{
    application_auth_service service;
    ft_string key;
    const char username[] = "fixture-user-with-a-long-identifier";
    const char expected[] = "application/auth/users/fixture-user-with-a-long-identifier";
    cma_set_alloc_limit(sizeof(expected));
    const int32_t result = service.build_user_key(username, key);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT(key == expected);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.build_login_session_key("fixture-user", key));
    FT_ASSERT(key == "application/auth/sessions/fixture-user");
    cma_set_alloc_limit(1);
    const int32_t failed_update = service.build_login_approval_key(username, key);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_update);
    FT_ASSERT(key == "application/auth/sessions/fixture-user");
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, service.build_user_key("", key));
    return (1);
}

FT_TEST(test_application_auth_service_session_timestamp_preserves_decimal_format)
{
    application_auth_service service;
    ft_string value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.build_login_session_value("token", nullptr,
        INT64_MIN, value));
    FT_ASSERT(value == "token||-9223372036854775808");
    return (1);
}

FT_TEST(test_application_auth_service_session_writer_builds_one_exact_record)
{
    application_auth_service service;
    ft_string output;
    const char expected[] = "fixture-session|2001:db8::7|1700000000";
    cma_set_alloc_limit(sizeof(expected));
    const int32_t result = service.build_login_session_value(
        "fixture-session", "2001:db8::7", 1700000000, output);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT(output == expected);
    ft_string address;
    int64_t timestamp = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.parse_login_session_value(output.c_str(), &address, timestamp));
    FT_ASSERT(address == "2001:db8::7");
    FT_ASSERT_EQ(1700000000, timestamp);
    return (1);
}

FT_TEST(test_application_auth_service_session_writer_preserves_old_output_on_failure)
{
    application_auth_service service;
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("fixture-session"));
    cma_set_alloc_limit(1);
    const int32_t result = service.build_login_session_value(
        output.c_str(), "192.0.2.7", 42, output);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT(output == "fixture-session");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.build_login_session_value(
        output.c_str(), "192.0.2.7", 42, output));
    FT_ASSERT(output == "fixture-session|192.0.2.7|42");
    return (1);
}

FT_TEST(test_application_auth_service_session_decoding_materializes_only_the_requested_address)
{
    application_auth_service service;
    ft_string address;
    int64_t timestamp = -1;
    cma_set_alloc_limit(1);
    const int32_t timestamp_result = service.parse_login_session_value(
        "fixture-session|192.0.2.7|1700000000", nullptr, timestamp);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, timestamp_result);
    FT_ASSERT_EQ(1700000000, timestamp);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.parse_login_session_value(
        "fixture-session|192.0.2.7|42", &address, timestamp));
    FT_ASSERT(address == "192.0.2.7");
    FT_ASSERT_EQ(42, timestamp);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.parse_login_session_value(
        "fixture-session||7", &address, timestamp));
    FT_ASSERT(address.empty());
    FT_ASSERT_EQ(7, timestamp);
    return (1);
}

FT_TEST(test_application_auth_service_session_decoding_preserves_outputs_on_failure)
{
    application_auth_service service;
    ft_string address;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, address.initialize("saved"));
    int64_t timestamp = 17;
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, service.parse_login_session_value(
        "fixture-session|192.0.2.7|invalid", &address, timestamp));
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, service.parse_login_session_value(
        "fixture-session|192.0.2.7", &address, timestamp));
    cma_set_alloc_limit(1);
    const int32_t result = service.parse_login_session_value(
        "fixture-session|192.0.2.7|23", &address, timestamp);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT(address == "saved");
    FT_ASSERT_EQ(17, timestamp);
    address = "fixture-session|192.0.2.8|25";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, address.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, service.parse_login_session_value(address.c_str(),
        &address, timestamp));
    FT_ASSERT(address == "192.0.2.8");
    FT_ASSERT_EQ(25, timestamp);
    return (1);
}
