#ifndef APPLICATION_AUTH_SERVICE_HPP
#define APPLICATION_AUTH_SERVICE_HPP

#include <stdint.h>
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Errno/errno.hpp"
#include "../Storage/kv_store.hpp"
#include "application_settings.hpp"

template <typename ElementType> class ft_vector;

class application_auth_service
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        kv_store _credential_store;
        application_settings _settings;
        ft_string _database_path;

        void destroy_partial_copy_state() noexcept;
        int32_t initialize_from_copy(const application_auth_service &other) noexcept;
        int32_t initialize_from_move(application_auth_service &other) noexcept;
        int32_t build_user_key(const char *username, ft_string &key_output) const noexcept;
        int32_t build_login_approval_key(const char *username, ft_string &key_output) const noexcept;
        int32_t build_user_approval_requirement_key(const char *username, ft_string &key_output) const noexcept;
        int32_t build_manual_login_approval_setting_key(ft_string &key_output) const noexcept;
        int32_t build_login_signal_one_time_password_key(const char *username, ft_string &key_output) const noexcept;
        int32_t build_login_session_key(const char *username, ft_string &key_output) const noexcept;
        int32_t build_user_stat_key(const char *key_prefix, const char *username, ft_string &key_output) const noexcept;
        int32_t load_integer_value(const char *key, int64_t &value, ft_bool *found = ft_nullptr) const noexcept;
        int32_t load_string_value(const char *key, ft_string &value, ft_bool *found = ft_nullptr) const noexcept;
        int32_t generate_login_signal_one_time_password(ft_string &one_time_password_output) const noexcept;
        int32_t generate_login_session_token(ft_string &session_token_output) const noexcept;
        int32_t build_login_signal_one_time_password_hash(const char *one_time_password, ft_string &hash_hex) const noexcept;
        int32_t write_login_signal_one_time_password(const char *username, const char *one_time_password) const noexcept;
        int32_t build_login_session_value(const char *session_token, const char *client_ip_address,
            int64_t login_timestamp, ft_string &value_output) const noexcept;
        int32_t parse_login_session_value(const char *value, ft_string *client_ip_address_output,
            int64_t &login_timestamp_output) const noexcept;
        int32_t is_login_access_allowed(const char *username) const noexcept;
        int32_t build_credential_record(const char *password, ft_string &record_output) const noexcept;
        int32_t load_manual_login_approval_enabled(ft_bool &enabled) const noexcept;
        int32_t load_user_login_approval_required(const char *username, ft_bool &required) const noexcept;
        int32_t record_login_failure(const char *username, const char *client_ip_address) noexcept;
        int32_t record_login_success(const char *username, const char *client_ip_address) noexcept;
        int32_t get_session_username_from_key(const char *session_key, ft_string &username_output) const noexcept;
        int32_t load_user_session_value(const char *username, ft_string &session_value, ft_bool *found = ft_nullptr) const noexcept;
        int32_t collect_logged_in_usernames(ft_vector<ft_string> &usernames) const noexcept;
        int32_t begin_user_session(const char *username, const char *client_ip_address,
            ft_string &session_token_output) noexcept;

    public:
        application_auth_service() noexcept;
        application_auth_service(const application_auth_service &other) noexcept = delete;
        application_auth_service(application_auth_service &&other) noexcept = delete;
        ~application_auth_service() noexcept;

        application_auth_service &operator=(const application_auth_service &other) noexcept = delete;
        application_auth_service &operator=(application_auth_service &&other) noexcept = delete;

        int32_t initialize(const application_settings &settings) noexcept;
        int32_t initialize(const char *database_root_path, const char *database_relative_path, const char *encryption_key = ft_nullptr, ft_bool enable_encryption = FT_FALSE, const char *encryption_algorithm_name = ft_nullptr) noexcept;
        int32_t initialize(const application_auth_service &other) noexcept;
        int32_t initialize(application_auth_service &&other) noexcept;
        int32_t destroy() noexcept;
        int32_t move(application_auth_service &other) noexcept;

        ft_bool is_initialised() const noexcept;

        int32_t register_user(const char *username, const char *password) noexcept;
        int32_t authenticate_user(const char *username, const char *password, ft_bool &authenticated) const noexcept;
        int32_t user_exists(const char *username, ft_bool &exists) const noexcept;
        int32_t remove_user(const char *username) noexcept;
        int32_t set_manual_login_approval_enabled(ft_bool enabled) noexcept;
        int32_t is_manual_login_approval_enabled(ft_bool &enabled) const noexcept;
        int32_t set_login_approval_required_for_user(const char *username, ft_bool enabled) noexcept;
        int32_t is_login_approval_required_for_user(const char *username, ft_bool &enabled) const noexcept;
        int32_t approve_login(const char *username) noexcept;
        int32_t revoke_login_approval(const char *username) noexcept;
        int32_t is_login_approved(const char *username, ft_bool &approved) const noexcept;
        int32_t set_login_signal_output_file_descriptor(int32_t file_descriptor) noexcept;
        int32_t get_login_signal_output_file_descriptor(int32_t &file_descriptor) const noexcept;
        int32_t set_login_signal_token_timeout_seconds(int64_t timeout_seconds) noexcept;
        int32_t get_login_signal_token_timeout_seconds(int64_t &timeout_seconds) const noexcept;
        int32_t set_login_session_timeout_seconds(int64_t timeout_seconds) noexcept;
        int32_t get_login_session_timeout_seconds(int64_t &timeout_seconds) const noexcept;
        int32_t login_user(const char *username, const char *password, const char *client_ip_address,
            ft_string &session_token_output, ft_bool &authenticated) noexcept;
        int32_t login_with_signal(const char *username, const char *one_time_password, const char *client_ip_address,
            ft_string &session_token_output, ft_bool &authenticated) noexcept;
        int32_t end_user_session(const char *username) noexcept;
        int32_t is_user_logged_in(const char *username, ft_bool &logged_in) const noexcept;
        int32_t get_logged_in_usernames(ft_vector<ft_string> &usernames) const noexcept;
        int32_t get_failed_login_count(const char *username, int64_t &failed_login_count) const noexcept;
        int32_t get_consecutive_failed_login_count(const char *username, int64_t &failed_login_count) const noexcept;
        int32_t get_last_failed_login_ip_address(const char *username, ft_string &client_ip_address) const noexcept;
        int32_t get_last_failed_login_timestamp(const char *username, int64_t &timestamp) const noexcept;
        int32_t get_last_successful_login_ip_address(const char *username, ft_string &client_ip_address) const noexcept;
        int32_t get_last_successful_login_timestamp(const char *username, int64_t &timestamp) const noexcept;
        int32_t get_login_session_client_ip_address(const char *username, ft_string &client_ip_address) const noexcept;
        int32_t get_login_session_timestamp(const char *username, int64_t &timestamp) const noexcept;
        int32_t request_login_signal_one_time_password(const char *username) noexcept;
        int32_t authenticate_login_signal_one_time_password(const char *username, const char *one_time_password, ft_bool &authenticated) noexcept;
};

#endif
