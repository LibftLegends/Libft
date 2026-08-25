Crypto_TARGET := crypto.a
Crypto_DEBUG_TARGET := crypto_debug.a

Crypto_SOURCES := crypto_primitives.cpp \
        crypto_chacha20.cpp \
        crypto_poly1305.cpp \
        crypto_aead.cpp \
        crypto_x25519.cpp \
        crypto_random.cpp \
        crypto_session.cpp

Crypto_HEADERS := crypto_primitives.hpp \
        crypto_chacha20.hpp \
        crypto_poly1305.hpp \
        crypto_aead.hpp \
        crypto_x25519.hpp \
        crypto_random.hpp \
        crypto_session.hpp
