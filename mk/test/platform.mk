TOTAL_SRCS := $(words $(SRCS))

MAKEFLAGS += --no-print-directory

LIBFT_TEST_PARALLEL_JOBS = $(filter -j%,$(MAKEFLAGS))
LIBFT_TEST_JOBSERVER = $(filter --jobserver-auth=%,$(MAKEFLAGS))
LIBFT_TEST_BATCH_OUTPUT = 0
ifneq ($(LIBFT_TEST_JOBSERVER),)
    LIBFT_TEST_BATCH_OUTPUT = 1
else ifneq ($(LIBFT_TEST_PARALLEL_JOBS),)
    ifneq ($(LIBFT_TEST_PARALLEL_JOBS),-j1)
        LIBFT_TEST_BATCH_OUTPUT = 1
    endif
endif

LIBFT_TEST_LIGHT_BLUE := \033[1;94m
LIBFT_TEST_PURPLE := \033[1;35m
LIBFT_TEST_COLOR_OFF := \033[0m

ifeq ($(OS),Windows_NT)
    ifneq ($(LIBFT_POSIX_SHELL),)
        MKDIR = mkdir -p
        RM    = rm -f
        RMDIR = rm -rf
    else
        MKDIR = mkdir
        RM    = del /F /Q
        RMDIR = rmdir /S /Q
    endif
else
    MKDIR = mkdir -p
    RM    = rm -f
    RMDIR = rm -rf
endif
