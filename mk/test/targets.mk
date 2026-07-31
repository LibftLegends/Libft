TARGET := libft_tests
DEBUG_TARGET := libft_tests_debug
EFFICIENCY_TARGET := libft_efficiency_tests
ifeq ($(OS),Windows_NT)
TARGET := libft_tests.exe
DEBUG_TARGET := libft_tests_debug.exe
EFFICIENCY_TARGET := libft_efficiency_tests.exe
endif
EFFICIENCY_OPT_LEVEL ?= 3
EFFICIENCY_ARCHIVE_SUFFIX := _opt$(EFFICIENCY_OPT_LEVEL)
LIBFT_ARCHIVE := $(LIBFT_ROOT_DIR)/Test/Full_Libft_test.a
LIBFT_DEBUG_ARCHIVE := $(LIBFT_ROOT_DIR)/Test/Full_Libft_test_debug.a
EFFICIENCY_LIBFT_RELATIVE_ARCHIVES := \
	Modules/Basic/Basic$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/Compatebility/Compatebility$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/Debug/Debug$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/Errno/errno$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/CMA/CustomMemoryAllocator$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/SCMA/SCMA$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/System_utils/System_utils$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/Printf/Printf$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/PThread/PThread$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/CPP_class/CPP_class$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/Time/time$(EFFICIENCY_ARCHIVE_SUFFIX).a \
	Modules/Sink/Sink$(EFFICIENCY_ARCHIVE_SUFFIX).a
EFFICIENCY_LIBFT_ARCHIVES := $(addprefix $(LIBFT_ROOT_DIR)/,$(EFFICIENCY_LIBFT_RELATIVE_ARCHIVES))
EFFICIENCY_MODULE_DIRS := $(sort $(dir $(EFFICIENCY_LIBFT_RELATIVE_ARCHIVES)))
EFFICIENCY_ARCHIVE_SOURCE_FILES := $(shell find \
	$(addprefix $(LIBFT_ROOT_DIR)/,$(EFFICIENCY_MODULE_DIRS)) \
	$(LIBFT_ROOT_DIR)/Modules/Template \
	-type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.ipp' \
	-o -name 'Makefile' \) 2>/dev/null) \
	$(wildcard $(LIBFT_ROOT_DIR)/mk/modules/*.mk) \
	$(wildcard $(LIBFT_ROOT_DIR)/mk/modules/common/*.mk) \
	$(LIBFT_ROOT_DIR)/mk/compiler_flags.mk \
	$(LIBFT_ROOT_DIR)/mk/build_config.mk
EFFICIENCY_ARCHIVE_STAMP = $(EFFICIENCY_OBJDIR)/.archives_ready
