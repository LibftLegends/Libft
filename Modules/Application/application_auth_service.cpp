#include "application_auth_service.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../CMA/CMA.hpp"
#include "../Encoding/encoding.hpp"
#include "../Encryption/encryption.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Errno/errno.hpp"
#include "../File/file_utils.hpp"
#include "../Filesystem/filesystem.hpp"
#include "../Printf/printf.hpp"
#include "../Storage/kv_store.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Time/time.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Template/move.hpp"
#include "../Template/vector.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"

static const char *application_auth_user_key_prefix = "application/auth/users/";
static const char *application_auth_login_approval_key_prefix = "application/auth/approvals/";
static const char *application_auth_login_approval_requirement_key_prefix = "application/auth/settings/approval_required/";
static const char *application_auth_manual_login_approval_setting_key = "application/auth/settings/manual_login_approval";
static const char *application_auth_login_signal_one_time_password_key_prefix = "application/auth/login_signal/";
static const char *application_auth_login_session_key_prefix = "application/auth/sessions/";
static const char *application_auth_failed_login_count_key_prefix = "application/auth/audit/failed_count/";
static const char *application_auth_consecutive_failed_login_count_key_prefix = "application/auth/audit/consecutive_failed_count/";
static const char *application_auth_last_failed_login_ip_key_prefix = "application/auth/audit/last_failed_ip/";
static const char *application_auth_last_failed_login_timestamp_key_prefix = "application/auth/audit/last_failed_timestamp/";
static const char *application_auth_last_successful_login_ip_key_prefix = "application/auth/audit/last_successful_ip/";
static const char *application_auth_last_successful_login_timestamp_key_prefix = "application/auth/audit/last_successful_timestamp/";
static const ft_size_t application_auth_salt_length = 16U;
static const ft_size_t application_auth_digest_length = 32U;
static const ft_size_t application_auth_login_signal_one_time_password_length = 16U;

static int32_t application_auth_encode_hex(const uint8_t *buffer, ft_size_t buffer_size, ft_string &output)
{
    char *encoded_buffer;
    int32_t error_code;

    encoded_buffer = encoding_hex_encode(buffer, buffer_size, FT_FALSE);
    if (encoded_buffer == ft_nullptr)
        return (encoding_get_error());
    (void)output.destroy();
    error_code = output.initialize(encoded_buffer);
    cma_free(encoded_buffer);
    return (error_code);
}

static int32_t application_auth_build_database_path(const char *database_root_path, const char *database_relative_path, ft_string &database_path)
{
    ft_string *joined_path;
    int32_t error_code;

    joined_path = filesystem_safe_join_path(database_root_path, database_relative_path);
    if (joined_path == ft_nullptr)
        return (FT_ERR_INVALID_PATH);
    error_code = file_validate_path_resolution_inside_root(
            database_root_path, joined_path->c_str());
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)joined_path->destroy();
        delete joined_path;
        return (error_code);
    }
    (void)database_path.destroy();
    error_code = database_path.initialize(joined_path->c_str());
    (void)joined_path->destroy();
    delete joined_path;
    return (error_code);
}

static int32_t application_auth_write_integer_value(int64_t value, ft_string &output)
{
    char value_buffer[64];
    int32_t written_length;

    written_length = pf_snprintf(value_buffer, sizeof(value_buffer), FT_INT64_DECIMAL_FORMAT, value);
    if (written_length < 0 || written_length >= static_cast<int32_t>(sizeof(value_buffer)))
        return (FT_ERR_INVALID_OPERATION);
    (void)output.destroy();
    return (output.initialize(value_buffer));
}

static int32_t application_auth_parse_integer_value(const char *value_string, int64_t &value)
{
    char *end_pointer;
    int64_t parsed_value;

    if (value_string == ft_nullptr || value_string[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    parsed_value = ft_strtol(value_string, &end_pointer, 10);
    if (end_pointer == value_string || *end_pointer != '\0')
        return (FT_ERR_CONFIGURATION);
    value = parsed_value;
    return (FT_ERR_SUCCESS);
}

static int32_t application_auth_write_descriptor(int32_t file_descriptor, const char *buffer, ft_size_t length)
{
    int64_t write_result;
    ft_size_t written_length;

    written_length = 0;
    while (written_length < length)
    {
        write_result = su_write(file_descriptor, buffer + written_length, length - written_length);
        if (write_result < 0)
            return (FT_ERR_IO);
        if (write_result == 0)
            return (FT_ERR_IO);
        if (write_result > static_cast<int64_t>(length - written_length))
            return (FT_ERR_IO);
        written_length += static_cast<ft_size_t>(write_result);
    }
    return (FT_ERR_SUCCESS);
}

void application_auth_service::destroy_partial_copy_state() noexcept
{
    (void)this->_credential_store.destroy();
    (void)this->_settings.destroy();
    (void)this->_database_path.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return ;
}

application_auth_service::application_auth_service() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED)
    , _credential_store()
    , _settings()
    , _database_path()
{
    return ;
}

application_auth_service::~application_auth_service() noexcept
{
    (void)this->destroy();
    return ;
}

ft_bool application_auth_service::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t application_auth_service::destroy() noexcept
{
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    error_code = this->_credential_store.destroy();
    if (this->_settings.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_INVALID_OPERATION;
    if (this->_database_path.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_INVALID_OPERATION;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (error_code);
}

int32_t application_auth_service::initialize(const application_settings &settings) noexcept
{
    ft_string database_root_path;
    ft_string database_relative_path;
    ft_string encryption_key;
    ft_string encryption_algorithm_name;
    int32_t error_code;
    ft_bool encryption_enabled;
    ft_bool manual_login_approval_enabled;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "application_auth_service::initialize", "initialize called on initialised instance");
    if (settings.is_initialised() != FT_TRUE)
        errno_abort_lifecycle(FT_CLASS_STATE_UNINITIALISED, "application_auth_service::initialize(settings)", "source settings are uninitialised");
    error_code = settings.get_database_root_path(database_root_path);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = settings.get_database_relative_path(database_relative_path);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = settings.get_encryption_key(encryption_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = settings.get_encryption_algorithm_name(encryption_algorithm_name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = settings.is_encryption_enabled(encryption_enabled);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = application_auth_build_database_path(database_root_path.c_str(), database_relative_path.c_str(), this->_database_path);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    error_code = this->_credential_store.initialize(this->_database_path.c_str(),
        encryption_key.c_str(), encryption_enabled, encryption_algorithm_name.c_str());
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    error_code = this->_settings.initialize(settings);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    error_code = this->load_manual_login_approval_enabled(manual_login_approval_enabled);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    error_code = this->_settings.set_manual_login_approval_enabled(manual_login_approval_enabled);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::initialize(const char *database_root_path, const char *database_relative_path,
    const char *encryption_key, ft_bool enable_encryption, const char *encryption_algorithm_name) noexcept
{
    application_settings settings;
    int32_t error_code;

    error_code = settings.initialize(database_root_path, database_relative_path, encryption_key,
        enable_encryption, encryption_algorithm_name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->initialize(settings));
}

int32_t application_auth_service::initialize_from_copy(const application_auth_service &other) noexcept
{
    int32_t error_code;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "application_auth_service::initialize(copy)", "source is uninitialised");
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
    error_code = this->_settings.initialize(other._settings);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (error_code);
    }
    error_code = this->_database_path.initialize(other._database_path);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (error_code);
    }
    error_code = this->_credential_store.initialize(other._credential_store);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::initialize_from_move(application_auth_service &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize_from_copy(other);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = other.destroy();
    if (error_code != FT_ERR_SUCCESS)
    {
        this->destroy_partial_copy_state();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::initialize(const application_auth_service &other) noexcept
{
    return (this->initialize_from_copy(other));
}

int32_t application_auth_service::initialize(application_auth_service &&other) noexcept
{
    return (this->initialize_from_move(other));
}

int32_t application_auth_service::move(application_auth_service &other) noexcept
{
    return (this->initialize_from_move(other));
}

int32_t application_auth_service::build_user_key(const char *username, ft_string &key_output) const noexcept
{
    int32_t error_code;

    if (username == ft_nullptr || username[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    (void)key_output.destroy();
    error_code = key_output.initialize(application_auth_user_key_prefix);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = key_output.append(username);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)key_output.destroy();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::build_login_approval_key(const char *username, ft_string &key_output) const noexcept
{
    int32_t error_code;

    if (username == ft_nullptr || username[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    (void)key_output.destroy();
    error_code = key_output.initialize(application_auth_login_approval_key_prefix);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = key_output.append(username);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)key_output.destroy();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::build_user_approval_requirement_key(const char *username, ft_string &key_output) const noexcept
{
    int32_t error_code;

    if (username == ft_nullptr || username[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    (void)key_output.destroy();
    error_code = key_output.initialize(application_auth_login_approval_requirement_key_prefix);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = key_output.append(username);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)key_output.destroy();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::build_manual_login_approval_setting_key(ft_string &key_output) const noexcept
{
    int32_t error_code;

    (void)key_output.destroy();
    error_code = key_output.initialize(application_auth_manual_login_approval_setting_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::build_login_signal_one_time_password_key(const char *username, ft_string &key_output) const noexcept
{
    int32_t error_code;

    if (username == ft_nullptr || username[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    (void)key_output.destroy();
    error_code = key_output.initialize(application_auth_login_signal_one_time_password_key_prefix);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = key_output.append(username);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)key_output.destroy();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::build_login_session_key(const char *username, ft_string &key_output) const noexcept
{
    return (this->build_user_stat_key(application_auth_login_session_key_prefix, username, key_output));
}

int32_t application_auth_service::build_user_stat_key(const char *key_prefix, const char *username, ft_string &key_output) const noexcept
{
    int32_t error_code;

    if (key_prefix == ft_nullptr || key_prefix[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    if (username == ft_nullptr || username[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    (void)key_output.destroy();
    error_code = key_output.initialize(key_prefix);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = key_output.append(username);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)key_output.destroy();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::store_integer_value(const char *key, int64_t value) noexcept
{
    ft_string value_string;
    int32_t error_code;

    error_code = application_auth_write_integer_value(value, value_string);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->_credential_store.kv_set(key, value_string.c_str()));
}

int32_t application_auth_service::load_integer_value(const char *key, int64_t &value, ft_bool *found) const noexcept
{
    const char *value_string;
    int32_t error_code;

    value_string = this->_credential_store.kv_get(key);
    if (value_string == ft_nullptr)
    {
        if (found != ft_nullptr)
            *found = FT_FALSE;
        value = 0;
        return (FT_ERR_SUCCESS);
    }
    if (found != ft_nullptr)
        *found = FT_TRUE;
    error_code = application_auth_parse_integer_value(value_string, value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::store_string_value(const char *key, const char *value) noexcept
{
    if (value == ft_nullptr)
        value = "";
    return (this->_credential_store.kv_set(key, value));
}

int32_t application_auth_service::load_string_value(const char *key, ft_string &value, ft_bool *found) const noexcept
{
    const char *value_string;
    int32_t error_code;

    value_string = this->_credential_store.kv_get(key);
    if (value_string == ft_nullptr)
    {
        if (found != ft_nullptr)
            *found = FT_FALSE;
        (void)value.destroy();
        return (FT_ERR_SUCCESS);
    }
    if (found != ft_nullptr)
        *found = FT_TRUE;
    (void)value.destroy();
    error_code = value.initialize(value_string);
    return (error_code);
}

int32_t application_auth_service::generate_salt_hex(ft_string &salt_hex) const noexcept
{
    uint8_t salt_buffer[application_auth_salt_length];
    int32_t error_code;

    error_code = encryption_fill_secure_buffer(salt_buffer, application_auth_salt_length);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (application_auth_encode_hex(salt_buffer, application_auth_salt_length, salt_hex));
}

int32_t application_auth_service::generate_login_signal_one_time_password(ft_string &one_time_password_output) const noexcept
{
    uint8_t password_buffer[application_auth_login_signal_one_time_password_length];
    int32_t error_code;

    error_code = encryption_fill_secure_buffer(password_buffer, application_auth_login_signal_one_time_password_length);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (application_auth_encode_hex(password_buffer, application_auth_login_signal_one_time_password_length, one_time_password_output));
}

int32_t application_auth_service::generate_login_session_token(ft_string &session_token_output) const noexcept
{
    uint8_t token_buffer[application_auth_login_signal_one_time_password_length];
    int32_t error_code;

    error_code = encryption_fill_secure_buffer(token_buffer, application_auth_login_signal_one_time_password_length);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (application_auth_encode_hex(token_buffer, application_auth_login_signal_one_time_password_length, session_token_output));
}

int32_t application_auth_service::build_login_signal_one_time_password_hash(const char *one_time_password, ft_string &hash_hex) const noexcept
{
    uint8_t digest_buffer[application_auth_digest_length];

    if (one_time_password == ft_nullptr || one_time_password[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    sha256_hash(one_time_password, ft_strlen(one_time_password), digest_buffer);
    return (application_auth_encode_hex(digest_buffer, application_auth_digest_length, hash_hex));
}

int32_t application_auth_service::build_login_session_value(const char *session_token, const char *client_ip_address,
    int64_t login_timestamp, ft_string &value_output) const noexcept
{
    ft_string timestamp_string;
    int32_t error_code;

    if (session_token == ft_nullptr || session_token[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = application_auth_write_integer_value(login_timestamp, timestamp_string);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    (void)value_output.destroy();
    error_code = value_output.initialize(session_token);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = value_output.append('|');
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)value_output.destroy();
        return (error_code);
    }
    if (client_ip_address != ft_nullptr && client_ip_address[0] != '\0')
        error_code = value_output.append(client_ip_address);
    else
        error_code = FT_ERR_SUCCESS;
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)value_output.destroy();
        return (error_code);
    }
    error_code = value_output.append('|');
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)value_output.destroy();
        return (error_code);
    }
    error_code = value_output.append(timestamp_string);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)value_output.destroy();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::parse_login_session_value(const char *value, ft_string &session_token_output,
    ft_string &client_ip_address_output, int64_t &login_timestamp_output) const noexcept
{
    ft_string session_value;
    ft_string *session_token;
    ft_string *client_ip_address;
    ft_string *timestamp_string;
    const char *first_separator;
    const char *second_separator;
    int32_t error_code;

    if (value == ft_nullptr || value[0] == '\0')
        return (FT_ERR_CONFIGURATION);
    error_code = session_value.initialize(value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    first_separator = ft_strchr(session_value.c_str(), '|');
    if (first_separator == ft_nullptr)
    {
        (void)session_value.destroy();
        return (FT_ERR_CONFIGURATION);
    }
    second_separator = ft_strchr(first_separator + 1, '|');
    if (second_separator == ft_nullptr || second_separator <= first_separator)
    {
        (void)session_value.destroy();
        return (FT_ERR_CONFIGURATION);
    }
    session_token = session_value.substr(0, static_cast<ft_size_t>(first_separator - session_value.c_str()));
    if (session_token == ft_nullptr)
    {
        (void)session_value.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    client_ip_address = session_value.substr(static_cast<ft_size_t>(first_separator - session_value.c_str()) + 1,
        static_cast<ft_size_t>(second_separator - first_separator - 1));
    if (client_ip_address == ft_nullptr)
    {
        (void)session_token->destroy();
        delete session_token;
        (void)session_value.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    timestamp_string = session_value.substr(static_cast<ft_size_t>(second_separator - session_value.c_str()) + 1);
    if (timestamp_string == ft_nullptr)
    {
        (void)session_token->destroy();
        delete session_token;
        (void)client_ip_address->destroy();
        delete client_ip_address;
        (void)session_value.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    (void)session_token_output.destroy();
    error_code = session_token_output.initialize(*session_token);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)session_token->destroy();
        delete session_token;
        (void)client_ip_address->destroy();
        delete client_ip_address;
        (void)timestamp_string->destroy();
        delete timestamp_string;
        (void)session_value.destroy();
        return (error_code);
    }
    (void)client_ip_address_output.destroy();
    error_code = client_ip_address_output.initialize(*client_ip_address);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)session_token->destroy();
        delete session_token;
        (void)client_ip_address->destroy();
        delete client_ip_address;
        (void)timestamp_string->destroy();
        delete timestamp_string;
        (void)session_value.destroy();
        return (error_code);
    }
    error_code = application_auth_parse_integer_value(timestamp_string->c_str(), login_timestamp_output);
    (void)session_token->destroy();
    delete session_token;
    (void)client_ip_address->destroy();
    delete client_ip_address;
    (void)timestamp_string->destroy();
    delete timestamp_string;
    (void)session_value.destroy();
    return (error_code);
}

int32_t application_auth_service::is_login_access_allowed(const char *username) const noexcept
{
    ft_bool manual_login_approval_enabled;
    ft_bool approval_required;
    ft_bool approved;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::is_login_access_allowed");
    error_code = this->_settings.is_manual_login_approval_enabled(manual_login_approval_enabled);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (manual_login_approval_enabled == FT_TRUE)
    {
        approved = FT_FALSE;
        error_code = this->is_login_approved(username, approved);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (approved == FT_FALSE)
            return (FT_ERR_PERMISSION_DENIED);
        return (FT_ERR_SUCCESS);
    }
    approval_required = FT_FALSE;
    error_code = this->load_user_login_approval_required(username, approval_required);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (approval_required == FT_TRUE)
    {
        approved = FT_FALSE;
        error_code = this->is_login_approved(username, approved);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (approved == FT_FALSE)
            return (FT_ERR_PERMISSION_DENIED);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::write_login_signal_one_time_password(const char *username, const char *one_time_password) const noexcept
{
    ft_string message;
    int32_t error_code;

    if (username == ft_nullptr || one_time_password == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = message.initialize(username);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = message.append(": ");
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)message.destroy();
        return (error_code);
    }
    error_code = message.append(one_time_password);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)message.destroy();
        return (error_code);
    }
    error_code = message.append('\n');
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)message.destroy();
        return (error_code);
    }
    {
        int32_t login_signal_output_file_descriptor;

        error_code = this->_settings.get_login_signal_output_file_descriptor(login_signal_output_file_descriptor);
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)message.destroy();
            return (error_code);
        }
        error_code = application_auth_write_descriptor(login_signal_output_file_descriptor,
            message.c_str(), message.size());
    }
    (void)message.destroy();
    return (error_code);
}

int32_t application_auth_service::build_password_hash(const char *password, const ft_string &salt_hex, ft_string &hash_hex) const noexcept
{
    ft_string hash_input;
    uint8_t digest_buffer[application_auth_digest_length];
    int32_t error_code;

    if (password == ft_nullptr || password[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = hash_input.initialize(salt_hex);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = hash_input.append(':');
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = hash_input.append(password);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    sha256_hash(hash_input.c_str(), hash_input.size(), digest_buffer);
    return (application_auth_encode_hex(digest_buffer, application_auth_digest_length, hash_hex));
}

int32_t application_auth_service::build_credential_record(const char *password, ft_string &record_output) const noexcept
{
    ft_string salt_hex;
    ft_string hash_hex;
    int32_t error_code;

    error_code = this->generate_salt_hex(salt_hex);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_password_hash(password, salt_hex, hash_hex);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    (void)record_output.destroy();
    error_code = record_output.initialize(salt_hex);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = record_output.append('|');
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = record_output.append(hash_hex);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)record_output.destroy();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::parse_credential_record(const char *record_string, ft_string &salt_hex, ft_string &hash_hex) const noexcept
{
    ft_size_t delimiter_index;
    int32_t error_code;

    if (record_string == ft_nullptr || record_string[0] == '\0')
        return (FT_ERR_CONFIGURATION);
    delimiter_index = 0;
    while (record_string[delimiter_index] != '\0' && record_string[delimiter_index] != '|')
        delimiter_index++;
    if (delimiter_index == 0 || record_string[delimiter_index] != '|')
        return (FT_ERR_CONFIGURATION);
    (void)salt_hex.destroy();
    error_code = salt_hex.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = salt_hex.append(record_string, delimiter_index);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    (void)hash_hex.destroy();
    error_code = hash_hex.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = hash_hex.append(record_string + delimiter_index + 1);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (hash_hex.empty() == FT_TRUE)
        return (FT_ERR_CONFIGURATION);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::load_manual_login_approval_enabled(ft_bool &enabled) const noexcept
{
    ft_string setting_key;
    const char *setting_value;
    int32_t error_code;

    error_code = this->build_manual_login_approval_setting_key(setting_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    setting_value = this->_credential_store.kv_get(setting_key.c_str());
    if (setting_value == ft_nullptr)
    {
        enabled = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    if (ft_strcmp(setting_value, "1") == FT_ERR_SUCCESS)
    {
        enabled = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }
    if (ft_strcmp(setting_value, "0") == FT_ERR_SUCCESS)
    {
        enabled = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_CONFIGURATION);
}

int32_t application_auth_service::load_user_login_approval_required(const char *username, ft_bool &required) const noexcept
{
    ft_string setting_key;
    const char *setting_value;
    int32_t error_code;

    error_code = this->build_user_approval_requirement_key(username, setting_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    setting_value = this->_credential_store.kv_get(setting_key.c_str());
    if (setting_value == ft_nullptr)
    {
        required = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    if (ft_strcmp(setting_value, "1") == FT_ERR_SUCCESS)
    {
        required = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }
    if (ft_strcmp(setting_value, "0") == FT_ERR_SUCCESS)
    {
        required = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_CONFIGURATION);
}

int32_t application_auth_service::get_session_username_from_key(const char *session_key, ft_string &username_output) const noexcept
{
    ft_size_t prefix_length;
    int32_t error_code;

    if (session_key == ft_nullptr || session_key[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    prefix_length = ft_strlen_size_t(application_auth_login_session_key_prefix);
    if (ft_strncmp(session_key, application_auth_login_session_key_prefix, prefix_length) != FT_ERR_SUCCESS)
        return (FT_ERR_CONFIGURATION);
    if (session_key[prefix_length] == '\0')
        return (FT_ERR_CONFIGURATION);
    (void)username_output.destroy();
    error_code = username_output.initialize(session_key + prefix_length);
    return (error_code);
}

int32_t application_auth_service::load_user_session_value(const char *username, ft_string &session_value, ft_bool *found) const noexcept
{
    ft_string session_key;
    const char *stored_session_value;
    int32_t error_code;

    error_code = this->build_login_session_key(username, session_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    stored_session_value = this->_credential_store.kv_get(session_key.c_str());
    if (stored_session_value == ft_nullptr)
    {
        if (found != ft_nullptr)
            *found = FT_FALSE;
        (void)session_value.destroy();
        return (FT_ERR_SUCCESS);
    }
    if (found != ft_nullptr)
        *found = FT_TRUE;
    (void)session_value.destroy();
    error_code = session_value.initialize(stored_session_value);
    return (error_code);
}

int32_t application_auth_service::collect_logged_in_usernames(ft_vector<ft_string> &usernames) const noexcept
{
    ft_vector<kv_store_snapshot_entry> snapshot_entries;
    ft_size_t prefix_length;
    ft_size_t entry_index;
    int32_t error_code;

    error_code = snapshot_entries.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->_credential_store.export_snapshot(snapshot_entries);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    prefix_length = ft_strlen_size_t(application_auth_login_session_key_prefix);
    usernames.clear();
    entry_index = 0;
    while (entry_index < snapshot_entries.size())
    {
        const kv_store_snapshot_entry &snapshot_entry = snapshot_entries[entry_index];

        if (snapshot_entry.key.size() > prefix_length
            && ft_strncmp(snapshot_entry.key.c_str(), application_auth_login_session_key_prefix, prefix_length) == FT_ERR_SUCCESS)
        {
            ft_string username_output;
            error_code = username_output.initialize(snapshot_entry.key.c_str() + prefix_length);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            error_code = usernames.push_back(ft_move(username_output));
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        entry_index++;
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::record_login_failure(const char *username, const char *client_ip_address) noexcept
{
    ft_string failed_count_key;
    ft_string consecutive_failed_count_key;
    ft_string last_failed_ip_key;
    ft_string last_failed_timestamp_key;
    int64_t failed_count;
    int64_t consecutive_failed_count;
    int64_t current_time;
    int32_t error_code;

    error_code = this->build_user_stat_key(application_auth_failed_login_count_key_prefix, username, failed_count_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_consecutive_failed_login_count_key_prefix, username, consecutive_failed_count_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_last_failed_login_ip_key_prefix, username, last_failed_ip_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_last_failed_login_timestamp_key_prefix, username, last_failed_timestamp_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->load_integer_value(failed_count_key.c_str(), failed_count, ft_nullptr);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->load_integer_value(consecutive_failed_count_key.c_str(), consecutive_failed_count, ft_nullptr);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    failed_count++;
    consecutive_failed_count++;
    error_code = this->store_integer_value(failed_count_key.c_str(), failed_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->store_integer_value(consecutive_failed_count_key.c_str(), consecutive_failed_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->store_string_value(last_failed_ip_key.c_str(), client_ip_address);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    current_time = time_now();
    if (current_time < 0)
        return (FT_ERR_IO);
    return (this->store_integer_value(last_failed_timestamp_key.c_str(), current_time));
}

int32_t application_auth_service::record_login_success(const char *username, const char *client_ip_address) noexcept
{
    ft_string consecutive_failed_count_key;
    ft_string last_successful_login_ip_key;
    ft_string last_successful_login_timestamp_key;
    int64_t current_time;
    int32_t error_code;

    error_code = this->build_user_stat_key(application_auth_consecutive_failed_login_count_key_prefix, username, consecutive_failed_count_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_last_successful_login_ip_key_prefix, username, last_successful_login_ip_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_last_successful_login_timestamp_key_prefix, username, last_successful_login_timestamp_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->store_integer_value(consecutive_failed_count_key.c_str(), 0);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->store_string_value(last_successful_login_ip_key.c_str(), client_ip_address);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    current_time = time_now();
    if (current_time < 0)
        return (FT_ERR_IO);
    return (this->store_integer_value(last_successful_login_timestamp_key.c_str(), current_time));
}

int32_t application_auth_service::register_user(const char *username, const char *password) noexcept
{
    ft_string user_key;
    ft_string credential_record;
    const char *existing_record;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::register_user");
    error_code = this->build_user_key(username, user_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    existing_record = this->_credential_store.kv_get(user_key.c_str());
    if (existing_record != ft_nullptr)
        return (FT_ERR_ALREADY_EXISTS);
    error_code = this->build_credential_record(password, credential_record);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->_credential_store.kv_set(user_key.c_str(), credential_record.c_str()));
}

int32_t application_auth_service::authenticate_user(const char *username, const char *password, ft_bool &authenticated) const noexcept
{
    ft_string user_key;
    ft_string salt_hex;
    ft_string stored_hash_hex;
    ft_string computed_hash_hex;
    const char *record_string;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::authenticate_user");
    authenticated = FT_FALSE;
    error_code = this->build_user_key(username, user_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    record_string = this->_credential_store.kv_get(user_key.c_str());
    if (record_string == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    error_code = this->parse_credential_record(record_string, salt_hex, stored_hash_hex);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_password_hash(password, salt_hex, computed_hash_hex);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (ft_strcmp(stored_hash_hex.c_str(), computed_hash_hex.c_str()) != 0)
        return (FT_ERR_PERMISSION_DENIED);
    error_code = this->is_login_access_allowed(username);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    authenticated = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::user_exists(const char *username, ft_bool &exists) const noexcept
{
    ft_string user_key;
    const char *record_string;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::user_exists");
    error_code = this->build_user_key(username, user_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    record_string = this->_credential_store.kv_get(user_key.c_str());
    if (record_string != ft_nullptr)
        exists = FT_TRUE;
    else
        exists = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::remove_user(const char *username) noexcept
{
    ft_string user_key;
    ft_string approval_key;
    ft_string approval_requirement_key;
    ft_string login_signal_key;
    ft_string login_session_key;
    ft_string failed_count_key;
    ft_string consecutive_failed_count_key;
    ft_string last_failed_ip_key;
    ft_string last_failed_timestamp_key;
    ft_string last_successful_login_ip_key;
    ft_string last_successful_login_timestamp_key;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::remove_user");
    error_code = this->build_user_key(username, user_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_login_approval_key(username, approval_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_approval_requirement_key(username, approval_requirement_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_login_signal_one_time_password_key(username, login_signal_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_login_session_key(username, login_session_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_failed_login_count_key_prefix, username, failed_count_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_consecutive_failed_login_count_key_prefix, username, consecutive_failed_count_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_last_failed_login_ip_key_prefix, username, last_failed_ip_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_last_failed_login_timestamp_key_prefix, username, last_failed_timestamp_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_last_successful_login_ip_key_prefix, username, last_successful_login_ip_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_user_stat_key(application_auth_last_successful_login_timestamp_key_prefix, username, last_successful_login_timestamp_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    (void)this->_credential_store.kv_delete(approval_key.c_str());
    (void)this->_credential_store.kv_delete(approval_requirement_key.c_str());
    (void)this->_credential_store.kv_delete(login_signal_key.c_str());
    (void)this->_credential_store.kv_delete(login_session_key.c_str());
    (void)this->_credential_store.kv_delete(failed_count_key.c_str());
    (void)this->_credential_store.kv_delete(consecutive_failed_count_key.c_str());
    (void)this->_credential_store.kv_delete(last_failed_ip_key.c_str());
    (void)this->_credential_store.kv_delete(last_failed_timestamp_key.c_str());
    (void)this->_credential_store.kv_delete(last_successful_login_ip_key.c_str());
    (void)this->_credential_store.kv_delete(last_successful_login_timestamp_key.c_str());
    return (this->_credential_store.kv_delete(user_key.c_str()));
}

int32_t application_auth_service::set_manual_login_approval_enabled(ft_bool enabled) noexcept
{
    ft_string setting_key;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::set_manual_login_approval_enabled");
    error_code = this->build_manual_login_approval_setting_key(setting_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (enabled == FT_TRUE)
        error_code = this->_credential_store.kv_set(setting_key.c_str(), "1");
    else
        error_code = this->_credential_store.kv_set(setting_key.c_str(), "0");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->_settings.set_manual_login_approval_enabled(enabled);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::is_manual_login_approval_enabled(ft_bool &enabled) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::is_manual_login_approval_enabled");
    return (this->_settings.is_manual_login_approval_enabled(enabled));
}

int32_t application_auth_service::set_login_approval_required_for_user(const char *username, ft_bool enabled) noexcept
{
    ft_string user_key;
    ft_string approval_requirement_key;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::set_login_approval_required_for_user");
    error_code = this->build_user_key(username, user_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (this->_credential_store.kv_get(user_key.c_str()) == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    error_code = this->build_user_approval_requirement_key(username, approval_requirement_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (enabled == FT_TRUE)
        error_code = this->_credential_store.kv_set(approval_requirement_key.c_str(), "1");
    else
        error_code = this->_credential_store.kv_delete(approval_requirement_key.c_str());
    if (error_code == FT_ERR_NOT_FOUND)
        return (FT_ERR_SUCCESS);
    return (error_code);
}

int32_t application_auth_service::is_login_approval_required_for_user(const char *username, ft_bool &enabled) const noexcept
{
    ft_string approval_requirement_key;
    const char *approval_requirement_value;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::is_login_approval_required_for_user");
    error_code = this->build_user_approval_requirement_key(username, approval_requirement_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    approval_requirement_value = this->_credential_store.kv_get(approval_requirement_key.c_str());
    if (approval_requirement_value == ft_nullptr)
    {
        enabled = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    if (ft_strcmp(approval_requirement_value, "1") == FT_ERR_SUCCESS)
    {
        enabled = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }
    if (ft_strcmp(approval_requirement_value, "0") == FT_ERR_SUCCESS)
    {
        enabled = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_CONFIGURATION);
}

int32_t application_auth_service::approve_login(const char *username) noexcept
{
    ft_string user_key;
    ft_string approval_key;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::approve_login");
    if (username == ft_nullptr || username[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->build_user_key(username, user_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (this->_credential_store.kv_get(user_key.c_str()) == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    error_code = this->build_login_approval_key(username, approval_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->_credential_store.kv_set(approval_key.c_str(), "1"));
}

int32_t application_auth_service::revoke_login_approval(const char *username) noexcept
{
    ft_string approval_key;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::revoke_login_approval");
    error_code = this->build_login_approval_key(username, approval_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->_credential_store.kv_delete(approval_key.c_str());
    if (error_code == FT_ERR_NOT_FOUND)
        return (FT_ERR_SUCCESS);
    return (error_code);
}

int32_t application_auth_service::is_login_approved(const char *username, ft_bool &approved) const noexcept
{
    ft_string approval_key;
    const char *approval_value;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::is_login_approved");
    error_code = this->build_login_approval_key(username, approval_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    approval_value = this->_credential_store.kv_get(approval_key.c_str());
    if (approval_value == ft_nullptr)
    {
        approved = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    approved = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::set_login_signal_output_file_descriptor(int32_t file_descriptor) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::set_login_signal_output_file_descriptor");
    return (this->_settings.set_login_signal_output_file_descriptor(file_descriptor));
}

int32_t application_auth_service::get_login_signal_output_file_descriptor(int32_t &file_descriptor) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_login_signal_output_file_descriptor");
    return (this->_settings.get_login_signal_output_file_descriptor(file_descriptor));
}

int32_t application_auth_service::set_login_signal_token_timeout_seconds(int64_t timeout_seconds) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::set_login_signal_token_timeout_seconds");
    return (this->_settings.set_login_signal_token_timeout_seconds(timeout_seconds));
}

int32_t application_auth_service::get_login_signal_token_timeout_seconds(int64_t &timeout_seconds) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_login_signal_token_timeout_seconds");
    return (this->_settings.get_login_signal_token_timeout_seconds(timeout_seconds));
}

int32_t application_auth_service::set_login_session_timeout_seconds(int64_t timeout_seconds) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::set_login_session_timeout_seconds");
    return (this->_settings.set_login_session_timeout_seconds(timeout_seconds));
}

int32_t application_auth_service::get_login_session_timeout_seconds(int64_t &timeout_seconds) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_login_session_timeout_seconds");
    return (this->_settings.get_login_session_timeout_seconds(timeout_seconds));
}

int32_t application_auth_service::begin_user_session(const char *username, const char *client_ip_address, ft_string &session_token_output) noexcept
{
    ft_string user_key;
    ft_string session_key;
    ft_string session_token;
    ft_string session_value;
    ft_bool user_exists;
    int64_t session_timeout_seconds;
    int64_t current_time;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::begin_user_session");
    error_code = this->build_user_key(username, user_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    user_exists = FT_FALSE;
    if (this->_credential_store.kv_get(user_key.c_str()) != ft_nullptr)
        user_exists = FT_TRUE;
    if (user_exists == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    error_code = this->generate_login_session_token(session_token);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    (void)session_token_output.destroy();
    error_code = session_token_output.initialize(session_token);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    current_time = time_now();
    if (current_time < 0)
    {
        (void)session_token_output.destroy();
        return (FT_ERR_IO);
    }
    error_code = this->build_login_session_key(username, session_key);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)session_token_output.destroy();
        return (error_code);
    }
    error_code = this->build_login_session_value(session_token_output.c_str(), client_ip_address, current_time, session_value);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)session_token_output.destroy();
        return (error_code);
    }
    error_code = this->_settings.get_login_session_timeout_seconds(session_timeout_seconds);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)session_token_output.destroy();
        return (error_code);
    }
    error_code = this->_credential_store.kv_set(session_key.c_str(), session_value.c_str(), session_timeout_seconds);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)session_token_output.destroy();
        return (error_code);
    }
    error_code = this->record_login_success(username, client_ip_address);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_credential_store.kv_delete(session_key.c_str());
        (void)session_token_output.destroy();
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::end_user_session(const char *username) noexcept
{
    ft_string session_key;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::end_user_session");
    error_code = this->build_login_session_key(username, session_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->_credential_store.kv_delete(session_key.c_str());
    if (error_code == FT_ERR_NOT_FOUND)
        return (FT_ERR_SUCCESS);
    return (error_code);
}

int32_t application_auth_service::is_user_logged_in(const char *username, ft_bool &logged_in) const noexcept
{
    ft_string session_key;
    const char *session_value;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::is_user_logged_in");
    error_code = this->build_login_session_key(username, session_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    session_value = this->_credential_store.kv_get(session_key.c_str());
    if (session_value == ft_nullptr)
        logged_in = FT_FALSE;
    else
        logged_in = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::get_logged_in_usernames(ft_vector<ft_string> &usernames) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_logged_in_usernames");
    return (this->collect_logged_in_usernames(usernames));
}

int32_t application_auth_service::get_failed_login_count(const char *username, int64_t &failed_login_count) const noexcept
{
    ft_string failed_login_count_key;
    ft_bool found;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_failed_login_count");
    error_code = this->build_user_stat_key(application_auth_failed_login_count_key_prefix, username, failed_login_count_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    found = FT_FALSE;
    error_code = this->load_integer_value(failed_login_count_key.c_str(), failed_login_count, &found);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::get_consecutive_failed_login_count(const char *username, int64_t &failed_login_count) const noexcept
{
    ft_string consecutive_failed_login_count_key;
    ft_bool found;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_consecutive_failed_login_count");
    error_code = this->build_user_stat_key(application_auth_consecutive_failed_login_count_key_prefix, username, consecutive_failed_login_count_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    found = FT_FALSE;
    error_code = this->load_integer_value(consecutive_failed_login_count_key.c_str(), failed_login_count, &found);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::get_last_failed_login_ip_address(const char *username, ft_string &client_ip_address) const noexcept
{
    ft_string last_failed_ip_key;
    ft_bool found;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_last_failed_login_ip_address");
    error_code = this->build_user_stat_key(application_auth_last_failed_login_ip_key_prefix, username, last_failed_ip_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    found = FT_FALSE;
    error_code = this->load_string_value(last_failed_ip_key.c_str(), client_ip_address, &found);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (found == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::get_last_failed_login_timestamp(const char *username, int64_t &timestamp) const noexcept
{
    ft_string last_failed_timestamp_key;
    ft_bool found;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_last_failed_login_timestamp");
    error_code = this->build_user_stat_key(application_auth_last_failed_login_timestamp_key_prefix, username, last_failed_timestamp_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    found = FT_FALSE;
    error_code = this->load_integer_value(last_failed_timestamp_key.c_str(), timestamp, &found);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (found == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::get_last_successful_login_ip_address(const char *username, ft_string &client_ip_address) const noexcept
{
    ft_string last_successful_ip_key;
    ft_bool found;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_last_successful_login_ip_address");
    error_code = this->build_user_stat_key(application_auth_last_successful_login_ip_key_prefix, username, last_successful_ip_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    found = FT_FALSE;
    error_code = this->load_string_value(last_successful_ip_key.c_str(), client_ip_address, &found);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (found == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::get_last_successful_login_timestamp(const char *username, int64_t &timestamp) const noexcept
{
    ft_string last_successful_timestamp_key;
    ft_bool found;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_last_successful_login_timestamp");
    error_code = this->build_user_stat_key(application_auth_last_successful_login_timestamp_key_prefix, username, last_successful_timestamp_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    found = FT_FALSE;
    error_code = this->load_integer_value(last_successful_timestamp_key.c_str(), timestamp, &found);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (found == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::get_login_session_client_ip_address(const char *username, ft_string &client_ip_address) const noexcept
{
    ft_string session_value;
    ft_string session_token;
    int64_t login_timestamp;
    ft_bool found;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_login_session_client_ip_address");
    found = FT_FALSE;
    error_code = this->load_user_session_value(username, session_value, &found);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (found == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    error_code = this->parse_login_session_value(session_value.c_str(), session_token, client_ip_address, login_timestamp);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::get_login_session_timestamp(const char *username, int64_t &timestamp) const noexcept
{
    ft_string session_value;
    ft_string session_token;
    ft_string client_ip_address;
    ft_bool found;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::get_login_session_timestamp");
    found = FT_FALSE;
    error_code = this->load_user_session_value(username, session_value, &found);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (found == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    error_code = this->parse_login_session_value(session_value.c_str(), session_token, client_ip_address, timestamp);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::login_user(const char *username, const char *password, const char *client_ip_address,
    ft_string &session_token_output, ft_bool &authenticated) noexcept
{
    int32_t error_code;
    ft_bool user_exists;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::login_user");
    authenticated = FT_FALSE;
    error_code = this->authenticate_user(username, password, authenticated);
    if (error_code != FT_ERR_SUCCESS)
    {
        if (error_code == FT_ERR_INVALID_ARGUMENT)
            return (error_code);
        user_exists = FT_FALSE;
        if (this->user_exists(username, user_exists) != FT_ERR_SUCCESS || user_exists == FT_FALSE)
            return (error_code);
        if (this->record_login_failure(username, client_ip_address) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        return (error_code);
    }
    error_code = this->is_login_access_allowed(username);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->begin_user_session(username, client_ip_address, session_token_output);
    if (error_code != FT_ERR_SUCCESS)
    {
        authenticated = FT_FALSE;
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::login_with_signal(const char *username, const char *one_time_password, const char *client_ip_address,
    ft_string &session_token_output, ft_bool &authenticated) noexcept
{
    int32_t error_code;
    ft_bool user_exists;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::login_with_signal");
    authenticated = FT_FALSE;
    error_code = this->is_login_access_allowed(username);
    if (error_code != FT_ERR_SUCCESS)
    {
        if (error_code == FT_ERR_INVALID_ARGUMENT)
            return (error_code);
        user_exists = FT_FALSE;
        if (this->user_exists(username, user_exists) != FT_ERR_SUCCESS || user_exists == FT_FALSE)
            return (error_code);
        if (this->record_login_failure(username, client_ip_address) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        return (error_code);
    }
    error_code = this->authenticate_login_signal_one_time_password(username, one_time_password, authenticated);
    if (error_code != FT_ERR_SUCCESS)
    {
        if (error_code == FT_ERR_INVALID_ARGUMENT)
            return (error_code);
        user_exists = FT_FALSE;
        if (this->user_exists(username, user_exists) != FT_ERR_SUCCESS || user_exists == FT_FALSE)
            return (error_code);
        if (this->record_login_failure(username, client_ip_address) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        return (error_code);
    }
    error_code = this->begin_user_session(username, client_ip_address, session_token_output);
    if (error_code != FT_ERR_SUCCESS)
    {
        authenticated = FT_FALSE;
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::request_login_signal_one_time_password(const char *username) noexcept
{
    ft_string user_key;
    ft_string login_signal_key;
    ft_string one_time_password;
    ft_string one_time_password_hash;
    const char *existing_record;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::request_login_signal_one_time_password");
    error_code = this->build_user_key(username, user_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    existing_record = this->_credential_store.kv_get(user_key.c_str());
    if (existing_record == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    error_code = this->build_login_signal_one_time_password_key(username, login_signal_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->generate_login_signal_one_time_password(one_time_password);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->build_login_signal_one_time_password_hash(one_time_password.c_str(), one_time_password_hash);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    {
        int64_t login_signal_token_timeout_seconds;

        error_code = this->_settings.get_login_signal_token_timeout_seconds(login_signal_token_timeout_seconds);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = this->_credential_store.kv_set(login_signal_key.c_str(), one_time_password_hash.c_str(),
            login_signal_token_timeout_seconds);
    }
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->write_login_signal_one_time_password(username, one_time_password.c_str());
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_credential_store.kv_delete(login_signal_key.c_str());
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t application_auth_service::authenticate_login_signal_one_time_password(const char *username, const char *one_time_password, ft_bool &authenticated) noexcept
{
    ft_string login_signal_key;
    ft_string provided_hash;
    const char *stored_hash;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "application_auth_service::authenticate_login_signal_one_time_password");
    authenticated = FT_FALSE;
    error_code = this->build_login_signal_one_time_password_key(username, login_signal_key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    stored_hash = this->_credential_store.kv_get(login_signal_key.c_str());
    if (stored_hash == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    error_code = this->build_login_signal_one_time_password_hash(one_time_password, provided_hash);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (ft_strcmp(stored_hash, provided_hash.c_str()) != FT_ERR_SUCCESS)
        return (FT_ERR_PERMISSION_DENIED);
    error_code = this->_credential_store.kv_delete(login_signal_key.c_str());
    if (error_code != FT_ERR_SUCCESS && error_code != FT_ERR_NOT_FOUND)
        return (error_code);
    authenticated = FT_TRUE;
    return (FT_ERR_SUCCESS);
}
