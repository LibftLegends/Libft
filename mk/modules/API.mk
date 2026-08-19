API_TARGET         := API.a
API_DEBUG_TARGET   := API_debug.a

API_SOURCES := api_request.cpp api_request_tls.cpp api_request_async.cpp api_promise.cpp api_tls_client.cpp \
        api_content_length.cpp api_connection_pool.cpp api_connection_pool_handle.cpp api_http_common.cpp api_http_plain.cpp \
        api_http_tls.cpp api_http_retry.cpp api_request_error.cpp api_transport_hooks.cpp api_retry.cpp \
        api_request_signing.cpp api_request_metrics.cpp api_streaming_handler.cpp api_retry_policy.cpp

API_HEADERS := api.hpp promise.hpp tls_client.hpp api_internal.hpp api_http_internal.hpp api_http_common.hpp \
        api_request_signing.hpp api_request_metrics.hpp
