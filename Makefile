Exit code: 0
Wall time: 5.3 seconds
Output:
ifeq ($(OS),Windows_NT)
SHELL := C:/Progra~1/Git/usr/bin/bash.exe
.SHELLFLAGS := -lc
export SHELL
export LIBFT_POSIX_SHELL := 1
endif

MAKEFLAGS += -r
# BUILD_PLAN_MODE=1 is used by the stale-work planning wrapper.  Recipes emit
# machine-readable markers in that mode and concise status lines otherwise.
BUILD_PLAN_MODE ?= 0
BUILD_PROGRESS_ACTIVE ?= 0
BUILD_PROGRESS_SESSION_DIR ?=
# The graph uses GNU Make features available in 3.81. Output synchronization
# remains an optional command-line feature for newer Make installations.
LIBFT_SUPPORTED_MAKE_VERSION := $(filter 3.81 3.82 4.% 5.% 6.% 7.% 8.% 9.%,$(MAKE_VERSION))
ifeq ($(LIBFT_SUPPORTED_MAKE_VERSION),)
$(error GNU Make 3.81 or newer is required; found $(MAKE_VERSION))
endif
LIBFT_GLOBAL_GRAPH := 1
include mk/compiler_flags.mk
include mk/build_config.mk
LIBFT_ROOT_TARGET := $(TARGET)
LIBFT_ROOT_DEBUG_TARGET := $(DEBUG_TARGET)
LIBFT_ROOT_TEST_TARGET := $(TEST_TARGET)
LIBFT_ROOT_TEST_DEBUG_TARGET := $(TEST_DEBUG_TARGET)
include mk/global_graph.mk

TARGET := $(LIBFT_ROOT_TARGET)
DEBUG_TARGET := $(LIBFT_ROOT_DEBUG_TARGET)
TEST_TARGET := $(LIBFT_ROOT_TEST_TARGET)
TEST_DEBUG_TARGET := $(LIBFT_ROOT_TEST_DEBUG_TARGET)

LIBFT_GLOBAL_TARGET := $(TARGET)
LIBFT_GLOBAL_DEBUG_TARGET := $(DEBUG_TARGET)
LIBFT_GLOBAL_TEST_TARGET := $(TEST_TARGET)
LIBFT_GLOBAL_TEST_DEBUG_TARGET := $(TEST_DEBUG_TARGET)
LIBFT_LEGACY_OBJECT_ROOTS := $(wildcard Modules/*/objs_* Test/objs_*)

define LIBFT_GLOBAL_ARCHIVE_RULE
$(1): $(2) $(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS)
	@if [ "$$(BUILD_PLAN_MODE)" = "1" ]; then printf '%s\n' "__BUILD_PLAN__|archive|libft|Full_Libft|$(1)"; else printf '\033[1;35m[LIBFT] Archiving %s\033[0m\n' "$(1)"; fi
	@$(MKDIR) $(dir $$@)
	@$(RM) $$@.tmp
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		libtool -static -o "$$@.tmp" $(2); \
	else \
		{ printf 'CREATE %s\n' "$$@.tmp"; \
		  for lib in $(2); do printf 'ADDLIB %s\n' "$$$$lib"; done; \
		  printf 'SAVE\nEND\n'; } | $(AR) -M; \
	fi
	@mv $$@.tmp $$@
	@printf '\033[1;35m[LIBFT] Archive ready: %s\033[0m\n' "$(1)"
	@if [ "$$(BUILD_PROGRESS_ACTIVE)" = "1" ]; then \
		sh mk/update_build_progress.sh "$$(BUILD_PROGRESS_SESSION_DIR)" archive libft Full_Libft || true; \
	fi
endef

$(eval $(call LIBFT_GLOBAL_ARCHIVE_RULE,$(LIBFT_GLOBAL_TARGET),$(LIBFT_GLOBAL_RELEASE_ARCHIVES)))
$(eval $(call LIBFT_GLOBAL_ARCHIVE_RULE,$(LIBFT_GLOBAL_DEBUG_TARGET),$(LIBFT_GLOBAL_DEBUG_ARCHIVES)))
$(eval $(call LIBFT_GLOBAL_ARCHIVE_RULE,$(LIBFT_GLOBAL_TEST_TARGET),$(LIBFT_GLOBAL_TEST_ARCHIVES)))
$(eval $(call LIBFT_GLOBAL_ARCHIVE_RULE,$(LIBFT_GLOBAL_TEST_DEBUG_TARGET),$(LIBFT_GLOBAL_TEST_DEBUG_ARCHIVES)))
include mk/global_test_graph.mk
include mk/global_efficiency_graph.mk

ssh:
	printf '\033[1;35m[LIBFT GIT] Switching GitHub remote to SSH\033[0m\n'
	git remote set-url origin git@github.com:Adyem/Libft.git
	git remote -v

all:
	@sh mk/run_build_with_progress.sh "$(MAKE)" internal-all

plan:
	@sh mk/print_build_plan.sh "$(MAKE)" internal-all

internal-all: $(LIBFT_GLOBAL_TARGET) $(LIBFT_GLOBAL_Template_TARGET)

global-all: $(LIBFT_GLOBAL_TARGET)

global-debug: $(LIBFT_GLOBAL_DEBUG_TARGET)

global-tests: $(LIBFT_GLOBAL_TEST_TARGET)

debug:
	@sh mk/run_build_with_progress.sh "$(MAKE)" internal-debug

internal-debug: $(LIBFT_GLOBAL_DEBUG_TARGET)

both: all debug

demo:
	printf '\033[1;35m[LIBFT BUILD] Building Demo module\033[0m\n'
	$(MAKE) -C Demo all -B $(SUBMAKE_OVERRIDES) OPT_LEVEL=$(DEMO_OPT_LEVEL)

template: $(LIBFT_GLOBAL_Template_TARGET)
	@printf '\033[1;35m[LIBFT BUILD] Template archive ready\033[0m\n'

tests:
	@sh mk/run_build_with_progress.sh "$(MAKE)" internal-tests
	@printf '\033[1;35m[LIBFT BUILD] Test executable ready (%s)\033[0m\n' "$(LIBFT_GLOBAL_TEST_EXECUTABLE)"

internal-tests: $(LIBFT_GLOBAL_TEST_EXECUTABLE)

test-executable: $(LIBFT_GLOBAL_TEST_EXECUTABLE)

run-tests: $(LIBFT_GLOBAL_TEST_EXECUTABLE)
	@cd Test && ./$(notdir $(LIBFT_GLOBAL_TEST_EXECUTABLE))

debug-tests: $(LIBFT_GLOBAL_TEST_DEBUG_EXECUTABLE)

run-debug-tests: $(LIBFT_GLOBAL_TEST_DEBUG_EXECUTABLE)
	@cd Test && ./$(notdir $(LIBFT_GLOBAL_TEST_DEBUG_EXECUTABLE))

performance_benchmarks Efficiency:
	@sh mk/run_build_with_progress.sh "$(MAKE)" internal-performance
	@printf '\033[1;35m[LIBFT BUILD] Efficiency executable ready (%s)\033[0m\n' "$(LIBFT_GLOBAL_EFFICIENCY_EXECUTABLE)"

internal-performance: $(LIBFT_GLOBAL_EFFICIENCY_EXECUTABLE)

LIBFT_GLOBAL_AGGREGATE_RELEASE_OBJECTS := $(foreach module_name,$(LIBFT_GLOBAL_ARCHIVE_MODULE_NAMES),$(LIBFT_GLOBAL_$(module_name)_RELEASE_OBJECTS))
LIBFT_ARCHIVE_INTEGRITY_TARGETS := $(foreach module_name,$(LIBFT_GLOBAL_ARCHIVE_MODULE_NAMES),archive-integrity-$(module_name)) archive-integrity-Full_Libft
LIBFT_ARCHIVE_NOOP_STATE := $(LIBFT_GLOBAL_ROOT)/archive-noop-timestamps

define LIBFT_DEFINE_ARCHIVE_INTEGRITY_RULE
archive-integrity-$(1): $(2)
	@bash mk/check_archive_integrity.sh "$(2)" $(3)
endef

$(foreach module_name,$(LIBFT_GLOBAL_ARCHIVE_MODULE_NAMES),$(eval $(call LIBFT_DEFINE_ARCHIVE_INTEGRITY_RULE,$(module_name),$(LIBFT_GLOBAL_$(module_name)_TARGET),$(LIBFT_GLOBAL_$(module_name)_RELEASE_OBJECTS))))
$(eval $(call LIBFT_DEFINE_ARCHIVE_INTEGRITY_RULE,Full_Libft,$(LIBFT_GLOBAL_TARGET),--from-archives $(LIBFT_GLOBAL_RELEASE_ARCHIVES)))

archive-integrity: $(LIBFT_ARCHIVE_INTEGRITY_TARGETS)
	@sh mk/check_archive_rebuild.sh "$(LIBFT_GLOBAL_Basic_TARGET)" "$(LIBFT_GLOBAL_Basic_MANIFEST)" $(LIBFT_GLOBAL_Basic_RELEASE_OBJECTS)
	+$(MAKE) --no-print-directory global-all
	@sh mk/check_archive_noop.sh capture "$(LIBFT_ARCHIVE_NOOP_STATE)" $(LIBFT_GLOBAL_RELEASE_ARCHIVES) $(LIBFT_GLOBAL_TARGET)
	+$(MAKE) --no-print-directory global-all
	@sh mk/check_archive_noop.sh verify "$(LIBFT_ARCHIVE_NOOP_STATE)"
	@$(RM) "$(LIBFT_ARCHIVE_NOOP_STATE)"
	@printf '\033[1;35m[LIBFT BUILD] Archive integrity checks passed\033[0m\n'

run_performance_benchmarks run_Efficiency: $(LIBFT_GLOBAL_EFFICIENCY_EXECUTABLE)
	@./$(LIBFT_GLOBAL_EFFICIENCY_EXECUTABLE)

print-build-mode:
	printf "MAKEFLAGS=%s\n" "$(MAKEFLAGS)"
	printf "LIBFT_PARALLEL_JOBS=%s\n" "$(LIBFT_PARALLEL_JOBS)"
	printf "LIBFT_JOBSERVER=%s\n" "$(LIBFT_JOBSERVER)"
	printf "LIBFT_BATCH_OUTPUT=%s\n" "$(LIBFT_BATCH_OUTPUT)"

format:
	if ! command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		echo "Error: clang-format not found."; \
		exit 1; \
	fi
	files="$$(git ls-files '*.cpp' '*.hpp' '*.ipp')"; \
	if [ -n "$$files" ]; then \
		printf '%s\n' "$$files" | tr '\n' '\0' | xargs -0 $(CLANG_FORMAT) --style=file -i; \
	else \
		echo "No source files to format."; \
	fi

sanitize-clean:
	$(MAKE) clean

asan: sanitize-clean
	$(MAKE) SANITIZERS=address all

asan-tests: sanitize-clean
	$(MAKE) SANITIZERS=address tests

run-asan-tests: asan-tests
	@cd Test && ./$(notdir $(LIBFT_GLOBAL_TEST_EXECUTABLE))

ubsan: sanitize-clean
	$(MAKE) SANITIZERS=undefined all

ubsan-tests: sanitize-clean
	$(MAKE) SANITIZERS=undefined tests

run-ubsan-tests: ubsan-tests
	@cd Test && ./$(notdir $(LIBFT_GLOBAL_TEST_EXECUTABLE))

asan-ubsan: sanitize-clean
	$(MAKE) SANITIZERS="address undefined" all

asan-ubsan-tests: sanitize-clean
	$(MAKE) SANITIZERS="address undefined" tests

run-asan-ubsan-tests: asan-ubsan-tests
	@cd Test && ./$(notdir $(LIBFT_GLOBAL_TEST_EXECUTABLE))

re:
	$(MAKE) fclean
	$(MAKE) all

re-tests:
	$(MAKE) fclean
	$(MAKE) all
	$(MAKE) tests


clean:
	@$(RMDIR) "$(LIBFT_GLOBAL_ROOT)"
	@for legacy_object_root in $(LIBFT_LEGACY_OBJECT_ROOTS); do $(RMDIR) "$$legacy_object_root"; done
	@$(RM) $(LIBFT_GLOBAL_RELEASE_ARCHIVES) $(LIBFT_GLOBAL_DEBUG_ARCHIVES) \
		$(LIBFT_GLOBAL_TEST_ARCHIVES) $(LIBFT_GLOBAL_TARGET) \
		$(LIBFT_GLOBAL_DEBUG_TARGET) $(LIBFT_GLOBAL_TEST_TARGET) \
		$(LIBFT_GLOBAL_TEST_EXECUTABLE) $(LIBFT_GLOBAL_TEST_DEBUG_EXECUTABLE) \
		Test/libft_test_objects.a Test/libft_test_debug_objects.a \
		$(LIBFT_GLOBAL_Template_TARGET)

fclean:
	@$(RMDIR) build/libft
	@for legacy_object_root in $(LIBFT_LEGACY_OBJECT_ROOTS); do $(RMDIR) "$$legacy_object_root"; done
	@$(RM) $(LIBFT_GLOBAL_RELEASE_ARCHIVES) $(LIBFT_GLOBAL_DEBUG_ARCHIVES) \
		$(LIBFT_GLOBAL_TEST_ARCHIVES) $(LIBFT_GLOBAL_TARGET) \
		$(LIBFT_GLOBAL_DEBUG_TARGET) $(LIBFT_GLOBAL_TEST_TARGET) \
		$(LIBFT_GLOBAL_TEST_EXECUTABLE) $(LIBFT_GLOBAL_TEST_DEBUG_EXECUTABLE) \
		$(LIBFT_GLOBAL_Template_TARGET)

.PHONY: all plan internal-all internal-debug global-all global-debug global-tests debug both template demo re re-tests tests internal-tests test-executable run-tests debug-tests performance_benchmarks Efficiency internal-performance run_performance_benchmarks run_Efficiency archive-integrity print-build-mode format sanitize-clean \
        run-debug-tests run-asan-tests run-ubsan-tests run-asan-ubsan-tests \
        asan asan-tests ubsan ubsan-tests asan-ubsan asan-ubsan-tests
