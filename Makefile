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
$(1): $(2) $(LIBFT_GLOBAL_ARCHIVE_CONFIG_INPUTS) $(LIBFT_GLOBAL_GRAPH_PREFIX)mk/global_graph.mk
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
	@if [ "$$(BUILD_PROGRESS_ACTIVE)" = "1" ]; then \
		sh mk/update_build_progress.sh "$$(BUILD_PROGRESS_SESSION_DIR)" archive libft Full_Libft "$(1)" || true; \
	else \
		printf '\033[1;35m[LIBFT] Archive ready: %s\033[0m\n' "$(1)"; \
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

all: normal analytics

normal:
	@sh mk/run_build_with_progress.sh "$(MAKE)" internal-all

analytics:
	@$(MAKE) --no-print-directory FT_VOX_ANALYTICS=1 BUILD_OUTPUT_SUFFIX=_analytics \
		LIBFT_ARCHIVE_SUFFIX=_analytics TARGET=Full_Libft_analytics.a \
		DEBUG_TARGET=Full_Libft_analytics_debug.a \
		COMPILE_FLAGS="$(COMPILE_FLAGS) -DLIBFT_ENABLE_ANALYTICS=1" internal-all

analytics-debug:
	@$(MAKE) --no-print-directory FT_VOX_ANALYTICS=1 BUILD_OUTPUT_SUFFIX=_analytics \
		LIBFT_ARCHIVE_SUFFIX=_analytics TARGET=Full_Libft_analytics.a \
		DEBUG_TARGET=Full_Libft_analytics_debug.a \
		COMPILE_FLAGS="$(COMPILE_FLAGS) -DLIBFT_ENABLE_ANALYTICS=1" internal-debug

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

FUZZ_CXX ?= clang++
FUZZ_SANITIZER_FLAGS ?= -fsanitize=fuzzer,address,undefined
LIBFT_NETWORKING_FUZZ_EXECUTABLE := Test/networking_fuzz
LIBFT_CRYPTO_FUZZ_EXECUTABLE := Test/crypto_fuzz
LIBFT_CRYPTO_SHA_FUZZ_EXECUTABLE := Test/crypto_sha_fuzz
LIBFT_CRYPTO_AEAD_FUZZ_EXECUTABLE := Test/crypto_aead_fuzz
LIBFT_CRYPTO_X25519_FUZZ_EXECUTABLE := Test/crypto_x25519_fuzz
LIBFT_CRYPTO_HMAC_HKDF_FUZZ_EXECUTABLE := Test/crypto_hmac_hkdf_fuzz
LIBFT_CRYPTO_X25519_MILLION_EXECUTABLE := Test/crypto_x25519_million
LIBFT_NETWORKING_SIMULATOR_FUZZ_EXECUTABLE := Test/networking_simulator_fuzz
LIBFT_NETWORKING_NAT_FUZZ_EXECUTABLE := Test/networking_nat_fuzz
LIBFT_NETWORKING_HANDSHAKE_FUZZ_EXECUTABLE := Test/networking_handshake_fuzz
FUZZ_COMPILE_FLAGS := $(filter-out -Wuseless-cast -Wmaybe-uninitialized,$(LIBFT_GLOBAL_TEST_CXX_FLAGS))
FUZZ_TEST_HOOK_SOURCES := Test/Test/networking_test_hooks.cpp \
	Test/Test/crypto_test_hooks.cpp
ifeq ($(OS),Windows_NT)
LIBFT_FUZZ_LINK_FLAGS := $(LINK_OPT_FLAGS) -lws2_32 -lgdi32 -lwinmm -ldbghelp -lopengl32
else
LIBFT_FUZZ_LINK_FLAGS := $(LIBFT_GLOBAL_TEST_LINK_FLAGS)
endif

networking-fuzz: $(LIBFT_GLOBAL_TEST_TARGET) Test/Fuzz/networking_fuzz_target.cpp
	@$(MKDIR) "$(dir $(LIBFT_NETWORKING_FUZZ_EXECUTABLE))"
	@$(FUZZ_CXX) $(FUZZ_COMPILE_FLAGS) -I. $(FUZZ_SANITIZER_FLAGS) \
		-o $(LIBFT_NETWORKING_FUZZ_EXECUTABLE) \
		Test/Fuzz/networking_fuzz_target.cpp \
		$(FUZZ_TEST_HOOK_SOURCES) \
		$(LIBFT_GLOBAL_TEST_TARGET) $(LIBFT_FUZZ_LINK_FLAGS)
	@printf '\033[1;35m[LIBFT][Fuzz] Ready: %s\033[0m\n' \
		"$(LIBFT_NETWORKING_FUZZ_EXECUTABLE)"

crypto-fuzz: $(LIBFT_GLOBAL_TEST_TARGET) Test/Fuzz/crypto_fuzz_target.cpp
	@$(MKDIR) "$(dir $(LIBFT_CRYPTO_FUZZ_EXECUTABLE))"
	@$(FUZZ_CXX) $(FUZZ_COMPILE_FLAGS) -I. $(FUZZ_SANITIZER_FLAGS) \
		-o $(LIBFT_CRYPTO_FUZZ_EXECUTABLE) \
		Test/Fuzz/crypto_fuzz_target.cpp \
		$(FUZZ_TEST_HOOK_SOURCES) \
		$(LIBFT_GLOBAL_TEST_TARGET) $(LIBFT_FUZZ_LINK_FLAGS)
	@printf '\033[1;35m[LIBFT][Fuzz] Ready: %s\033[0m\n' \
		"$(LIBFT_CRYPTO_FUZZ_EXECUTABLE)"

crypto-sha-fuzz: $(LIBFT_GLOBAL_TEST_TARGET) Test/Fuzz/crypto_sha_fuzz_target.cpp
	@$(FUZZ_CXX) $(FUZZ_COMPILE_FLAGS) -I. $(FUZZ_SANITIZER_FLAGS) \
		-o $(LIBFT_CRYPTO_SHA_FUZZ_EXECUTABLE) Test/Fuzz/crypto_sha_fuzz_target.cpp \
		$(FUZZ_TEST_HOOK_SOURCES) \
		$(LIBFT_GLOBAL_TEST_TARGET) $(LIBFT_FUZZ_LINK_FLAGS)

crypto-aead-fuzz: $(LIBFT_GLOBAL_TEST_TARGET) Test/Fuzz/crypto_aead_fuzz_target.cpp
	@$(FUZZ_CXX) $(FUZZ_COMPILE_FLAGS) -I. $(FUZZ_SANITIZER_FLAGS) \
		-o $(LIBFT_CRYPTO_AEAD_FUZZ_EXECUTABLE) Test/Fuzz/crypto_aead_fuzz_target.cpp \
		$(FUZZ_TEST_HOOK_SOURCES) \
		$(LIBFT_GLOBAL_TEST_TARGET) $(LIBFT_FUZZ_LINK_FLAGS)

crypto-x25519-fuzz: $(LIBFT_GLOBAL_TEST_TARGET) Test/Fuzz/crypto_x25519_fuzz_target.cpp
	@$(FUZZ_CXX) $(FUZZ_COMPILE_FLAGS) -I. $(FUZZ_SANITIZER_FLAGS) \
		-o $(LIBFT_CRYPTO_X25519_FUZZ_EXECUTABLE) Test/Fuzz/crypto_x25519_fuzz_target.cpp \
		$(FUZZ_TEST_HOOK_SOURCES) \
		$(LIBFT_GLOBAL_TEST_TARGET) $(LIBFT_FUZZ_LINK_FLAGS)

crypto-hmac-hkdf-fuzz: $(LIBFT_GLOBAL_TEST_TARGET) Test/Fuzz/crypto_hmac_hkdf_fuzz_target.cpp
	@$(FUZZ_CXX) $(FUZZ_COMPILE_FLAGS) -I. $(FUZZ_SANITIZER_FLAGS) \
		-o $(LIBFT_CRYPTO_HMAC_HKDF_FUZZ_EXECUTABLE) \
		Test/Fuzz/crypto_hmac_hkdf_fuzz_target.cpp \
		$(FUZZ_TEST_HOOK_SOURCES) \
		$(LIBFT_GLOBAL_TEST_TARGET) $(LIBFT_FUZZ_LINK_FLAGS)

networking-simulator-fuzz: $(LIBFT_GLOBAL_TEST_TARGET) Test/Fuzz/networking_simulator_fuzz_target.cpp
	@$(FUZZ_CXX) $(FUZZ_COMPILE_FLAGS) -I. $(FUZZ_SANITIZER_FLAGS) \
		-o $(LIBFT_NETWORKING_SIMULATOR_FUZZ_EXECUTABLE) \
		Test/Fuzz/networking_simulator_fuzz_target.cpp \
		$(FUZZ_TEST_HOOK_SOURCES) \
		$(LIBFT_GLOBAL_TEST_TARGET) $(LIBFT_FUZZ_LINK_FLAGS)

networking-nat-fuzz: $(LIBFT_GLOBAL_TEST_TARGET) Test/Fuzz/networking_nat_fuzz_target.cpp
	@$(MKDIR) "$(dir $(LIBFT_NETWORKING_NAT_FUZZ_EXECUTABLE))"
	@$(FUZZ_CXX) $(FUZZ_COMPILE_FLAGS) -I. $(FUZZ_SANITIZER_FLAGS) \
		-o $(LIBFT_NETWORKING_NAT_FUZZ_EXECUTABLE) \
		Test/Fuzz/networking_nat_fuzz_target.cpp \
		$(FUZZ_TEST_HOOK_SOURCES) \
		$(LIBFT_GLOBAL_TEST_TARGET) $(LIBFT_FUZZ_LINK_FLAGS)

networking-handshake-fuzz: $(LIBFT_GLOBAL_TEST_TARGET) Test/Fuzz/networking_handshake_fuzz_target.cpp
	@$(MKDIR) "$(dir $(LIBFT_NETWORKING_HANDSHAKE_FUZZ_EXECUTABLE))"
	@$(FUZZ_CXX) $(FUZZ_COMPILE_FLAGS) -I. $(FUZZ_SANITIZER_FLAGS) \
		-o $(LIBFT_NETWORKING_HANDSHAKE_FUZZ_EXECUTABLE) \
		Test/Fuzz/networking_handshake_fuzz_target.cpp \
		$(FUZZ_TEST_HOOK_SOURCES) \
		$(LIBFT_GLOBAL_TEST_TARGET) $(LIBFT_FUZZ_LINK_FLAGS)

crypto-x25519-million: Modules/Crypto/crypto.a Modules/Basic/Basic.a \
		Test/Slow/crypto_x25519_million_runner.cpp
	@$(MKDIR) "$(dir $(LIBFT_CRYPTO_X25519_MILLION_EXECUTABLE))"
	@$(CXX) $(COMPILE_FLAGS) -I. -o $(LIBFT_CRYPTO_X25519_MILLION_EXECUTABLE) \
		Test/Slow/crypto_x25519_million_runner.cpp \
		Modules/Crypto/crypto.a Modules/Basic/Basic.a $(LINK_OPT_FLAGS)

terrain-persistence-tests: $(LIBFT_GLOBAL_TEST_EXECUTABLE)
	@cd Test && FT_TEST_NAME_FILTER="test_voxel_json_unsigned_boundaries_and_transaction,test_voxel_json_file_failure_hooks_are_transactional" ./$(notdir $(LIBFT_GLOBAL_TEST_EXECUTABLE))

crypto-tests: $(LIBFT_GLOBAL_TEST_EXECUTABLE)
	@cd Test && FT_TEST_NAME_FILTER="test_crypto_" ./$(notdir $(LIBFT_GLOBAL_TEST_EXECUTABLE))

networking-message-tests: $(LIBFT_GLOBAL_TEST_EXECUTABLE)
	@cd Test && FT_TEST_NAME_FILTER="test_networking_" ./$(notdir $(LIBFT_GLOBAL_TEST_EXECUTABLE))

networking-netem-tests: $(LIBFT_GLOBAL_TEST_EXECUTABLE)
	@cd Test && FT_TEST_NAME_FILTER="test_networking_simulator_" ./$(notdir $(LIBFT_GLOBAL_TEST_EXECUTABLE))

networking-namespace-netem-tests:
	@sh mk/run_linux_network_namespace_tests.sh

networking-soak: $(LIBFT_GLOBAL_TEST_EXECUTABLE)
	@cd Test && run_count=0; while [ $$run_count -lt 10 ]; do FT_TEST_NAME_FILTER="test_networking_" ./$(notdir $(LIBFT_GLOBAL_TEST_EXECUTABLE)) || exit $$?; run_count=$$(($$run_count + 1)); done

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
	@sh mk/check_archive_no_test_symbols.sh $(LIBFT_GLOBAL_RELEASE_ARCHIVES) $(LIBFT_GLOBAL_TARGET)
	@sh mk/check_archive_rebuild.sh "$(LIBFT_GLOBAL_Basic_TARGET)" "$(LIBFT_GLOBAL_Basic_MANIFEST)" $(LIBFT_GLOBAL_Basic_RELEASE_OBJECTS)
	+$(MAKE) --no-print-directory global-all
	@sh mk/check_archive_noop.sh capture "$(LIBFT_ARCHIVE_NOOP_STATE)" $(LIBFT_GLOBAL_RELEASE_ARCHIVES) $(LIBFT_GLOBAL_TARGET)
	+$(MAKE) --no-print-directory global-all
	@sh mk/check_archive_noop.sh verify "$(LIBFT_ARCHIVE_NOOP_STATE)"
	@$(RM) "$(LIBFT_ARCHIVE_NOOP_STATE)"
	@printf '\033[1;35m[LIBFT BUILD] Archive integrity checks passed\033[0m\n'

incremental-build-tests:
	@LIBFT_INCREMENTAL_JOBS=$(if $(JOBS),$(JOBS),4) bash mk/test_incremental_build_graph.sh

agents-policy-scan:
	@sh mk/check_agents_policy.sh

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

tsan: sanitize-clean
	$(MAKE) SANITIZERS=thread all

tsan-tests: sanitize-clean
	$(MAKE) SANITIZERS=thread tests

run-tsan-tests: tsan-tests
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
		$(LIBFT_GLOBAL_Template_TARGET) Full_Libft_analytics.a \
		Full_Libft_analytics_debug.a

fclean:
	@$(RMDIR) build/libft
	@for legacy_object_root in $(LIBFT_LEGACY_OBJECT_ROOTS); do $(RMDIR) "$$legacy_object_root"; done
	@$(RM) $(LIBFT_GLOBAL_RELEASE_ARCHIVES) $(LIBFT_GLOBAL_DEBUG_ARCHIVES) \
		$(LIBFT_GLOBAL_TEST_ARCHIVES) $(LIBFT_GLOBAL_TARGET) \
		$(LIBFT_GLOBAL_DEBUG_TARGET) $(LIBFT_GLOBAL_TEST_TARGET) \
		$(LIBFT_GLOBAL_TEST_EXECUTABLE) $(LIBFT_GLOBAL_TEST_DEBUG_EXECUTABLE) \
		$(LIBFT_GLOBAL_Template_TARGET) Full_Libft_analytics.a \
		Full_Libft_analytics_debug.a

.PHONY: all normal analytics analytics-debug plan internal-all internal-debug global-all global-debug global-tests debug both template demo re re-tests tests internal-tests test-executable run-tests debug-tests performance_benchmarks Efficiency internal-performance run_performance_benchmarks run_Efficiency archive-integrity incremental-build-tests print-build-mode format sanitize-clean networking-fuzz crypto-fuzz crypto-sha-fuzz crypto-aead-fuzz crypto-x25519-fuzz crypto-hmac-hkdf-fuzz crypto-x25519-million networking-simulator-fuzz networking-nat-fuzz networking-handshake-fuzz terrain-persistence-tests crypto-tests networking-message-tests networking-netem-tests networking-namespace-netem-tests networking-soak \
          run-debug-tests run-asan-tests run-ubsan-tests run-asan-ubsan-tests \
          run-tsan-tests asan asan-tests ubsan ubsan-tests tsan tsan-tests \
          asan-ubsan asan-ubsan-tests
