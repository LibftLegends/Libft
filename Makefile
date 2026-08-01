ifeq ($(OS),Windows_NT)
SHELL := C:/Progra~1/Git/usr/bin/bash.exe
.SHELLFLAGS := -lc
export SHELL
export LIBFT_POSIX_SHELL := 1
endif

include mk/compiler_flags.mk
include mk/build_config.mk

# Compiler-generated .d files cover source and header changes. Include a stable
# configuration fingerprint in child object paths so compiler/flag changes can
# never reuse incompatible objects.
MODULE_CONFIG_FINGERPRINT := $(shell printf '%s\n' '$(CXX)|$(COMPILE_FLAGS)|$(BUILD_OUTPUT_SUFFIX)' | cksum | awk '{print $$1}')
MODULE_BUILD_OUTPUT_SUFFIX := $(BUILD_OUTPUT_SUFFIX)_cfg$(MODULE_CONFIG_FINGERPRINT)

define PRINT_ORDERED_LOGS
	if [ "$(LIBFT_BATCH_OUTPUT)" = "1" ]; then \
		for lib in $(1); do \
			log_file="Test/.libft_build_$$(printf '%s' "$$lib" | tr '/.' '__').log"; \
			if [ -f "$$log_file" ]; then \
				cat "$$log_file"; \
				$(RM) "$$log_file"; \
			fi; \
		done; \
	fi
endef

TEST_PROGRESS_SESSION := $(shell date +%s%N)
TEST_PROGRESS_INIT := Test/.libft_progress/initialized.$(TEST_PROGRESS_SESSION)
BUILD_PROGRESS_INIT := Test/.libft_progress/build/initialized.$(TEST_PROGRESS_SESSION)
DEBUG_PROGRESS_INIT := Test/.libft_progress/debug/initialized.$(TEST_PROGRESS_SESSION)
BUILD_SCAN_INIT := Test/.libft_progress/build/scanning.$(TEST_PROGRESS_SESSION)
DEBUG_SCAN_INIT := Test/.libft_progress/debug/scanning.$(TEST_PROGRESS_SESSION)
BUILD_STALE_FILE := Test/.libft_progress/build/stale_modules
DEBUG_STALE_FILE := Test/.libft_progress/debug/stale_modules
sanitize_path = $(subst .,_,$(subst /,_,$(1)))
BUILD_CHECK_STAMPS := $(foreach lib,$(LIBS),Test/.libft_progress/build/checks/$(call sanitize_path,$(lib)).check)
DEBUG_CHECK_STAMPS := $(foreach lib,$(DEBUG_LIBS),Test/.libft_progress/debug/checks/$(call sanitize_path,$(lib)).check)

TEST_EXECUTABLE_MAKE_FLAGS :=

ssh:
	printf '\033[1;35m[LIBFT GIT] Switching GitHub remote to SSH\033[0m\n'
	git remote set-url origin git@github.com:Adyem/Libft.git
	git remote -v

all: $(TARGET) template

debug: $(DEBUG_TARGET)

both: all debug

demo:
	printf '\033[1;35m[LIBFT BUILD] Building Demo module\033[0m\n'
	$(MAKE) -C Demo all -B $(SUBMAKE_OVERRIDES) OPT_LEVEL=$(DEMO_OPT_LEVEL)

template: $(CPP_CLASS_LIB)
	printf '\033[1;35m[LIBFT BUILD] Running Template verification\033[0m\n'
	$(MAKE) -C Modules/Template all $(SUBMAKE_OVERRIDES)

tests: $(TEST_TARGET)
	@need_build=0; \
	if $(MAKE) -C Test -q all $(SUBMAKE_OVERRIDES); then \
		printf '\033[1;35m[LIBFT CHECK] Test suite is up to date\033[0m\n'; \
	else \
		status=$$?; \
		if [ $$status -eq 1 ]; then \
			need_build=1; \
		else \
			exit $$status; \
		fi; \
	fi; \
	if [ $$need_build -eq 1 ]; then \
		printf '\033[1;35m[LIBFT BUILD] Updating test suite\033[0m\n'; \
		batch_output=0; \
		case "$$MAKEFLAGS" in \
			*"-j1"*) batch_output=0 ;; \
			*"-j"*|*"--jobserver-auth="*) batch_output=1 ;; \
		esac; \
		$(MAKE) -C Test $(TEST_EXECUTABLE_MAKE_FLAGS) all $(SUBMAKE_OVERRIDES) LIBFT_TEST_BATCH_OUTPUT="$$batch_output"; \
	fi

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

ubsan: sanitize-clean
	$(MAKE) SANITIZERS=undefined all

ubsan-tests: sanitize-clean
	$(MAKE) SANITIZERS=undefined tests

asan-ubsan: sanitize-clean
	$(MAKE) SANITIZERS="address undefined" all

asan-ubsan-tests: sanitize-clean
	$(MAKE) SANITIZERS="address undefined" tests

re:
	$(MAKE) fclean
	$(MAKE) all

re-tests:
	$(MAKE) fclean
	$(MAKE) all
	$(MAKE) tests

$(TARGET): FORCE $(LIBS)
	@mk/progress.sh finish build
	@$(RM) $@
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		libtool -static -o "$@" $(LIBS); \
	else \
		{ printf 'CREATE %s\n' "$@"; \
		  for lib in $(LIBS); do printf 'ADDLIB %s\n' "$$lib"; done; \
		  printf 'SAVE\nEND\n'; } | $(AR) -M; \
	fi

$(DEBUG_TARGET): FORCE $(DEBUG_LIBS)
	@mk/progress.sh finish debug
	@$(RM) $@
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		libtool -static -o "$@" $(DEBUG_LIBS); \
	else \
		{ printf 'CREATE %s\n' "$@"; \
		  for lib in $(DEBUG_LIBS); do printf 'ADDLIB %s\n' "$$lib"; done; \
		  printf 'SAVE\nEND\n'; } | $(AR) -M; \
	fi

$(TEST_TARGET): FORCE $(TEST_LIBS)
	@mk/progress.sh finish test
	@$(RM) $@
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		libtool -static -o "$@" $(TEST_LIBS); \
	else \
		{ printf 'CREATE %s\n' "$@"; \
		  for lib in $(TEST_LIBS); do printf 'ADDLIB %s\n' "$$lib"; done; \
		  printf 'SAVE\nEND\n'; } | $(AR) -M; \
	fi

$(LIBFT_ROOT_DIR)/Test/Full_Libft_test.a: $(TEST_TARGET)

$(TEST_DEBUG_TARGET): FORCE $(DEBUG_LIBS)
	@mk/progress.sh finish debug
	@$(RM) $@
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		libtool -static -o "$@" $(DEBUG_LIBS); \
	else \
		{ printf 'CREATE %s\n' "$@"; \
		  for lib in $(DEBUG_LIBS); do printf 'ADDLIB %s\n' "$$lib"; done; \
		  printf 'SAVE\nEND\n'; } | $(AR) -M; \
	fi

$(LIBFT_ROOT_DIR)/Test/Full_Libft_test_debug.a: $(TEST_DEBUG_TARGET)

Modules/%_test.a: FORCE
Modules/%_test.a: | $(TEST_PROGRESS_INIT)
	@module_dir="$(patsubst %/,%,$(dir $@))"; \
	module_target="$(notdir $@)"; \
	module_path="$$module_dir/$$module_target"; \
	progress_index=$$(printf '%s\n' "$(TEST_LIBS)" | tr ' ' '\n' | nl -ba | awk -v target="$$module_path" '$$2==target {print $$1}'); \
	log_file="Test/.libft_build_$$(printf '%s' "$$module_path" | tr '/.' '__').log"; \
	batch_output=0; \
	case "$$MAKEFLAGS" in \
		*"-j1"*) batch_output=0 ;; \
		*"-j"*|*"--jobserver-auth="*) batch_output=1 ;; \
	esac; \
		LIBFT_BATCH_OUTPUT="$$batch_output" mk/run_module_build.sh "$(TOTAL_TEST_LIBS)" "$$progress_index" "$$module_path" "$$log_file" test -- env LIBFT_POSIX_SHELL=1 LIBFT_BATCH_OUTPUT="$$batch_output" $(MAKE) -C $$module_dir $$module_target $(SUBMAKE_OVERRIDES) TARGET="$$module_target" BUILD_OUTPUT_SUFFIX="$(MODULE_BUILD_OUTPUT_SUFFIX)" COMPILE_FLAGS="$(COMPILE_FLAGS) -DLIBFT_TEST_BUILD"; \
		status=$$?; \
		if [ $$status -ne 0 ]; then exit $$status; fi

$(TEST_PROGRESS_INIT):
	@mk/progress.sh init "$(TOTAL_TEST_LIBS)" test; \
	$(MKDIR) $(dir $@); \
	: > "$@"

define BUILD_CHECK_RULE
Test/.libft_progress/build/checks/$(call sanitize_path,$(1)).check: FORCE | $(BUILD_SCAN_INIT)
	+@$(MKDIR) $$(dir $$@); \
	module_path="$(1)"; module_dir="$$$${module_path%/*}"; module_target="$$$${module_path##*/}"; \
	if $$(MAKE) -C "$$$$module_dir" -q "$$$$module_target" $$(SUBMAKE_OVERRIDES) TARGET="$$$$module_target" BUILD_OUTPUT_SUFFIX="$$(MODULE_BUILD_OUTPUT_SUFFIX)" COMPILE_FLAGS="$$(COMPILE_FLAGS)"; then \
		: > "$$@"; \
	else \
		status=$$$$?; \
		if [ $$$$status -eq 1 ]; then printf '%s\n' "$$$$module_path" > "$$@"; else exit $$$$status; fi; \
	fi
endef

define DEBUG_CHECK_RULE
Test/.libft_progress/debug/checks/$(call sanitize_path,$(1)).check: FORCE | $(DEBUG_SCAN_INIT)
	+@$(MKDIR) $$(dir $$@); \
	module_path="$(1)"; module_dir="$$$${module_path%/*}"; module_target="$$$${module_path##*/}"; \
	if $$(MAKE) -C "$$$$module_dir" -q "$$$$module_target" $$(SUBMAKE_OVERRIDES) DEBUG_TARGET="$$$$module_target" BUILD_OUTPUT_SUFFIX="$$(MODULE_BUILD_OUTPUT_SUFFIX)" COMPILE_FLAGS="$$(COMPILE_FLAGS)"; then \
		: > "$$@"; \
	else \
		status=$$$$?; \
		if [ $$$$status -eq 1 ]; then printf '%s\n' "$$$$module_path" > "$$@"; else exit $$$$status; fi; \
	fi
endef

$(foreach lib,$(LIBS),$(eval $(call BUILD_CHECK_RULE,$(lib))))
$(foreach lib,$(DEBUG_LIBS),$(eval $(call DEBUG_CHECK_RULE,$(lib))))

$(BUILD_SCAN_INIT):
	@$(MKDIR) $(dir $@)
	@printf '\033[1;35m[LIBFT CHECK]\033[0m Scanning %d modules for stale work...\n' "$(TOTAL_LIBS)"
	@: > "$@"

$(DEBUG_SCAN_INIT):
	@$(MKDIR) $(dir $@)
	@printf '\033[1;35m[LIBFT CHECK]\033[0m Scanning %d debug modules for stale work...\n' "$(TOTAL_DEBUG_LIBS)"
	@: > "$@"

$(BUILD_PROGRESS_INIT): $(BUILD_CHECK_STAMPS)
	@$(MKDIR) $(dir $@); \
	: > "$(BUILD_STALE_FILE)"; \
	for check_file in $(BUILD_CHECK_STAMPS); do \
		if [ -s "$$check_file" ]; then cat "$$check_file" >> "$(BUILD_STALE_FILE)"; fi; \
	done; \
	total=$$(wc -l < "$(BUILD_STALE_FILE)" | tr -d ' '); \
	mk/progress.sh init "$$total" build; \
	printf '%s\n' "$$total" > "$(dir $(BUILD_STALE_FILE))total"; \
	: > "$@"

$(DEBUG_PROGRESS_INIT): $(DEBUG_CHECK_STAMPS)
	@$(MKDIR) $(dir $@); \
	: > "$(DEBUG_STALE_FILE)"; \
	for check_file in $(DEBUG_CHECK_STAMPS); do \
		if [ -s "$$check_file" ]; then cat "$$check_file" >> "$(DEBUG_STALE_FILE)"; fi; \
	done; \
	total=$$(wc -l < "$(DEBUG_STALE_FILE)" | tr -d ' '); \
	mk/progress.sh init "$$total" debug; \
	printf '%s\n' "$$total" > "$(dir $(DEBUG_STALE_FILE))total"; \
	: > "$@"

Modules/%.a: FORCE | $(BUILD_PROGRESS_INIT)
	@module_dir="$(patsubst %/,%,$(dir $@))"; \
	module_target="$(notdir $@)"; \
	if grep -Fqx "$@" "$(BUILD_STALE_FILE)"; then \
		module_path="$$module_dir/$$module_target"; \
		progress_index=$$(nl -ba "$(BUILD_STALE_FILE)" | awk -v target="$$module_path" '$$2==target {print $$1}'); \
		progress_total=$$(cat "$(dir $(BUILD_STALE_FILE))total"); \
		log_file="Test/.libft_build_$$(printf '%s' "$$module_path" | tr '/.' '__').log"; \
		batch_output=0; \
		case "$$MAKEFLAGS" in \
			*"-j1"*) batch_output=0 ;; \
			*"-j"*|*"--jobserver-auth="*) batch_output=1 ;; \
		esac; \
		LIBFT_BATCH_OUTPUT="$$batch_output" mk/run_module_build.sh "$$progress_total" "$$progress_index" "$$module_path" "$$log_file" build -- env LIBFT_POSIX_SHELL=1 LIBFT_BATCH_OUTPUT="$$batch_output" $(MAKE) -C $$module_dir $$module_target $(SUBMAKE_OVERRIDES) TARGET="$$module_target" BUILD_OUTPUT_SUFFIX="$(MODULE_BUILD_OUTPUT_SUFFIX)" COMPILE_FLAGS="$(COMPILE_FLAGS)"; \
		status=$$?; \
		if [ $$status -ne 0 ]; then exit $$status; fi; \
	fi

Modules/%_debug.a: FORCE | $(DEBUG_PROGRESS_INIT)
	@module_dir="$(patsubst %/,%,$(dir $@))"; \
	module_target="$(notdir $@)"; \
	if grep -Fqx "$@" "$(DEBUG_STALE_FILE)"; then \
		module_path="$$module_dir/$$module_target"; \
		progress_index=$$(nl -ba "$(DEBUG_STALE_FILE)" | awk -v target="$$module_path" '$$2==target {print $$1}'); \
		progress_total=$$(cat "$(dir $(DEBUG_STALE_FILE))total"); \
		log_file="Test/.libft_build_$$(printf '%s' "$$module_path" | tr '/.' '__').log"; \
		batch_output=0; \
		case "$$MAKEFLAGS" in \
			*"-j1"*) batch_output=0 ;; \
			*"-j"*|*"--jobserver-auth="*) batch_output=1 ;; \
		esac; \
		LIBFT_BATCH_OUTPUT="$$batch_output" mk/run_module_build.sh "$$progress_total" "$$progress_index" "$$module_path" "$$log_file" debug -- env LIBFT_POSIX_SHELL=1 LIBFT_BATCH_OUTPUT="$$batch_output" $(MAKE) -C $$module_dir $$module_target $(SUBMAKE_OVERRIDES) DEBUG_TARGET="$$module_target" BUILD_OUTPUT_SUFFIX="$(MODULE_BUILD_OUTPUT_SUFFIX)" COMPILE_FLAGS="$(COMPILE_FLAGS)"; \
		status=$$?; \
		if [ $$status -ne 0 ]; then exit $$status; fi; \
	fi

clean:
	@status=0; \
	for dir in $(SUBDIRS); do \
		if ! $(MAKE) -C $$dir clean $(SUBMAKE_OVERRIDES); then \
			status=1; \
		fi; \
	done; \
	if ! $(MAKE) -C Demo clean $(SUBMAKE_OVERRIDES) LIBFT_PARENT_CLEAN=1; then \
		status=1; \
	fi; \
	if ! $(MAKE) -C Test clean $(SUBMAKE_OVERRIDES) LIBFT_PARENT_CLEAN=1; then \
		status=1; \
	fi; \
	if ! $(RMDIR) $(TEMP_DIRS); then \
		status=1; \
	fi; \
		if ! $(RM) $(TARGET) $(DEBUG_TARGET) $(TEST_TARGET) $(TEST_DEBUG_TARGET) $(OUTPUT_LOGS); then \
			status=1; \
		fi; \
		if [ $$status -eq 0 ]; then \
			printf '\033[1;35m[LIBFT CLEAN] Success\033[0m\n'; \
		else \
			printf '\033[1;35m[LIBFT CLEAN] Failed\033[0m\n'; \
			exit 1; \
		fi

fclean:
	@status=0; \
	for dir in $(SUBDIRS); do \
		if ! $(MAKE) -C $$dir fclean $(SUBMAKE_OVERRIDES); then \
			status=1; \
		fi; \
	done; \
	if ! $(MAKE) -C Demo fclean $(SUBMAKE_OVERRIDES) LIBFT_PARENT_CLEAN=1; then \
		status=1; \
	fi; \
	if ! $(MAKE) -C Test fclean $(SUBMAKE_OVERRIDES) LIBFT_PARENT_CLEAN=1; then \
		status=1; \
	fi; \
	if ! $(RMDIR) $(TEMP_DIRS); then \
		status=1; \
	fi; \
		if ! $(RM) $(TARGET) $(DEBUG_TARGET) $(TEST_TARGET) $(TEST_DEBUG_TARGET) $(OUTPUT_LOGS); then \
			status=1; \
		fi; \
		if [ $$status -eq 0 ]; then \
			printf '\033[1;35m[LIBFT FCLEAN] Success\033[0m\n'; \
		else \
			printf '\033[1;35m[LIBFT FCLEAN] Failed\033[0m\n'; \
			exit 1; \
		fi

.PHONY: all debug both template demo re re-tests clean fclean tests print-build-mode format sanitize-clean \
        asan asan-tests ubsan ubsan-tests asan-ubsan asan-ubsan-tests FORCE

FORCE:
