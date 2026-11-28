#include "../../../Modules/Application/application_auth_service.hpp"
#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include "../../../Modules/System_utils/system_utils.hpp"
#include "../../../Modules/Template/move.hpp"
#include "../../../Modules/Template/vector.hpp"
#include <chrono>
#include <cstdio>

static int64_t check_signal_record(int32_t, const void *buffer, ft_size_t count)
{
    const char expected[] = "fixture-user: fixture-code\n";
    if (count != sizeof(expected) - 1 || ft_memcmp(buffer, expected, count) != 0)
        return (-1);
    return (static_cast<int64_t>(count));
}

int main(int argument_count, char **arguments)
{
    constexpr ft_size_t RECORD_COUNT = 64;
    constexpr ft_size_t ITERATIONS = 32;
    application_auth_service service;
    ft_string path;
    ft_string root;
    ft_string name;
    if (path.initialize() != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_session_listing_benchmark", "json", &path) != FT_ERR_SUCCESS
        || filesystem_split_path(path.c_str(), &root, &name) != FT_ERR_SUCCESS)
        return (1);
    const char empty_database[] = "{\"kv_store\":{}}";
    if (file_write_all(path.c_str(), empty_database, sizeof(empty_database) - 1)
        != FT_ERR_SUCCESS)
        return (2);
    const int32_t service_error = service.initialize(root.c_str(), name.c_str());
    (void)file_delete(path.c_str());
    if (service_error != FT_ERR_SUCCESS)
        return (3);

    if (argument_count == 2 && ft_strcmp(arguments[1], "--signal") == 0)
    {
        ft_size_t index = 0;
        su_set_write_syscall_hook(check_signal_record);
        const auto signal_start = std::chrono::steady_clock::now();
        while (index < ITERATIONS)
        {
            if (service.write_login_signal_one_time_password("fixture-user", "fixture-code")
                != FT_ERR_SUCCESS)
                break ;
            ++index;
        }
        const std::chrono::duration<double, std::micro> signal_elapsed =
            std::chrono::steady_clock::now() - signal_start;
        su_reset_write_syscall_hook();
        if (index != ITERATIONS)
            return (21);
        std::printf("%.3f us/operation\n", signal_elapsed.count() / ITERATIONS);
        return (0);
    }

    if (argument_count == 2 && ft_strcmp(arguments[1], "--remove") == 0)
    {
        std::chrono::duration<double, std::micro> removal_time{};
        ft_size_t index = 0;
        while (index < ITERATIONS)
        {
            if (service.register_user("fixture-user", "fixture-password") != FT_ERR_SUCCESS)
                return (18);
            const auto start = std::chrono::steady_clock::now();
            if (service.remove_user("fixture-user") != FT_ERR_SUCCESS)
                return (19);
            removal_time += std::chrono::steady_clock::now() - start;
            ++index;
        }
        ft_bool exists = FT_TRUE;
        if (service.user_exists("fixture-user", exists) != FT_ERR_SUCCESS || exists == FT_TRUE)
            return (20);
        std::printf("%.3f us/operation\n", removal_time.count() / ITERATIONS);
        return (0);
    }

    if (argument_count == 2 && ft_strcmp(arguments[1], "--record") == 0)
    {
        ft_string record;
        ft_size_t index = 0;
        const auto record_start = std::chrono::steady_clock::now();
        while (index < ITERATIONS)
        {
            if (service.build_credential_record("fixture-password", record) != FT_ERR_SUCCESS)
                return (16);
            ++index;
        }
        const std::chrono::duration<double, std::micro> record_elapsed =
            std::chrono::steady_clock::now() - record_start;
        if (record.size() != 97 || record.c_str()[32] != '|')
            return (17);
        std::printf("%.3f us/operation\n", record_elapsed.count() / ITERATIONS);
        return (0);
    }

    if (argument_count == 2 && ft_strcmp(arguments[1], "--verify") == 0)
    {
        if (service.register_user("fixture-user", "fixture-password") != FT_ERR_SUCCESS)
            return (14);
        ft_size_t index = 0;
        const auto verify_start = std::chrono::steady_clock::now();
        while (index < ITERATIONS)
        {
            ft_bool authenticated = FT_FALSE;
            if (service.authenticate_user("fixture-user", "fixture-password", authenticated)
                != FT_ERR_SUCCESS || authenticated != FT_TRUE)
                return (15);
            ++index;
        }
        const std::chrono::duration<double, std::micro> verify_elapsed =
            std::chrono::steady_clock::now() - verify_start;
        std::printf("%.3f us/operation\n", verify_elapsed.count() / ITERATIONS);
        return (0);
    }

    if (argument_count == 2 && ft_strcmp(arguments[1], "--audit") == 0)
    {
        ft_size_t index = 0;
        const auto audit_start = std::chrono::steady_clock::now();
        while (index < ITERATIONS)
        {
            if (service.record_login_failure("fixture-user", "192.0.2.1") != FT_ERR_SUCCESS
                || service.record_login_success("fixture-user", "192.0.2.2") != FT_ERR_SUCCESS)
                return (12);
            ++index;
        }
        const std::chrono::duration<double, std::micro> audit_elapsed =
            std::chrono::steady_clock::now() - audit_start;
        int64_t failures = 0;
        if (service.get_failed_login_count("fixture-user", failures) != FT_ERR_SUCCESS
            || failures != static_cast<int64_t>(ITERATIONS))
            return (13);
        std::printf("%.3f us/operation\n", audit_elapsed.count() / (2 * ITERATIONS));
        return (0);
    }

    ft_vector<kv_store_snapshot_entry> entries;
    if (entries.initialize() != FT_ERR_SUCCESS)
        return (4);
    ft_size_t index = 0;
    while (index < RECORD_COUNT)
    {
        char key[64];
        std::snprintf(key, sizeof(key), "fixture/%u", static_cast<uint32_t>(index));
        if (index == 0 || index == RECORD_COUNT - 1)
            std::snprintf(key, sizeof(key), "application/auth/sessions/user-%u",
                static_cast<uint32_t>(index));
        kv_store_snapshot_entry entry;
        entry.has_expiration = FT_FALSE;
        entry.expiration_timestamp = 0;
        if (entry.key.initialize(key) != FT_ERR_SUCCESS
            || entry.value.initialize(8192, 'x') != FT_ERR_SUCCESS
            || entries.push_back(ft_move(entry)) != FT_ERR_SUCCESS)
            return (5);
        ++index;
    }
    if (service._credential_store.import_snapshot(entries) != FT_ERR_SUCCESS)
        return (6);
    if (argument_count == 2 && ft_strcmp(arguments[1], "--move") == 0)
    {
        application_auth_service destination;
        index = 0;
        const auto move_start = std::chrono::steady_clock::now();
        while (index < ITERATIONS)
        {
            if (destination.move(service) != FT_ERR_SUCCESS
                || service.move(destination) != FT_ERR_SUCCESS)
                return (10);
            ++index;
        }
        const std::chrono::duration<double, std::micro> move_elapsed =
            std::chrono::steady_clock::now() - move_start;
        ft_vector<ft_string> moved_usernames;
        if (moved_usernames.initialize() != FT_ERR_SUCCESS
            || service.get_logged_in_usernames(moved_usernames) != FT_ERR_SUCCESS
            || moved_usernames.size() != 2 || moved_usernames[0] != "user-0"
            || moved_usernames[1] != "user-63")
            return (11);
        std::printf("%.3f us/operation\n", move_elapsed.count() / (2 * ITERATIONS));
        return (0);
    }
    ft_vector<ft_string> usernames;
    if (usernames.initialize() != FT_ERR_SUCCESS)
        return (7);
    index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (service.get_logged_in_usernames(usernames) != FT_ERR_SUCCESS)
            return (8);
        ++index;
    }
    const std::chrono::duration<double, std::micro> elapsed =
        std::chrono::steady_clock::now() - start;
    if (usernames.size() != 2 || usernames[0] != "user-0" || usernames[1] != "user-63")
        return (9);
    std::printf("%.3f us/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}
