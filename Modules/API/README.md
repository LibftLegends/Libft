# API

The `API` module provides HTTP/HTTPS request helpers over the Networking and JSON modules. It supports callbacks, promises, streaming responses, retry policy configuration, transport hooks for tests/adapters, request signing, metrics guards, TLS diagnostics, connection-pool tuning, and async request helpers built on the separate `Threading` module.

## Callback and Hook Types

- `api_callback` - Receives a raw response body, status, and user data for async string requests.
- `api_json_callback` - Receives a parsed JSON body, status, and user data for async JSON requests.
- `api_stream_headers_callback` - Receives response status and headers for streaming requests.
- `api_stream_body_callback` - Receives streaming body chunks and can stop further processing.
- `api_transport_hooks` - Optional full transport override table for sync, async, TLS, host, HTTP/2, and streaming request paths.
- `api_set_transport_hooks(...)`, `api_clear_transport_hooks()`, and `api_get_transport_hooks()` - Install, clear, or inspect transport overrides.

## Streaming and Retry Classes

- `api_streaming_handler` - Lifecycle class holding streaming header/body callbacks and user data.
- `api_streaming_handler` methods - `initialize`, copy/move initialization, `destroy`, `move`, thread-safety toggles, `reset`, callback setters, `set_user_data`, and callback invocation helpers.
- `api_retry_policy` - Lifecycle class describing retry attempts, initial/max delay, backoff multiplier, circuit-breaker threshold, cooldown, and half-open success count.
- `api_retry_policy` methods - Lifecycle/thread-safety methods, `reset`, setters for each retry/circuit-breaker field, and getters for each field.

## Request Helpers

All request helpers accept method/path, optional JSON payload, optional headers, timeout, and optional retry policy unless the signature says otherwise.

- `api_request(...)` - Streaming request that can negotiate HTTP/2.
- `api_request_stream(...)`, `api_request_stream_http2(...)`, `api_request_stream_host(...)`, `api_request_stream_tls(...)`, and `api_request_stream_tls_http2(...)` - Streaming request variants for IP, host, TLS, and HTTP/2 paths.
- `api_request_string(...)`, `api_request_string_http2(...)`, `api_request_string_host(...)`, `api_request_string_tls(...)`, `api_request_string_tls_http2(...)`, and `api_request_https(...)` variants - Return allocated response text and write status when requested.
- The built-in HTTPS transport uses HTTP/1.1, including for HTTPS entry points with an HTTP/2 suffix. A custom transport hook can supply HTTP/2 and set the corresponding output flag.
- `api_request_string_bearer(...)`, `api_request_string_basic(...)`, host bearer/basic variants, and TLS bearer/basic variants - Add bearer or basic authorization to string requests.
- `api_request_json(...)`, `api_request_json_http2(...)`, `api_request_json_host(...)`, `api_request_json_tls(...)`, and `api_request_json_tls_http2(...)` - Return parsed JSON response groups.
- `api_request_json_bearer(...)`, `api_request_json_basic(...)`, host bearer/basic variants, and TLS bearer/basic variants - Add bearer or basic authorization to JSON requests.
- `api_request_string_url(...)` / `api_request_json_url(...)` - Parse a URL and dispatch a string or JSON request.
- Async variants `api_request_string_async`, `api_request_string_http2_async`, `api_request_string_tls_async`, `api_request_string_tls_http2_async`, `api_request_json_async`, `api_request_json_http2_async`, `api_request_json_tls_async`, and `api_request_json_tls_http2_async` - Dispatch asynchronous requests and invoke callbacks.

## Promises

- `api_promise` - `ft_promise<json_group *>` wrapper for non-TLS JSON requests.
- `api_string_promise` - `ft_promise<char *>` wrapper for non-TLS string requests.
- `api_tls_promise` - `ft_promise<json_group *>` wrapper for TLS JSON requests.
- `api_tls_string_promise` - `ft_promise<char *>` wrapper for TLS string requests.
- Each promise class exposes `request(...)` to start the matching request and fulfill the underlying promise.

## Signing and Metrics

- `api_hmac_signature_input` - Canonical request pieces used for HMAC signing.
- `api_sign_request_hmac_sha256(...)` - Computes an HMAC-SHA-256 request signature.
- `api_apply_hmac_signature_header(...)` - Builds a signed header value.
- `api_oauth1_parameter` / `api_oauth1_parameters` - OAuth 1 signing input fields.
- `api_build_oauth1_authorization_header(...)` - Builds an OAuth 1 Authorization header.
- `api_request_metrics_guard` - RAII-style metrics helper that records endpoint, method, resource, request bytes, result body, status, and error pointer.

## TLS Client and Diagnostics

- `api_tls_certificate_diagnostics` - Certificate subject, issuer, serial number, validity window, and SHA-256 fingerprint.
- `api_tls_handshake_diagnostics` - TLS protocol, cipher, and certificate chain diagnostics.
- `api_tls_client` - Lifecycle TLS client with sync string/JSON requests and async request support.
- `api_tls_client` methods - Lifecycle/thread-safety methods, configured initialization, `is_valid`, `request`, `request_json`, `request_async`, `refresh_handshake_diagnostics`, and `get_handshake_diagnostics`.

## Connection Pool and Debug Counters

- `api_connection_pool_flush()` - Clears idle pooled connections.
- `api_connection_pool_set_max_idle(...)` / `api_connection_pool_get_max_idle()` - Configure or read idle connection capacity.
- `api_connection_pool_set_idle_timeout(...)` / `api_connection_pool_get_idle_timeout()` - Configure or read idle timeout.
- `api_debug_reset_connection_pool_counters()` and acquire/reuse/miss getters - Inspect connection pool behavior.
- Async debug getters - Read last async request size, bytes sent/received, send/receive state, and send/receive timeout.
