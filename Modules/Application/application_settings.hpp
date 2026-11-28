#ifndef APPLICATION_SETTINGS_HPP
#define APPLICATION_SETTINGS_HPP

#include <stdint.h>
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Errno/errno.hpp"

class application_settings
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        ft_string _database_root_path;
        ft_string _database_relative_path;
        ft_string _encryption_key;
        ft_string _encryption_algorithm_name;
        ft_bool _encryption_enabled;
        ft_bool _manual_login_approval_enabled;
        int32_t _login_signal_output_file_descriptor;
        int64_t _login_signal_token_timeout_seconds;
        int64_t _login_session_timeout_seconds;

        int32_t initialize_from_copy(const application_settings &other) noexcept;
        int32_t initialize_from_move(application_settings &other) noexcept;
        void copy_scalar_settings(const application_settings &other) noexcept;
        void destroy_partial_copy_state() noexcept;

    public:
        application_settings() noexcept;
        application_settings(const application_settings &other) noexcept = delete;
        application_settings(application_settings &&other) noexcept = delete;
        ~application_settings() noexcept;

        application_settings &operator=(const application_settings &other) noexcept = delete;
        application_settings &operator=(application_settings &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const char *database_root_path, const char *database_relative_path, const char *encryption_key = ft_nullptr, ft_bool enable_encryption = FT_FALSE, const char *encryption_algorithm_name = ft_nullptr) noexcept;
        int32_t initialize(const application_settings &other) noexcept;
        int32_t initialize(application_settings &&other) noexcept;
        int32_t destroy() noexcept;
        int32_t move(application_settings &other) noexcept;

        ft_bool is_initialised() const noexcept;

        int32_t set_database_location(const char *database_root_path, const char *database_relative_path) noexcept;
        int32_t get_database_root_path(ft_string &database_root_path) const noexcept;
        int32_t get_database_relative_path(ft_string &database_relative_path) const noexcept;
        int32_t set_encryption_key(const char *encryption_key) noexcept;
        int32_t get_encryption_key(ft_string &encryption_key) const noexcept;
        int32_t set_encryption_algorithm_name(const char *encryption_algorithm_name) noexcept;
        int32_t get_encryption_algorithm_name(ft_string &encryption_algorithm_name) const noexcept;
        int32_t set_encryption_enabled(ft_bool enabled) noexcept;
        int32_t is_encryption_enabled(ft_bool &enabled) const noexcept;
        int32_t set_manual_login_approval_enabled(ft_bool enabled) noexcept;
        int32_t is_manual_login_approval_enabled(ft_bool &enabled) const noexcept;
        int32_t set_login_signal_output_file_descriptor(int32_t file_descriptor) noexcept;
        int32_t get_login_signal_output_file_descriptor(int32_t &file_descriptor) const noexcept;
        int32_t set_login_signal_token_timeout_seconds(int64_t timeout_seconds) noexcept;
        int32_t get_login_signal_token_timeout_seconds(int64_t &timeout_seconds) const noexcept;
        int32_t set_login_session_timeout_seconds(int64_t timeout_seconds) noexcept;
        int32_t get_login_session_timeout_seconds(int64_t &timeout_seconds) const noexcept;
};

#endif
