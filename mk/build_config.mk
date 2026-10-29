ifndef LIBFT_BUILD_CONFIG_INCLUDED
LIBFT_BUILD_CONFIG_INCLUDED := 1

ifeq ($(OS),Windows_NT)
SHELL := C:/Progra~1/Git/usr/bin/bash.exe
.SHELLFLAGS := -lc
export SHELL
export LIBFT_POSIX_SHELL := 1
endif

DEMO_OPT_LEVEL ?= 3

CXX             := g++
AR              := ar
ARFLAGS         := rcs
CLANG_FORMAT   ?= clang-format

MAKEFLAGS      += --no-print-directory

LIBFT_PARALLEL_JOBS = $(filter -j% j%,$(MAKEFLAGS))
LIBFT_EXPLICIT_J1 = $(filter -j1 j1,$(MAKEFLAGS))
LIBFT_JOBSERVER = $(findstring --jobserver-auth,$(MAKEFLAGS))
LIBFT_BATCH_OUTPUT = 0
ifneq ($(LIBFT_JOBSERVER),)
    ifeq ($(LIBFT_EXPLICIT_J1),)
        LIBFT_BATCH_OUTPUT = 1
    endif
else ifneq ($(LIBFT_PARALLEL_JOBS),)
    ifeq ($(LIBFT_EXPLICIT_J1),)
        LIBFT_BATCH_OUTPUT = 1
    endif
endif

SUBMAKE_OVERRIDES ?=
LIBFT_ARCHIVE_SUFFIX ?=

TEMP_DIRS :=
OUTPUT_LOGS :=

ifeq ($(OS),Windows_NT)
    ifeq ($(strip $(LIBFT_POSIX_SHELL)),)
        ifneq ($(findstring bash,$(SHELL)),)
            LIBFT_POSIX_SHELL := 1
        else ifneq ($(findstring sh,$(SHELL)),)
            LIBFT_POSIX_SHELL := 1
        endif
    endif
    ifneq ($(LIBFT_POSIX_SHELL),)
        MKDIR  = mkdir -p
        RM     = rm -f
        RMDIR  = rm -rf
    else
        MKDIR  = mkdir
        RM     = del /F /Q
        RMDIR  = rmdir /S /Q
    endif
else
    MKDIR  = mkdir -p
    RM     = rm -f
    RMDIR  = rm -rf
endif

SUBDIRS := Modules/Basic \
           Modules/BMP \
           Modules/Advanced \
           Modules/Compatebility \
           Modules/Debug \
           Modules/Errno \
           Modules/CMA \
           Modules/SCMA \
           Modules/GetNextLine \
           Modules/DUMB \
           Modules/Math \
           Modules/Geometry \
           Modules/System_utils \
           Modules/Printf \
           Modules/ReadLine \
           Modules/Regex \
           Modules/PThread \
           Modules/Threading \
           Modules/CPP_class \
           Modules/Template \
           Modules/Buffer \
           Modules/CLI \
           Modules/Command \
           Modules/Config \
           Modules/CrossProcess \
           Modules/Compression \
           Modules/CSV \
           Modules/Encryption \
           Modules/Crypto \
           Modules/Encoding \
           Modules/RNG \
           Modules/JSon \
           Modules/YAML \
           Modules/File \
           Modules/HTML \
           Modules/Time \
           Modules/Filesystem \
           Modules/XML \
           Modules/Storage \
           Modules/Networking \
           Modules/URI \
           Modules/API \
           Modules/Application \
           Modules/Observability \
           Modules/Sink \
           Modules/Logger \
           Modules/Parser \
           Modules/Game \
           Modules/Voxel \
           Modules/GPGR

LIB_BASES := \
  Modules/Basic/Basic \
  Modules/BMP/bmp \
  Modules/Advanced/Advanced \
  Modules/Compatebility/Compatebility \
  Modules/Debug/Debug \
  Modules/Errno/errno \
  Modules/CMA/CustomMemoryAllocator \
  Modules/SCMA/SCMA \
  Modules/GetNextLine/GetNextLine \
  Modules/DUMB/dumb \
  Modules/Math/Math \
  Modules/Geometry/geometry \
  Modules/System_utils/System_utils \
  Modules/Printf/Printf \
  Modules/ReadLine/ReadLine \
  Modules/Regex/regex \
  Modules/PThread/PThread \
  Modules/Threading/Threading \
  Modules/CPP_class/CPP_class \
  Modules/Buffer/buffer \
  Modules/CLI/cli \
  Modules/Command/command \
  Modules/Config/config \
  Modules/CrossProcess/CrossProcess \
  Modules/Compression/compression \
  Modules/CSV/CSV \
  Modules/Encryption/encryption \
  Modules/Crypto/crypto \
  Modules/Encoding/encoding \
  Modules/RNG/RNG \
  Modules/JSon/JSon \
  Modules/YAML/YAML  \
  Modules/File/file \
  Modules/HTML/HTMLParser \
  Modules/Time/time \
  Modules/Filesystem/filesystem \
  Modules/XML/XMLParser \
  Modules/Storage/storage \
  Modules/Networking/networking \
  Modules/URI/uri \
  Modules/API/API \
  Modules/Application/Application \
  Modules/Observability/Observability \
  Modules/Sink/Sink \
  Modules/Logger/Logger \
  Modules/Parser/parser \
  Modules/Game/Game \
  Modules/Voxel/Voxel \
  Modules/GPGR/GPGR

LIBS       := $(addsuffix $(LIBFT_ARCHIVE_SUFFIX).a, $(LIB_BASES))
DEBUG_LIBS := $(addsuffix $(LIBFT_ARCHIVE_SUFFIX)_debug.a, $(LIB_BASES))
TEST_LIBS  := $(addsuffix $(LIBFT_ARCHIVE_SUFFIX)_test.a, $(LIB_BASES))
TOTAL_LIBS := $(words $(LIBS))
TOTAL_DEBUG_LIBS := $(words $(DEBUG_LIBS))
TOTAL_TEST_LIBS := $(words $(TEST_LIBS))

TARGET        := Full_Libft.a
DEBUG_TARGET  := Full_Libft_debug.a
TEST_TARGET   := Test/Full_Libft_test.a
TEST_DEBUG_TARGET := Test/Full_Libft_test_debug.a

CPP_CLASS_LIB := Modules/CPP_class/CPP_class$(LIBFT_ARCHIVE_SUFFIX).a

endif
