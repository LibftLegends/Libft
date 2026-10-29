# Application

The `Application` module is the first top-level application-services layer in FullLibft.
It is intended for login/auth-style workflows that combine storage, encryption, and later
service-specific behavior without forcing higher layers to reassemble the same plumbing.

## `application_settings`

- `application_settings` - Lifecycle configuration object that groups the database, encryption, approval, and login-signal settings for an application in one place.
- Lifecycle methods - `initialize`, copy/move initialization, `destroy`, and `move`.
- `initialize(const char *database_root_path, const char *database_relative_path, const char *encryption_key, ft_bool enable_encryption, const char *encryption_algorithm_name)` - Sets the base database location and encryption defaults.
- `set_database_location(...)` / `get_database_root_path(...)` / `get_database_relative_path(...)` - Stores and reads the root-restricted database location.
- `set_encryption_key(...)` / `get_encryption_key(...)` / `set_encryption_algorithm_name(...)` / `get_encryption_algorithm_name(...)` / `set_encryption_enabled(...)` / `is_encryption_enabled(...)` - Stores the encryption configuration.
- `set_manual_login_approval_enabled(...)` / `is_manual_login_approval_enabled(...)` - Stores the global manual-approval override.
- `set_login_signal_output_file_descriptor(...)` / `get_login_signal_output_file_descriptor(...)` - Stores the descriptor used to print login-signal one-time passwords.
- `set_login_signal_token_timeout_seconds(...)` / `get_login_signal_token_timeout_seconds(...)` - Stores the login-signal one-time-password lifetime.
- `set_login_session_timeout_seconds(...)` / `get_login_session_timeout_seconds(...)` - Stores the default login-session lifetime.
- The settings object is intended to be configured before sharing across threads; callers must synchronize concurrent reads and writes themselves.

## `application_auth_service`

- `application_auth_service` - Lifecycle service that manages a credential database backed by `kv_store`.
- Lifecycle methods - `initialize`, copy/move initialization, `destroy`, and `move`.
- `is_initialised()` - Reports whether the service has been initialized.
- `initialize(const application_settings &settings)` - Boots the service from a preconfigured settings object.
- `initialize(const char *database_root_path, const char *database_relative_path, const char *encryption_key, ft_bool enable_encryption, const char *encryption_algorithm_name)` - Opens the store only when the relative database path stays inside the configured root directory.
- `register_user(...)` - Creates a salted password record for a new username.
- `authenticate_user(...)` - Verifies a username/password pair against the stored record.
- `user_exists(...)` - Checks whether a username already exists.
- `remove_user(...)` - Deletes a stored user record.
- `set_login_approval_required_for_user(...)` / `is_login_approval_required_for_user(...)` - Enables or disables manual approval for a specific user.
- `set_manual_login_approval_enabled(...)` / `is_manual_login_approval_enabled(...)` - Enables or disables the global approval override for all logins.
- `approve_login(...)` / `revoke_login_approval(...)` - Marks a user as approved or removes that approval.
- `is_login_approved(...)` - Reads whether a user currently has manual approval.
- `set_login_signal_output_file_descriptor(...)` / `get_login_signal_output_file_descriptor(...)` - Selects the file descriptor used to print login-signal one-time passwords.
- `set_login_signal_token_timeout_seconds(...)` / `get_login_signal_token_timeout_seconds(...)` - Configures the one-time-password lifetime.
- `set_login_session_timeout_seconds(...)` / `get_login_session_timeout_seconds(...)` - Configures the login-session lifetime.
- `login_user(...)` / `login_with_signal(...)` - Authenticates a user, records login audit metadata, and opens an active session token on success. Both flows enforce the configured approval rules before starting a session.
- `end_user_session(...)` - Clears the stored active session record for one user.
- `is_user_logged_in(...)` / `get_logged_in_usernames(...)` - Queries active session state. `get_logged_in_usernames(...)` expects an already initialized `ft_vector<ft_string>` out-parameter.
- `get_failed_login_count(...)` / `get_consecutive_failed_login_count(...)` - Reports total and consecutive failed-login counters.
- `get_last_failed_login_ip_address(...)` / `get_last_failed_login_timestamp(...)` - Reports the most recent failed-login source and time.
- `get_last_successful_login_ip_address(...)` / `get_last_successful_login_timestamp(...)` - Reports the most recent successful-login source and time.
- `get_login_session_client_ip_address(...)` / `get_login_session_timestamp(...)` - Reads the current active-session source IP and creation time for a user.
- `request_login_signal_one_time_password(...)` - Generates a one-time password for a user, stores a hashed copy with TTL, and prints the cleartext token to the configured descriptor.
- `authenticate_login_signal_one_time_password(...)` - Validates and consumes a previously issued one-time password.

## Public Contract

| Method | Behavior | Return codes |
| --- | --- | --- |
| `application_settings::initialize(database_root_path, database_relative_path, encryption_key, enable_encryption, encryption_algorithm_name)` | Creates a settings object with the database path, encryption defaults, and selected block-cipher name. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from string or lifecycle helpers |
| `application_settings::set_database_location(database_root_path, database_relative_path)` | Updates the base database location stored in the settings object. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from string helpers |
| `application_settings::get_database_root_path(database_root_path)` | Reads the configured database root path. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from string helpers |
| `application_settings::get_database_relative_path(database_relative_path)` | Reads the configured relative database path. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from string helpers |
| `application_settings::set_encryption_key(encryption_key)` | Stores the encryption key or an empty key when encryption is disabled. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from string helpers |
| `application_settings::get_encryption_key(encryption_key)` | Reads the configured encryption key. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from string helpers |
| `application_settings::set_encryption_algorithm_name(encryption_algorithm_name)` | Stores the selected encryption algorithm name for this settings object. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from string helpers |
| `application_settings::get_encryption_algorithm_name(encryption_algorithm_name)` | Reads the selected encryption algorithm name. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from string helpers |
| `application_settings::set_encryption_enabled(enabled)` | Stores whether the database should be encrypted. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `application_settings::is_encryption_enabled(enabled)` | Reads the encryption flag. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `application_settings::set_manual_login_approval_enabled(enabled)` | Stores the global manual-approval override flag. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `application_settings::is_manual_login_approval_enabled(enabled)` | Reads the global manual-approval override flag. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `application_settings::set_login_signal_output_file_descriptor(file_descriptor)` | Stores the descriptor used when printing issued one-time passwords. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `application_settings::get_login_signal_output_file_descriptor(file_descriptor)` | Reads the descriptor used when printing issued one-time passwords. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `application_settings::set_login_signal_token_timeout_seconds(timeout_seconds)` | Stores the one-time-password lifetime in seconds. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `application_settings::get_login_signal_token_timeout_seconds(timeout_seconds)` | Reads the one-time-password lifetime in seconds. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `application_settings::set_login_session_timeout_seconds(timeout_seconds)` | Stores the default login-session lifetime in seconds. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `application_settings::get_login_session_timeout_seconds(timeout_seconds)` | Reads the default login-session lifetime in seconds. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `initialize(settings)` | Boots the auth service from an already configured settings object. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_INVALID_PATH`, `FT_ERR_IO`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage or encoding helpers |
| `initialize(database_root_path, database_relative_path, encryption_key, enable_encryption, encryption_algorithm_name)` | Requires a non-empty root path and a non-empty relative database path. Rejects absolute paths, traversal outside the root, and paths whose existing parent or final object resolves through a symlink/reparse point outside the canonical root. A missing final database file is allowed when its existing parent resolves inside the root. The selected algorithm name is passed through to the credential store so the instance can be reconfigured independently. This validation is point-in-time; callers must still use secure open/replace operations to avoid a later TOCTOU race. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_INVALID_PATH`, `FT_ERR_IO`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage or encoding helpers |
| `register_user(username, password)` | Fails if the user already exists. Stores a salted SHA-256 credential record. | `FT_ERR_SUCCESS`, `FT_ERR_ALREADY_EXISTS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage or encoding helpers |
| `authenticate_user(username, password, authenticated)` | Verifies the password first. If the user has a manual approval requirement or the global override is enabled, approval must also be present. Sets `authenticated` to `FT_FALSE` on failure. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_PERMISSION_DENIED`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage or encoding helpers |
| `user_exists(username, exists)` | Reports whether a user record exists. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage or encoding helpers |
| `remove_user(username)` | Deletes the user record, the approval record, and the per-user approval requirement flag. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage or encoding helpers |
| `set_login_approval_required_for_user(username, enabled)` | Enables or disables approval for one user. Requires the user to exist. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage helpers |
| `is_login_approval_required_for_user(username, enabled)` | Reports whether the per-user approval requirement is set. Missing keys are treated as disabled. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage helpers |
| `set_manual_login_approval_enabled(enabled)` | Stores the global override flag for all logins. When enabled, every login requires approval. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage helpers |
| `is_manual_login_approval_enabled(enabled)` | Reports the cached global override flag. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage helpers |
| `approve_login(username)` | Marks a user as approved. Requires the user to exist. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage helpers |
| `revoke_login_approval(username)` | Removes the stored approval record. Missing approval records are treated as success. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage helpers |
| `is_login_approved(username, approved)` | Reports whether the user currently has approval. Missing keys are treated as not approved. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage helpers |
| `set_login_signal_output_file_descriptor(file_descriptor)` | Changes the descriptor used when printing issued one-time passwords. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `get_login_signal_output_file_descriptor(file_descriptor)` | Reads the current descriptor used for one-time-password output. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `set_login_signal_token_timeout_seconds(timeout_seconds)` | Updates the one-time-password lifetime in seconds. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `get_login_signal_token_timeout_seconds(timeout_seconds)` | Reads the current one-time-password lifetime in seconds. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `set_login_session_timeout_seconds(timeout_seconds)` | Updates the default session lifetime in seconds. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `get_login_session_timeout_seconds(timeout_seconds)` | Reads the default session lifetime in seconds. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT` |
| `request_login_signal_one_time_password(username)` | Generates a random one-time password, stores only its hash with TTL, and prints the cleartext token to the configured descriptor. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_IO`, `FT_ERR_*` from storage, encoding, or secure-random helpers |
| `authenticate_login_signal_one_time_password(username, one_time_password, authenticated)` | Verifies the token hash, consumes the stored token on success, and sets `authenticated` accordingly. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_PERMISSION_DENIED`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage or encoding helpers |
| `login_user(username, password, client_ip_address, session_token_output, authenticated)` | Verifies the password, records login audit metadata, and creates a new active session token on success. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_PERMISSION_DENIED`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_IO`, `FT_ERR_*` from storage, encoding, or time helpers |
| `login_with_signal(username, one_time_password, client_ip_address, session_token_output, authenticated)` | Validates a one-time password, enforces the same approval rules as password login, records login audit metadata, and creates a new active session token on success. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_PERMISSION_DENIED`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_IO`, `FT_ERR_*` from storage, encoding, or time helpers |
| `end_user_session(username)` | Deletes the active session record for one user. Missing sessions are treated as success. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage helpers |
| `is_user_logged_in(username, logged_in)` | Reports whether an active session exists for one user. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_*` from storage helpers |
| `get_logged_in_usernames(usernames)` | Collects all usernames that currently have active session records. The caller must initialize `usernames` before calling. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_INVALID_STATE`, `FT_ERR_*` from storage or vector helpers |
| `get_failed_login_count(username, failed_login_count)` | Reads the cumulative failed-login counter for one user. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage helpers |
| `get_consecutive_failed_login_count(username, failed_login_count)` | Reads the consecutive failed-login counter for one user. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage helpers |
| `get_last_failed_login_ip_address(username, client_ip_address)` | Reads the most recent failed-login source IP for one user. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage helpers |
| `get_last_failed_login_timestamp(username, timestamp)` | Reads the most recent failed-login timestamp for one user. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage, encoding, or time helpers |
| `get_last_successful_login_ip_address(username, client_ip_address)` | Reads the most recent successful-login source IP for one user. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage helpers |
| `get_last_successful_login_timestamp(username, timestamp)` | Reads the most recent successful-login timestamp for one user. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage, encoding, or time helpers |
| `get_login_session_client_ip_address(username, client_ip_address)` | Reads the client IP stored with the active session for one user. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage helpers |
| `get_login_session_timestamp(username, timestamp)` | Reads the creation timestamp stored with the active session for one user. | `FT_ERR_SUCCESS`, `FT_ERR_NOT_FOUND`, `FT_ERR_INVALID_ARGUMENT`, `FT_ERR_CONFIGURATION`, `FT_ERR_*` from storage, encoding, or time helpers |

## Storage format

The starter implementation stores each account as a salted SHA-256 record in `kv_store`.
The database file location is selected manually, but it must be provided as a relative path
inside a caller-supplied root directory. That root check is enforced with the `Filesystem`
path helpers before the store is opened.
The `application_settings` object is the preferred place to gather those database and runtime
options before handing them to `application_auth_service`.
Manual approval state is persisted alongside the user records so approval can be enabled
per user, and a separate global override can force approval for every login when needed.
The store itself can be encrypted through the existing `Storage` module configuration.
This is intentionally small and easy to extend when the module grows to cover sessions,
roles, password resets, audit logging, or external identity providers.

## Storage Keys

- `application/auth/users/<username>` - Stored credential record for a user.
- `application/auth/approvals/<username>` - Manual approval flag for a user.
- `application/auth/settings/approval_required/<username>` - Per-user approval requirement flag.
- `application/auth/settings/manual_login_approval` - Global approval override flag.
- `application/auth/login_signal/<username>` - Hashed one-time password issued for a login signal.
- `application/auth/sessions/<username>` - Active session token, client IP address, and login timestamp for a logged-in user.
- `application/auth/audit/failed_count/<username>` - Cumulative failed-login counter.
- `application/auth/audit/consecutive_failed_count/<username>` - Consecutive failed-login counter.
- `application/auth/audit/last_failed_ip/<username>` - Most recent failed-login source IP.
- `application/auth/audit/last_failed_timestamp/<username>` - Most recent failed-login timestamp.
- `application/auth/audit/last_successful_ip/<username>` - Most recent successful-login source IP.
- `application/auth/audit/last_successful_timestamp/<username>` - Most recent successful-login timestamp.

## Approval Rules

- Per-user approval can be enabled for selected users only.
- The global override applies to every login and has priority over the per-user setting.
- When the global override is enabled, approval is required even if a user does not have a per-user requirement flag.
- Removing a user clears the stored credential, the approval record, and the per-user approval requirement flag.

## Login Signal One-Time Passwords

- A login signal can issue a one-time password for a specific user.
- The generated token is printed to the configured file descriptor immediately after creation.
- The backend stores only a hashed copy of the token and expires it automatically after the configured timeout.
- The default one-time-password timeout is 300 seconds, which is roughly 5 minutes.
- A token is consumed after a successful authentication attempt.
- The configured file descriptor and timeout live in `application_settings` so they can be adjusted in one place.

## Session and Audit Data

- Successful logins create a per-user session record with a random session token, the client IP address, and a creation timestamp.
- Session creation is performed by `login_user(...)` and `login_with_signal(...)`; the lower-level session builder is an internal helper now.
- The default session timeout is 28,800 seconds, which is 8 hours.
- Failed logins update cumulative and consecutive failure counters.
- Failed and successful login attempts store the most recent source IP and timestamp for later inspection.
- `get_logged_in_usernames(...)` returns the current active-session usernames using the caller-provided vector container.
