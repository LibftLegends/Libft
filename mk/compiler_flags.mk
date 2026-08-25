ifndef COMPILER_FLAGS_INCLUDED
COMPILER_FLAGS_INCLUDED := 1

OPT_LEVEL ?= 0
LIBFT_ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
UNAME_S := $(shell uname -s 2>/dev/null)

ifeq ($(OPT_LEVEL),0)
    OPT_FLAGS = -O0 -g
else ifeq ($(OPT_LEVEL),1)
    ifeq ($(UNAME_S),Darwin)
        OPT_FLAGS = -O1 -flto -ffunction-sections -fdata-sections -Wl,-dead_strip
    else
        OPT_FLAGS = -O1 -s -ffunction-sections -fdata-sections -Wl,--gc-sections
    endif
else ifeq ($(OPT_LEVEL),2)
    ifeq ($(UNAME_S),Darwin)
        OPT_FLAGS = -O2 -flto -ffunction-sections -fdata-sections -Wl,-dead_strip
    else
        OPT_FLAGS = -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections
    endif
else ifeq ($(OPT_LEVEL),3)
    ifeq ($(UNAME_S),Darwin)
        OPT_FLAGS = -O3 -flto -ffunction-sections -fdata-sections -Wl,-dead_strip
    else
        OPT_FLAGS = -O3 -s -ffunction-sections -fdata-sections -Wl,--gc-sections
    endif
else
    $(error Unsupported OPT_LEVEL=$(OPT_LEVEL))
endif

SANITIZERS ?=
SANITIZER_FLAGS :=
SANITIZER_SUFFIX :=
ifneq ($(strip $(SANITIZERS)),)
    SANITIZER_SELECTION := $(sort $(SANITIZERS))
    UNSUPPORTED_SANITIZERS := $(filter-out address undefined thread,$(SANITIZER_SELECTION))
    ifneq ($(UNSUPPORTED_SANITIZERS),)
        $(error Unsupported SANITIZERS: $(UNSUPPORTED_SANITIZERS))
    endif
    ifneq ($(filter address,$(SANITIZER_SELECTION)),)
        SANITIZER_FLAGS += -fsanitize=address
    endif
    ifneq ($(filter undefined,$(SANITIZER_SELECTION)),)
        SANITIZER_FLAGS += -fsanitize=undefined
    endif
    ifneq ($(filter thread,$(SANITIZER_SELECTION)),)
        SANITIZER_FLAGS += -fsanitize=thread
    endif
    SANITIZER_FLAGS += -fno-omit-frame-pointer
    EMPTY :=
    SPACE := $(EMPTY) $(EMPTY)
    SANITIZER_SUFFIX := _san_$(subst $(SPACE),_,$(SANITIZER_SELECTION))

    LIBFT_SANITIZER_PROBE := /tmp/libft_sanitizer_probe_$(SANITIZER_SUFFIX)
    LIBFT_SANITIZER_PROBE_RESULT := $(shell printf 'int main(void){return 0;}\n' | $(CXX) $(SANITIZER_FLAGS) -pthread -x c++ - -o $(LIBFT_SANITIZER_PROBE) >/dev/null 2>&1; probe_status=$$?; rm -f $(LIBFT_SANITIZER_PROBE); if [ $$probe_status -eq 0 ]; then printf '1'; else printf '0'; fi)
    ifneq ($(LIBFT_SANITIZER_PROBE_RESULT),1)
        $(error Sanitizer toolchain unavailable for SANITIZERS=$(SANITIZER_SELECTION); compiler/linker runtime support is required)
    endif
endif

BUILD_OUTPUT_SUFFIX := _opt$(OPT_LEVEL)$(SANITIZER_SUFFIX)

COMPILE_FLAGS ?= -Wall -Werror -Wextra -std=c++17 -Wmissing-declarations \
                -Wshadow -Wformat=2 -Wundef \
                -Wfloat-equal -Wodr \
                -DLIBFT_INTERNAL_HEADERS \
                $(OPT_FLAGS) $(SANITIZER_FLAGS)

ifeq ($(UNAME_S),Darwin)
    COMPILE_FLAGS += -Wno-format-nonliteral -Wno-tautological-compare
else
    COMPILE_FLAGS += -Wold-style-cast -Wconversion -Wuseless-cast \
        -Wzero-as-null-pointer-constant
    ifneq ($(findstring clang,$(CXX)),)
        COMPILE_FLAGS += -Wuninitialized
        COMPILE_FLAGS := $(filter-out -Wuseless-cast,$(COMPILE_FLAGS))
    else
        COMPILE_FLAGS += -Wmaybe-uninitialized
    endif
endif

endif
