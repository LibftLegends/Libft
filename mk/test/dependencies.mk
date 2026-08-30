ifeq ($(LIBFT_DISABLE_OPENSSL),1)
HAS_OPENSSL_HEADERS := 0
else
HAS_OPENSSL_HEADERS := $(shell printf "\#include <openssl/ssl.h>\n" | $(CXX) -x c++ -E - >/dev/null 2>&1 && echo 1 || echo 0)
endif
ifneq ($(HAS_OPENSSL_HEADERS),1)
SRCS := $(filter-out Test/test_api_tls_diagnostics.cpp \
                    Test/test_encryption_aead.cpp \
                    Test/test_encryption_aead_copy_move.cpp \
                    Test/test_encryption_hash_algorithms.cpp,$(SRCS))
endif

ifeq ($(LIBFT_DISABLE_OPENSSL),1)
HAS_OPENSSL_LIBS := 0
else
HAS_OPENSSL_LIBS := $(shell printf 'int main(void){return 0;}\n' | $(CXX) -x c++ -o /tmp/libft_test_openssl_check_$$$$_tmp - -lssl -lcrypto >/dev/null 2>&1 && echo 1 || echo 0)
endif
ifeq ($(and $(HAS_OPENSSL_HEADERS),$(HAS_OPENSSL_LIBS)),1)
OPENSSL_LIBS := -lssl -lcrypto
else
OPENSSL_LIBS :=
endif

HAS_SQLITE3_HEADERS := $(shell printf "\#include <sqlite3.h>\n" | $(CXX) -x c++ -E - >/dev/null 2>&1 && echo 1 || echo 0)
HAS_SQLITE3_LIBS := $(shell printf 'int main(void){return 0;}\n' | $(CXX) -x c++ -o /tmp/libft_test_sqlite3_check_$$$$_tmp - -lsqlite3 >/dev/null 2>&1 && echo 1 || echo 0)
ifeq ($(and $(HAS_SQLITE3_HEADERS),$(HAS_SQLITE3_LIBS)),1)
SQLITE_LIBS := -lsqlite3
else
SQLITE_LIBS :=
endif

HAS_X11_LIB := $(shell ldconfig -p 2>/dev/null | grep -q "libX11.so" && echo 1 || echo 0)
HAS_ASOUND_LIB := $(shell ldconfig -p 2>/dev/null | grep -q "libasound.so" && echo 1 || echo 0)
ifeq ($(HAS_X11_LIB),1)
X11_LIBS := -lX11
else
X11_LIBS :=
endif

HAS_XEXT_LIB := $(shell ldconfig -p 2>/dev/null | grep -q "libXext.so" && echo 1 || echo 0)
HAS_XI_LIB := $(shell ldconfig -p 2>/dev/null | grep -q "libXi.so" && echo 1 || echo 0)
HAS_GL_LIB := $(shell ldconfig -p 2>/dev/null | grep -q "libGL.so" && echo 1 || echo 0)
ifeq ($(HAS_ASOUND_LIB),1)
ASOUND_LIBS := -lasound
else
ASOUND_LIBS :=
endif

ifeq ($(HAS_XEXT_LIB),1)
XEXT_LIBS := -lXext
else
XEXT_LIBS :=
endif

ifeq ($(HAS_XI_LIB),1)
XI_LIBS := -lXi
else
XI_LIBS :=
endif

ifeq ($(HAS_GL_LIB),1)
GL_LIBS := -lGL
else
GL_LIBS :=
endif
