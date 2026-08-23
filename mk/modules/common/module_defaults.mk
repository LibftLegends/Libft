ifeq ($(LIBFT_GLOBAL_MANIFEST_LOAD),)

MODULE_NAME ?= $(notdir $(CURDIR))
TOTAL_SRCS ?= $(words $(SRCS) $(MM_SRCS))
LIBFT_BATCH_OUTPUT ?= 0
LIBFT_PARALLEL_JOBS = $(filter -j% j%,$(MAKEFLAGS))
LIBFT_EXPLICIT_J1 = $(filter -j1 j1,$(MAKEFLAGS))
LIBFT_JOBSERVER = $(findstring --jobserver-auth,$(MAKEFLAGS))
ifneq ($(LIBFT_JOBSERVER),)
    ifeq ($(LIBFT_EXPLICIT_J1),)
        LIBFT_BATCH_OUTPUT = 1
    endif
else ifneq ($(LIBFT_PARALLEL_JOBS),)
    ifeq ($(LIBFT_EXPLICIT_J1),)
        LIBFT_BATCH_OUTPUT = 1
    endif
endif

ifeq ($(OS),Windows_NT)
    SHELL := C:/Progra~1/Git/usr/bin/bash.exe
    .SHELLFLAGS := -lc
    export SHELL
    export LIBFT_POSIX_SHELL := 1
    ifneq ($(LIBFT_POSIX_SHELL),)
        MKDIR ?= mkdir -p
        RM ?= rm -f
        RMDIR ?= rm -rf
    else
        MKDIR ?= mkdir
        RM ?= del /F /Q
        RMDIR ?= rmdir /S /Q
    endif
else
    MKDIR ?= mkdir -p
    RM ?= rm -f
    RMDIR ?= rm -rf
endif

ifdef COMPILE_FLAGS
    CFLAGS := $(COMPILE_FLAGS)
endif
ifeq ($(OS),Windows_NT)
    CFLAGS += -I../..
endif

CXX ?= g++
AR ?= ar
ARFLAGS := rcs

# The parent build may append a compiler/flags fingerprint to this suffix.
BUILD_OBJ_SUFFIX ?= $(BUILD_OUTPUT_SUFFIX)
ifneq ($(findstring -DLIBFT_TEST_BUILD,$(COMPILE_FLAGS)),)
    BUILD_OBJ_SUFFIX := _test$(BUILD_OUTPUT_SUFFIX)
endif

OBJDIR ?= objs$(BUILD_OBJ_SUFFIX)
DEBUG_OBJDIR ?= objs_debug$(BUILD_OBJ_SUFFIX)

CPP_OBJS ?= $(patsubst %.cpp,$(OBJDIR)/%.o,$(filter %.cpp,$(SRCS)))
MM_OBJS ?= $(patsubst %.mm,$(OBJDIR)/%.o,$(MM_SRCS))
OBJS ?= $(CPP_OBJS) $(MM_OBJS)
DEBUG_CPP_OBJS ?= $(patsubst %.cpp,$(DEBUG_OBJDIR)/%.o,$(filter %.cpp,$(SRCS)))
DEBUG_MM_OBJS ?= $(patsubst %.mm,$(DEBUG_OBJDIR)/%.o,$(MM_SRCS))
DEBUG_OBJS ?= $(DEBUG_CPP_OBJS) $(DEBUG_MM_OBJS)
DEPS ?= $(OBJS:.o=.d)
DEBUG_DEPS ?= $(DEBUG_OBJS:.o=.d)
.SILENT: $(OBJS) $(DEBUG_OBJS)

MODULE_CFLAGS_EXTRA ?=
MODULE_MMFLAGS_EXTRA ?=
ifeq ($(shell uname -s 2>/dev/null),Darwin)
CFLAGS ?= -Wall -Wextra -Werror -g -O0 -std=c++17 \
          -Wmissing-declarations -Wshadow -Wformat=2 -Wundef -Wfloat-equal -Wodr \
          -Wno-format-nonliteral -Wno-tautological-compare \
          -DLIBFT_INTERNAL_HEADERS $(MODULE_CFLAGS_EXTRA)
else
CFLAGS ?= -Wall -Wextra -Werror -g -O0 -std=c++17 \
          -Wmissing-declarations -Wshadow -Wformat=2 -Wundef -Wfloat-equal -Wodr \
          -Wold-style-cast -Wconversion -Wuseless-cast \
          -Wzero-as-null-pointer-constant -Wmaybe-uninitialized \
          -DLIBFT_INTERNAL_HEADERS $(MODULE_CFLAGS_EXTRA)
endif
MMFLAGS ?= $(CFLAGS) $(MODULE_MMFLAGS_EXTRA)

endif
