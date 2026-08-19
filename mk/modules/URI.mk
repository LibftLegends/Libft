URI_TARGET := uri.a
URI_DEBUG_TARGET := uri_debug.a

URI_SOURCES := uri_internal.cpp \
        uri_components_reset.cpp \
        uri_components_destroy.cpp \
        uri_parse.cpp \
        uri_normalize.cpp \
        uri_percent_encode_component.cpp \
        uri_percent_decode_component.cpp \
        uri_query_get_value.cpp \
        uri_query_has_key.cpp

URI_HEADERS := uri.hpp uri_internal.hpp
