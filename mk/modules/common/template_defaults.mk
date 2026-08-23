ifeq ($(LIBFT_GLOBAL_MANIFEST_LOAD),)

MODULE_NAME ?= $(notdir $(CURDIR))
TOTAL_SRCS ?= $(words $(SRCS))

ifeq ($(OS),Windows_NT)
    SHELL := C:/Progra~1/Git/usr/bin/bash.exe
    .SHELLFLAGS := -lc
    export SHELL
    export LIBFT_POSIX_SHELL := 1
endif

ifeq ($(OS),Windows_NT)
    ifneq ($(LIBFT_POSIX_SHELL),)
        MKDIR := mkdir -p
        RM := rm -f
        RMDIR := rm -rf
    else
        MKDIR := mkdir
        RM := del /F /Q
        RMDIR := rmdir /S /Q
    endif
else
    MKDIR := mkdir -p
    RM := rm -f
    RMDIR := rm -rf
endif

ifdef COMPILE_FLAGS
    CFLAGS := $(COMPILE_FLAGS)
endif

CXX ?= g++
AR ?= ar
ARFLAGS := rcs

BUILD_OBJ_SUFFIX ?= $(BUILD_OUTPUT_SUFFIX)
ifneq ($(findstring -DLIBFT_TEST_BUILD,$(COMPILE_FLAGS)),)
    BUILD_OBJ_SUFFIX := _test$(BUILD_OUTPUT_SUFFIX)
endif

OBJDIR ?= objs$(BUILD_OBJ_SUFFIX)
OBJS ?= $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS ?= $(OBJS:.o=.d)
DEBUG_DEPS ?=

ifeq ($(shell uname -s 2>/dev/null),Darwin)
CFLAGS ?= -Wall -Wextra -Werror -g -O0 -std=c++17 \
          -Wmissing-declarations -Wshadow -Wformat=2 -Wundef -Wfloat-equal -Wodr \
          -Wno-format-nonliteral -Wno-tautological-compare \
          -DLIBFT_INTERNAL_HEADERS
else
CFLAGS ?= -Wall -Wextra -Werror -g -O0 -std=c++17 \
          -Wmissing-declarations -Wshadow -Wformat=2 -Wundef -Wfloat-equal -Wodr \
          -Wold-style-cast -Wconversion -Wuseless-cast \
          -Wzero-as-null-pointer-constant -Wmaybe-uninitialized \
          -DLIBFT_INTERNAL_HEADERS
endif

CLEAN_DIRS ?= $(wildcard objs*)
CLEAN_FILES ?= $(TARGET) $(OBJS) $(DEPS)
ifeq ($(OS),Windows_NT)
    ifeq ($(LIBFT_POSIX_SHELL),)
        CLEAN_FILES := $(subst /,\\,$(CLEAN_FILES))
        CLEAN_DIRS := $(subst /,\\,$(CLEAN_DIRS))
    endif
endif

endif
