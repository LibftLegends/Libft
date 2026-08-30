Networking_TARGET := networking.a
Networking_DEBUG_TARGET := networking_debug.a

Networking_SOURCES := networking_socket_class.cpp \
        networking_send_utils.cpp \
        networking.cpp \
        networking_dns_resolver.cpp \
        networking_setup_server.cpp \
        networking_setup_client.cpp \
        networking_setup_udp.cpp \
        networking_udp_event_loop.cpp \
        networking_socket_wrapper_functions.cpp \
        networking_socket_handle.cpp \
        networking_ssl_wrapper.cpp \
        networking_tls_aead.cpp \
        networking_quic_experimental.cpp \
        networking_message_transport.cpp \
        networking_crypto_backend.cpp \
        networking_secure_channel.cpp \
        networking_handshake.cpp \
        networking_simulator.cpp \
        networking_nat_traversal.cpp \
        networking_nonblocking.cpp \
        networking_event_loop.cpp \
        networking_http_client.cpp \
        networking_http2_client.cpp \
        networking_http_server.cpp \
        networking_websocket_client.cpp \
        networking_websocket_server.cpp \
        networking_socket_config_thread_safety.cpp

ifeq ($(OS),Windows_NT)
    Networking_SOURCES += networking_select.cpp
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        Networking_SOURCES += networking_epoll.cpp
    else ifeq ($(UNAME_S),Darwin)
        Networking_SOURCES += networking_kqueue.cpp
    else ifeq ($(UNAME_S),FreeBSD)
        Networking_SOURCES += networking_kqueue.cpp
    else ifeq ($(UNAME_S),NetBSD)
        Networking_SOURCES += networking_kqueue.cpp
    else ifeq ($(UNAME_S),OpenBSD)
        Networking_SOURCES += networking_kqueue.cpp
    else
        Networking_SOURCES += networking_select.cpp
    endif
endif

Networking_HEADERS := socket_class.hpp \
           networking.hpp \
           networking_ssl_compat.hpp \
           udp_socket.hpp \
           ssl_wrapper.hpp \
           networking_tls_aead.hpp \
           networking_quic_experimental.hpp \
           message_transport.hpp \
           networking_crypto_backend.hpp \
           networking_secure_channel.hpp \
           networking_handshake.hpp \
           networking_simulator.hpp \
           networking_nat_traversal.hpp \
           http_client.hpp \
           http2_client.hpp \
           http_server.hpp \
           websocket_client.hpp \
           websocket_server.hpp \
           socket_handle.hpp \
