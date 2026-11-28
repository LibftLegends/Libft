#include "application_settings.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Errno/errno.hpp"
#include <unistd.h>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static constexpr int64_t DEFAULT_LOGIN_SIGNAL_TOKEN_TIMEOUT_SECONDS = 300;
static constexpr int64_t DEFAULT_LOGIN_SESSION_TIMEOUT_SECONDS = 28800;

void application_settings::destroy_partial_copy_state() noexcept
{
    (void)this->_database_root_path.destroy();
    (void)this->_database_relative_path.destroy();
    (void)this->_encryption_key.destroy();
    (void)this->_encryption_algorithm_name.destroy();
    this->_encryption_enabled = FT_FALSE;
    this->_manual_login_approval_enabled = FT_FALSE;
    this->_login_signal_output_file_descriptor = STDOUT_FILENO;
    this->_login_signal_token_timeout_seconds = DEFAULT_LOGIN_SIGNAL_TOKEN_TIMEOUT_SECONDS;
    this->_login_session_timeout_seconds = DEFAULT_LOGIN_SESSION_TIMEOUT_SECONDS;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return ;
}

application_settings::application_settings() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED)
    , _database_root_path()
    , _database_relative_path()
    , _encryption_key()
    , _encryption_algorithm_name()
    , _encryption_enabled(FT_FALSE)
    , _manual_login_approval_enabled(FT_FALSE)
    , _login_signal_output_file_descriptor(STDOUT_FILENO)
    , _login_signal_token_timeout_seconds(DEFAULT_LOGIN_SIGNAL_TOKEN_TIMEOUT_SECONDS)
    , _login_session_timeout_seconds(DEFAULT_LOGIN_SESSION_TIMEOUT_SECONDS)
{
    return ;
}

application_settings::~application_settings() noexcept
{
    (void)this->destroy();
    return ;
}

ft_bool application_settings::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t application_settings::destroy() noexcept
{
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    error_code = FT_ERR_SUCCESS;
    if (this->_database_root_path.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_INVALID_OPERATION;
    if (this->_database_relative_path.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_INVALID_OPERATION;
    if (this->_encryption_key.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_INVALID_OPERATION;
    if (this->_encryption_algorithm_name.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_INVALID_OPERATION;
    this->_encryption_enabled = FT_FALSE;
    this->_manual_login_approval_enabled = FT_FALSE;
    this->_login_signal_output_file_descriptor = STDOUT_FILENO;
    this->_login_signal_token_timeout_seconds = DEFAULT_LOGIN_SIGNAL_TOKEN_TIMEOUT_SECONDS;
    this->_login_session_timeout_seconds = DEFAULT_LOGIN_SESSION_TIMEOUT_SECONDS;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (error_code);
}

int32_t application_settings::initialize_from_copy(const application_settings &other) noexcept
{
    int32_t error_code;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "application_settings::initialize(copy)", "source is uninitialised");
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        error_code = this->destroy();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        error_code = this->destroy();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    error_code = this->_database_root_path.initialize(other._database_root_path);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    error_code = this->_database_relative_path.initialize(other._database_relative_path);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    error_code = this->_encryption_key.initialize(other._encryption_key);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    error_code = this->_encryption_algorithm_name.initialize(other._encryption_algorithm_name);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    this->copy_scalar_settings(other);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

void application_settings::copy_scalar_settings(const application_settings &other) noexcept
{
    this->_encryption_enabled = other._encryption_enabled;
    this->_manual_login_approval_enabled = other._manual_login_approval_enabled;
    this->_login_signal_output_file_descriptor = other._login_signal_output_file_descriptor;
    this->_login_signal_token_timeout_seconds = other._login_signal_token_timeout_seconds;
    this->_login_session_timeout_seconds = other._login_session_timeout_seconds;
    return ;
}

int32_t application_settings::initialize_from_move(application_settings &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "application_settings::initialize(move)",
            "source is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);

    constexpr ft_string application_settings::* STRING_FIELDS[] = {
        &application_settings::_database_root_path,
        &application_settings::_database_relative_path,
        &application_settings::_encryption_key,
        &application_settings::_encryption_algorithm_name
    };
    ft_size_t index = 0;
    while (index < sizeof(STRING_FIELDS) / sizeof(*STRING_FIELDS))
    {
        const int32_t result = (this->*STRING_FIELDS[index]).move(other.*STRING_FIELDS[index]);
        if (result != FT_ERR_SUCCESS)
        {
            while (index > 0)
            {
                --index;
                (void)(other.*STRING_FIELDS[index]).move(this->*STRING_FIELDS[index]);
            }
            this->destroy_partial_copy_state();
            return (result);
        }
        ++index;
    }
    this->copy_scalar_settings(other);
    const int32_t source_destroy_error = other.destroy();
    if (source_destroy_error != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (source_destroy_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::initialize() noexcept
{
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "application_settings::initialize", "initialize called on initialised instance");
    error_code = this->_database_root_path.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->_database_relative_path.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_database_root_path.destroy();
        return (error_code);
    }
    error_code = this->_encryption_key.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_database_root_path.destroy();
        (void)this->_database_relative_path.destroy();
        return (error_code);
    }
    error_code = this->_encryption_algorithm_name.initialize("aes-128-ecb-base64");
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_database_root_path.destroy();
        (void)this->_database_relative_path.destroy();
        (void)this->_encryption_key.destroy();
        return (error_code);
    }
    this->_encryption_enabled = FT_FALSE;
    this->_manual_login_approval_enabled = FT_FALSE;
    this->_login_signal_output_file_descriptor = STDOUT_FILENO;
    this->_login_signal_token_timeout_seconds = DEFAULT_LOGIN_SIGNAL_TOKEN_TIMEOUT_SECONDS;
    this->_login_session_timeout_seconds = DEFAULT_LOGIN_SESSION_TIMEOUT_SECONDS;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::initialize(const char *database_root_path, const char *database_relative_path,
    const char *encryption_key, ft_bool enable_encryption, const char *encryption_algorithm_name) noexcept
{
    int32_t error_code;

    error_code = this->initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->set_database_location(database_root_path, database_relative_path);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (error_code);
    }
    error_code = this->set_encryption_key(encryption_key);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (error_code);
    }
    error_code = this->set_encryption_algorithm_name(encryption_algorithm_name);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (error_code);
    }
    error_code = this->set_encryption_enabled(enable_encryption);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::initialize(const application_settings &other) noexcept
{
    return (this->initialize_from_copy(other));
}

int32_t application_settings::initialize(application_settings &&other) noexcept
{
    return (this->initialize_from_move(other));
}

int32_t application_settings::move(application_settings &other) noexcept
{
    return (this->initialize_from_move(other));
}

int32_t application_settings::set_database_location(const char *database_root_path, const char *database_relative_path) noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::set_database_location");
    if (database_root_path == ft_nullptr || database_root_path[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    if (database_relative_path == ft_nullptr || database_relative_path[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string root_path;
    ft_string relative_path;
    error_code = root_path.initialize(database_root_path);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = relative_path.initialize(database_relative_path);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->_database_root_path.move(root_path);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->_database_relative_path.move(relative_path));
}

int32_t application_settings::get_database_root_path(ft_string &database_root_path) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::get_database_root_path");
    error_code = database_root_path.initialize(this->_database_root_path);
    return (error_code);
}

int32_t application_settings::get_database_relative_path(ft_string &database_relative_path) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::get_database_relative_path");
    error_code = database_relative_path.initialize(this->_database_relative_path);
    return (error_code);
}

int32_t application_settings::set_encryption_key(const char *encryption_key) noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::set_encryption_key");
    if (encryption_key == ft_nullptr)
        encryption_key = "";
    (void)this->_encryption_key.destroy();
    error_code = this->_encryption_key.initialize(encryption_key);
    return (error_code);
}

int32_t application_settings::get_encryption_key(ft_string &encryption_key) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::get_encryption_key");
    error_code = encryption_key.initialize(this->_encryption_key);
    return (error_code);
}

int32_t application_settings::set_encryption_algorithm_name(const char *encryption_algorithm_name) noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::set_encryption_algorithm_name");
    if (encryption_algorithm_name == ft_nullptr || encryption_algorithm_name[0] == '\0')
        encryption_algorithm_name = "aes-128-ecb-base64";
    (void)this->_encryption_algorithm_name.destroy();
    error_code = this->_encryption_algorithm_name.initialize(encryption_algorithm_name);
    return (error_code);
}

int32_t application_settings::get_encryption_algorithm_name(ft_string &encryption_algorithm_name) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::get_encryption_algorithm_name");
    error_code = encryption_algorithm_name.initialize(this->_encryption_algorithm_name);
    return (error_code);
}

int32_t application_settings::set_encryption_enabled(ft_bool enabled) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::set_encryption_enabled");
    this->_encryption_enabled = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::is_encryption_enabled(ft_bool &enabled) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::is_encryption_enabled");
    enabled = this->_encryption_enabled;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::set_manual_login_approval_enabled(ft_bool enabled) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::set_manual_login_approval_enabled");
    this->_manual_login_approval_enabled = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::is_manual_login_approval_enabled(ft_bool &enabled) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::is_manual_login_approval_enabled");
    enabled = this->_manual_login_approval_enabled;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::set_login_signal_output_file_descriptor(int32_t file_descriptor) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::set_login_signal_output_file_descriptor");
    this->_login_signal_output_file_descriptor = file_descriptor;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::get_login_signal_output_file_descriptor(int32_t &file_descriptor) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::get_login_signal_output_file_descriptor");
    file_descriptor = this->_login_signal_output_file_descriptor;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::set_login_signal_token_timeout_seconds(int64_t timeout_seconds) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::set_login_signal_token_timeout_seconds");
    if (timeout_seconds <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_login_signal_token_timeout_seconds = timeout_seconds;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::get_login_signal_token_timeout_seconds(int64_t &timeout_seconds) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::get_login_signal_token_timeout_seconds");
    timeout_seconds = this->_login_signal_token_timeout_seconds;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::set_login_session_timeout_seconds(int64_t timeout_seconds) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::set_login_session_timeout_seconds");
    if (timeout_seconds <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_login_session_timeout_seconds = timeout_seconds;
    return (FT_ERR_SUCCESS);
}

int32_t application_settings::get_login_session_timeout_seconds(int64_t &timeout_seconds) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_settings::get_login_session_timeout_seconds");
    timeout_seconds = this->_login_session_timeout_seconds;
    return (FT_ERR_SUCCESS);
}
