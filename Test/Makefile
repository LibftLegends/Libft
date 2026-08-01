ifeq ($(OS),Windows_NT)
SHELL := C:/Progra~1/Git/usr/bin/bash.exe
.SHELLFLAGS := -lc
export SHELL
export LIBFT_POSIX_SHELL := 1
endif

ifneq ($(filter Efficiency run_Efficiency performance_benchmarks run_performance_benchmarks,$(MAKECMDGOALS)),)
OPT_LEVEL := 3
endif

include ../mk/compiler_flags.mk
include ../mk/test/config.mk

TEST_LIBFT_ARCHIVE_TARGET := Test/Full_Libft_test.a
TEST_LIBFT_DEBUG_ARCHIVE_TARGET := Test/Full_Libft_test_debug.a
TEST_BUILD_PROGRESS_FILE := $(OBJDIR)/.stale_object_progress
DEBUG_BUILD_PROGRESS_FILE := $(DEBUG_OBJDIR)/.stale_object_progress
EFFICIENCY_BUILD_PROGRESS_FILE := $(EFFICIENCY_OBJDIR)/.stale_object_progress
TEST_BUILD_OBJECT_LIST := $(OBJDIR)/.stale_object_paths
DEBUG_BUILD_OBJECT_LIST := $(DEBUG_OBJDIR)/.stale_object_paths
EFFICIENCY_BUILD_OBJECT_LIST := $(EFFICIENCY_OBJDIR)/.stale_object_paths

all: $(TARGET)

.PHONY: FORCE
FORCE:

$(OBJDIR) $(DEBUG_OBJDIR) $(EFFICIENCY_OBJDIR):
	@$(MKDIR) $@

$(TARGET): $(OBJS) $(LIBFT_ARCHIVE)
	@$(RM) $@
ifeq ($(UNAME_S),Darwin)
	@$(CXX) $(CFLAGS) -o $@ $(OBJS) $(LIBFT_ARCHIVE) $(LDFLAGS)
else
	@$(file >$@.rsp,$(OBJS) -Wl,--start-group $(LIBFT_ARCHIVE) -Wl,--end-group)
	@$(CXX) $(CFLAGS) -o $@ @$@.rsp $(LDFLAGS)
	@$(RM) $@.rsp
endif
	@printf '\033[1;35m[LIBFT BUILD] Test executable ready (%s)\033[0m\n' "$(TARGET)"

$(OBJS): | $(TEST_BUILD_PROGRESS_FILE)
$(DEBUG_OBJS): | $(DEBUG_BUILD_PROGRESS_FILE)
$(EFFICIENCY_OBJS): | $(EFFICIENCY_BUILD_PROGRESS_FILE)

$(TEST_BUILD_PROGRESS_FILE): FORCE | $(OBJDIR)
	$(file >$(TEST_BUILD_OBJECT_LIST),$(firstword $(OBJS)))
	$(foreach object,$(wordlist 2,$(words $(OBJS)),$(OBJS)),$(file >>$(TEST_BUILD_OBJECT_LIST),$(object)))
	@stale_count=$$(bash ../mk/count_stale_objects.sh --file "$(TEST_BUILD_OBJECT_LIST)"); \
        printf '0\n%s\n' "$$stale_count" > "$@"

$(DEBUG_BUILD_PROGRESS_FILE): FORCE | $(DEBUG_OBJDIR)
	$(file >$(DEBUG_BUILD_OBJECT_LIST),$(firstword $(DEBUG_OBJS)))
	$(foreach object,$(wordlist 2,$(words $(DEBUG_OBJS)),$(DEBUG_OBJS)),$(file >>$(DEBUG_BUILD_OBJECT_LIST),$(object)))
	@stale_count=$$(bash ../mk/count_stale_objects.sh --file "$(DEBUG_BUILD_OBJECT_LIST)"); \
        printf '0\n%s\n' "$$stale_count" > "$@"

$(EFFICIENCY_BUILD_PROGRESS_FILE): FORCE | $(EFFICIENCY_OBJDIR)
	$(file >$(EFFICIENCY_BUILD_OBJECT_LIST),$(firstword $(EFFICIENCY_OBJS)))
	$(foreach object,$(wordlist 2,$(words $(EFFICIENCY_OBJS)),$(EFFICIENCY_OBJS)),$(file >>$(EFFICIENCY_BUILD_OBJECT_LIST),$(object)))
	@stale_count=$$(bash ../mk/count_stale_objects.sh --file "$(EFFICIENCY_BUILD_OBJECT_LIST)"); \
        printf '0\n%s\n' "$$stale_count" > "$@"

Basic:
	@echo "Building basic test executable ($(TARGET))"
	@$(MAKE) FILES="$(BASIC_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

Time:
	@echo "Building Time test executable ($(TARGET))"
	@$(MAKE) FILES="$(TIME_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

CMA:
	@echo "Building CMA test executable ($(TARGET))"
	@$(MAKE) FILES="$(CMA_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

performance_benchmarks: $(EFFICIENCY_TARGET)
	@echo "Performance benchmark executable ready ($(EFFICIENCY_TARGET), OPT_LEVEL=$(EFFICIENCY_OPT_LEVEL))."

run_performance_benchmarks:
	@if [ ! -x "./$(EFFICIENCY_TARGET)" ]; then \
		printf 'Performance benchmark executable is missing; run make performance_benchmarks first.\n' >&2; \
		exit 1; \
	fi
	@./$(EFFICIENCY_TARGET)

Efficiency: performance_benchmarks

run_Efficiency: $(EFFICIENCY_TARGET)
	@$(MAKE) run_performance_benchmarks

single_test:
	@if [ -z "$(TEST)" ]; then \
		echo "Usage: make single_test TEST=Test/test_name.cpp"; \
		exit 1; \
	fi
	@python3 tools/build_single_test.py "$(TEST)"

run_single_test: single_test

get_next_line:
	@echo "Building get_next_line test executable ($(TARGET))"
	@$(MAKE) FILES="$(GET_NEXT_LINE_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

File:
	@echo "Building file test executable ($(TARGET))"
	@$(MAKE) FILES="$(FILE_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

Config:
	@echo "Building config test executable ($(TARGET))"
	@$(MAKE) FILES="$(CONFIG_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

Filesystem:
	@echo "Building filesystem test executable ($(TARGET))"
	@$(MAKE) FILES="$(FILESYSTEM_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

Storage:
	@echo "Building Storage test executable ($(TARGET))"
	@$(MAKE) FILES="$(STORAGE_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

Voxel:
	@echo "Building Voxel test executable ($(TARGET))"
	@$(MAKE) FILES="$(VOXEL_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

CLI:
	@echo "Building CLI test executable ($(TARGET))"
	@$(MAKE) FILES="$(CLI_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

Application:
	@echo "Building Application test executable ($(TARGET))"
	@$(MAKE) FILES="$(APPLICATION_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

Logger:
	@echo "Building Logger test executable ($(TARGET))"
	@$(MAKE) FILES="$(LOGGER_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

Sink:
	@echo "Building Sink test executable ($(TARGET))"
	@$(MAKE) FILES="$(SINK_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

GPGR:
	@echo "Building GPGR test executable ($(TARGET))"
	@$(MAKE) FILES="$(GPGR_TEST_FILES)" all LIBFT_TEST_BATCH_OUTPUT="$(LIBFT_TEST_BATCH_OUTPUT)"

re_get_next_line: fclean get_next_line

re_Basic: fclean Basic

re_CLI: fclean CLI

re_Config: fclean Config

re_Application: fclean Application

re_Logger: fclean Logger

re_Storage: fclean Storage

re_Sink: fclean Sink

re_GPGR: fclean GPGR

re_Voxel: fclean Voxel

re_CMA: fclean CMA

build_tests: all
	@echo "Test executable ready; run ./$(TARGET) manually if desired."

$(OBJDIR)/%.o: %.cpp
	@batch_output=0; \
	case "$$MAKEFLAGS" in \
		*"-j1"*) batch_output=0 ;; \
		*"-j"*|*"--jobserver-auth="*) batch_output=1 ;; \
	esac; \
	if [ "$$batch_output" = "1" ]; then \
		log_file=".libft_test_build_$$(printf '%s' "$@" | tr '/.' '__')_$$$$.log"; \
		{ \
			if [ ! -d "$(dir $@)" ]; then \
				$(MKDIR) $(dir $@); \
			fi; \
			$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@ && \
	$(FIXDEP); \
		} > "$$log_file" 2>&1; \
		status=$$?; \
		if [ $$status -ne 0 ]; then \
			while ! mkdir $(LIBFT_TEST_OUTPUT_LOCK) 2>/dev/null; do sleep 0.02; done; \
			cat "$$log_file"; \
			rmdir $(LIBFT_TEST_OUTPUT_LOCK); \
			$(RM) "$$log_file"; \
			exit $$status; \
		fi; \
		progress_lock="$(TEST_BUILD_PROGRESS_FILE).lock"; \
		while ! mkdir "$$progress_lock" 2>/dev/null; do sleep 0.02; done; \
		current_count=$$(sed -n '1p' "$(TEST_BUILD_PROGRESS_FILE)"); \
		total_count=$$(sed -n '2p' "$(TEST_BUILD_PROGRESS_FILE)"); \
		current_count=$$((current_count + 1)); \
		printf '%s\n%s\n' "$$current_count" "$$total_count" > "$(TEST_BUILD_PROGRESS_FILE)"; \
		printf '%b        [Test] Building file %s (%d/%d)%b\n' "$(LIBFT_TEST_LIGHT_BLUE)" "$<" $$current_count $$total_count "$(LIBFT_TEST_COLOR_OFF)"; \
		rmdir "$$progress_lock"; \
		while ! mkdir $(LIBFT_TEST_OUTPUT_LOCK) 2>/dev/null; do sleep 0.02; done; \
		rmdir $(LIBFT_TEST_OUTPUT_LOCK); \
		$(RM) "$$log_file"; \
	else \
		if [ ! -d "$(dir $@)" ]; then \
			$(MKDIR) $(dir $@); \
		fi; \
		$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@ && \
	$(FIXDEP); \
		status=$$?; \
		if [ $$status -ne 0 ]; then exit $$status; fi; \
		progress_lock="$(TEST_BUILD_PROGRESS_FILE).lock"; \
		while ! mkdir "$$progress_lock" 2>/dev/null; do sleep 0.02; done; \
		current_count=$$(sed -n '1p' "$(TEST_BUILD_PROGRESS_FILE)"); \
		total_count=$$(sed -n '2p' "$(TEST_BUILD_PROGRESS_FILE)"); \
		current_count=$$((current_count + 1)); \
		printf '%s\n%s\n' "$$current_count" "$$total_count" > "$(TEST_BUILD_PROGRESS_FILE)"; \
		rmdir "$$progress_lock"; \
		printf '%b        [Test] Building file %s (%d/%d)%b\n' "$(LIBFT_TEST_LIGHT_BLUE)" "$<" $$current_count $$total_count "$(LIBFT_TEST_COLOR_OFF)"; \
	fi

$(OBJDIR)/Test/test_promise.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_task_scheduler.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"PThread\"
$(OBJDIR)/Test/test_pthread_thread.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"PThread\"
$(OBJDIR)/Test/test_pthread_thread_thread_safety.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"PThread\"
$(OBJDIR)/Test/test_pthread_blocking_queue_thread_safety.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"PThread\"
$(OBJDIR)/Test/test_pthread_task_scheduler_thread_safety.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"PThread\"
$(OBJDIR)/Test/test_pthread_lock_tracking.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"PThread\"
$(OBJDIR)/Test/test_template_future_thread_safety.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_future_lifecycle.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_pool_thread_safety.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_stack_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_queue_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_bitset_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_function_copy_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_vector_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_matrix_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_priority_queue_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_pair_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_variant_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_pool_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_set_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_unique_ptr_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_shared_ptr_copy_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_future_copy_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_tuple_move_mutex.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_circular_buffer_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_unordered_map_copy_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_queue_lifecycle_abort.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_graph_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_graph_move_errors.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_deque_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_deque_move_errors.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_iterator_copy_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_unordered_map_allocation_failure_paths.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_template_trie_copy_move_deleted.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Template\"
$(OBJDIR)/Test/test_advanced_memory.o $(OBJDIR)/Test/test_advanced_numeric.o $(OBJDIR)/Test/test_advanced_strings.o \
$(OBJDIR)/Test/test_strjoin_multiple.o $(OBJDIR)/Test/test_to_string.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Advanced\"
$(OBJDIR)/Test/test_basic_atoi.o $(OBJDIR)/Test/test_basic_wstrlen.o \
$(OBJDIR)/Test/test_basic_utf8.o $(OBJDIR)/Test/test_basic_validate_int.o \
$(OBJDIR)/Test/test_basic_additional_coverage.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Basic\"
$(OBJDIR)/Test/test_networking.o $(OBJDIR)/Test/test_http_server.o $(OBJDIR)/Test/test_websocket.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Networking\"
$(OBJDIR)/Test/test_networking_quic.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Networking\"
$(OBJDIR)/Test/test_logger.o $(OBJDIR)/Test/test_logger_async_logging.o $(OBJDIR)/Test/test_logger_network.o \
$(OBJDIR)/Test/test_logger_file.o $(OBJDIR)/Test/test_logger_control.o $(OBJDIR)/Test/test_logger_sink.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Logger\"
$(OBJDIR)/Test/test_json_validate.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"JSon\"
$(OBJDIR)/Test/test_cli.o $(OBJDIR)/Test/test_cli_completion.o \
$(OBJDIR)/Test/test_cli_config.o $(OBJDIR)/Test/test_cli_get_bool.o \
$(OBJDIR)/Test/test_cli_get_double.o $(OBJDIR)/Test/test_cli_get_int64.o \
$(OBJDIR)/Test/test_cli_get_string.o $(OBJDIR)/Test/test_cli_get_uint64.o \
$(OBJDIR)/Test/test_cli_help.o $(OBJDIR)/Test/test_cli_tree.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"CLI\"
$(OBJDIR)/Test/test_storage_kv_store.o $(OBJDIR)/Test/test_storage_kv_store_entry.o \
$(OBJDIR)/Test/test_storage_kv_store_block_cipher_metadata.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Storage\"
$(OBJDIR)/Test/test_voxel_block_metadata.o $(OBJDIR)/Test/test_voxel_generator.o \
$(OBJDIR)/Test/test_voxel_mesh.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Voxel\" \
	-DGAME_USE_VOXEL_REGION_BACKEND=1
$(OBJDIR)/Test/test_voxel_block_metadata.o $(OBJDIR)/Test/test_voxel_generator.o \
$(OBJDIR)/Test/test_voxel_mesh.o: Makefile
$(OBJDIR)/Test/test_application_auth_service.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Application\"
$(OBJDIR)/Test/test_yaml.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"YAML\"
$(OBJDIR)/Test/test_rng.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"RNG\"
$(OBJDIR)/Test/test_rng_statistical_quality.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"RNG\"
$(OBJDIR)/Test/test_rng_distribution_coverage.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"RNG\"
$(OBJDIR)/Test/test_rng_vectorized_sampling.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"RNG\"
$(OBJDIR)/Test/test_rng_stream.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"RNG\"
$(OBJDIR)/Test/test_linear_algebra.o $(OBJDIR)/Test/test_quaternion.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Math\"
$(OBJDIR)/Test/test_string_view.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"String\"
$(OBJDIR)/Test/test_pathfinding.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_data_catalog.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_progress_tracker.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_economy_table.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_economy_table_copy_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_economy_records.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_economy_record_setters.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_economy_helpers.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_economy_table_setters.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_behavior_profiles.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_behavior_records.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_behavior_record_setters.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_behavior_helpers.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_behavior_table_copy_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_data_catalog_records.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_data_catalog_record_setters.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_dialogue_repository.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_dialogue_records.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_dialogue_record_setters.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_dialogue_helpers.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_dialogue_table_copy_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_dialogue_table_setters.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_world_registry.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_world_registry_records.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_world_registry_record_setters.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_world_registry_table_copy_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_game_world_registry_table_setters.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Game\"
$(OBJDIR)/Test/test_encryption_key.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Encryption\"
$(OBJDIR)/Test/test_encryption_secure_wipe.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Encryption\"
$(OBJDIR)/Test/test_encryption_key_management.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Encryption\"
$(OBJDIR)/Test/test_encryption_aead_copy_move.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Encryption\"
$(OBJDIR)/Test/test_encryption_block_hooks.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"Encryption\"
$(OBJDIR)/Test/test_xml_copy_move_deleted.o: CFLAGS := $(COMPILE_FLAGS) -DTEST_MODULE=\"XML\"

debug: CXXFLAGS += -DDEBUG=1
debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(DEBUG_OBJS) $(LIBFT_DEBUG_ARCHIVE)
	@printf "Compiled debug test files (%d files)\n" $(TOTAL_SRCS)
	@echo "Linking final executable ($(DEBUG_TARGET))"
	@$(MAKE) -C $(LIBFT_ROOT_DIR) $(LIBFT_DEBUG_ARCHIVE) COMPILE_FLAGS="$(COMPILE_FLAGS)" \
		OPT_LEVEL=$(OPT_LEVEL)
	@$(RM) $@
ifeq ($(UNAME_S),Darwin)
	@$(CXX) $(CFLAGS) -o $@ $(DEBUG_OBJS) $(LIBFT_DEBUG_ARCHIVE) $(LDFLAGS)
else
	@$(file >$@.rsp,$(DEBUG_OBJS) $(LIBFT_DEBUG_ARCHIVE))
	@$(CXX) $(CFLAGS) -o $@ @$@.rsp $(LDFLAGS)
	@$(RM) $@.rsp
endif

$(DEBUG_OBJDIR)/%.o: %.cpp
	@batch_output=0; \
	case "$$MAKEFLAGS" in \
		*"-j1"*) batch_output=0 ;; \
		*"-j"*|*"--jobserver-auth="*) batch_output=1 ;; \
	esac; \
	if [ "$$batch_output" = "1" ]; then \
		log_file=".libft_test_build_$$(printf '%s' "$@" | tr '/.' '__')_$$$$.log"; \
		{ \
			if [ ! -d "$(dir $@)" ]; then \
				$(MKDIR) $(dir $@); \
			fi; \
			$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@ && \
	$(FIXDEP); \
		} > "$$log_file" 2>&1; \
		status=$$?; \
		if [ $$status -ne 0 ]; then \
			while ! mkdir $(LIBFT_TEST_OUTPUT_LOCK) 2>/dev/null; do sleep 0.02; done; \
			cat "$$log_file"; \
			rmdir $(LIBFT_TEST_OUTPUT_LOCK); \
			$(RM) "$$log_file"; \
			exit $$status; \
		fi; \
		progress_lock="$(DEBUG_BUILD_PROGRESS_FILE).lock"; \
		while ! mkdir "$$progress_lock" 2>/dev/null; do sleep 0.02; done; \
		current_count=$$(sed -n '1p' "$(DEBUG_BUILD_PROGRESS_FILE)"); \
		total_count=$$(sed -n '2p' "$(DEBUG_BUILD_PROGRESS_FILE)"); \
		current_count=$$((current_count + 1)); \
		printf '%s\n%s\n' "$$current_count" "$$total_count" > "$(DEBUG_BUILD_PROGRESS_FILE)"; \
		printf '%b        [Test Debug] Building file %s (%d/%d)%b\n' "$(LIBFT_TEST_LIGHT_BLUE)" "$<" $$current_count $$total_count "$(LIBFT_TEST_COLOR_OFF)"; \
		rmdir "$$progress_lock"; \
		while ! mkdir $(LIBFT_TEST_OUTPUT_LOCK) 2>/dev/null; do sleep 0.02; done; \
		rmdir $(LIBFT_TEST_OUTPUT_LOCK); \
		$(RM) "$$log_file"; \
	else \
		if [ ! -d "$(dir $@)" ]; then \
			$(MKDIR) $(dir $@); \
		fi; \
		$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@ && \
	$(FIXDEP); \
		status=$$?; \
		if [ $$status -ne 0 ]; then exit $$status; fi; \
		progress_lock="$(DEBUG_BUILD_PROGRESS_FILE).lock"; \
		while ! mkdir "$$progress_lock" 2>/dev/null; do sleep 0.02; done; \
		current_count=$$(sed -n '1p' "$(DEBUG_BUILD_PROGRESS_FILE)"); \
		total_count=$$(sed -n '2p' "$(DEBUG_BUILD_PROGRESS_FILE)"); \
		current_count=$$((current_count + 1)); \
		printf '%s\n%s\n' "$$current_count" "$$total_count" > "$(DEBUG_BUILD_PROGRESS_FILE)"; \
		rmdir "$$progress_lock"; \
		printf '%b        [Test Debug] Building file %s (%d/%d)%b\n' "$(LIBFT_TEST_LIGHT_BLUE)" "$<" $$current_count $$total_count "$(LIBFT_TEST_COLOR_OFF)"; \
	fi

$(EFFICIENCY_TARGET): $(EFFICIENCY_OBJS) $(EFFICIENCY_ARCHIVE_STAMP)
	@printf "Compiled efficiency files (%d files)\n" $(EFF_TOTAL_SRCS)
	@echo "Linking final executable ($(EFFICIENCY_TARGET))"
	@$(RM) $@
ifeq ($(UNAME_S),Darwin)
	@$(CXX) $(EFFICIENCY_CFLAGS) -o $@ $(EFFICIENCY_OBJS) $(EFFICIENCY_LIBFT_ARCHIVES) $(LDFLAGS)
else
	@$(file >$@.rsp,$(EFFICIENCY_OBJS) -Wl,--start-group $(EFFICIENCY_LIBFT_ARCHIVES) -Wl,--end-group)
	@$(CXX) $(EFFICIENCY_CFLAGS) -o $@ @$@.rsp $(LDFLAGS)
	@$(RM) $@.rsp
endif

$(EFFICIENCY_OBJDIR)/%.o: %.cpp
	@batch_output=0; \
	case "$$MAKEFLAGS" in \
		*"-j1"*) batch_output=0 ;; \
		*"-j"*|*"--jobserver-auth="*) batch_output=1 ;; \
	esac; \
	if [ "$$batch_output" = "1" ]; then \
		log_file=".libft_test_build_$$(printf '%s' "$@" | tr '/.' '__')_$$$$.log"; \
		{ \
			if [ ! -d "$(dir $@)" ]; then \
				$(MKDIR) $(dir $@); \
			fi; \
			$(CXX) $(EFFICIENCY_CFLAGS) -MMD -MP -c $< -o $@ && \
	$(FIXDEP); \
		} > "$$log_file" 2>&1; \
		status=$$?; \
		if [ $$status -ne 0 ]; then \
			while ! mkdir $(LIBFT_TEST_OUTPUT_LOCK) 2>/dev/null; do sleep 0.02; done; \
			cat "$$log_file"; \
			rmdir $(LIBFT_TEST_OUTPUT_LOCK); \
			$(RM) "$$log_file"; \
			exit $$status; \
		fi; \
		progress_lock="$(EFFICIENCY_BUILD_PROGRESS_FILE).lock"; \
		while ! mkdir "$$progress_lock" 2>/dev/null; do sleep 0.02; done; \
		current_count=$$(sed -n '1p' "$(EFFICIENCY_BUILD_PROGRESS_FILE)"); \
		total_count=$$(sed -n '2p' "$(EFFICIENCY_BUILD_PROGRESS_FILE)"); \
		current_count=$$((current_count + 1)); \
		printf '%s\n%s\n' "$$current_count" "$$total_count" > "$(EFFICIENCY_BUILD_PROGRESS_FILE)"; \
		printf '%b        [Test Efficiency] Building file %s (%d/%d)%b\n' "$(LIBFT_TEST_LIGHT_BLUE)" "$<" $$current_count $$total_count "$(LIBFT_TEST_COLOR_OFF)"; \
		rmdir "$$progress_lock"; \
		while ! mkdir $(LIBFT_TEST_OUTPUT_LOCK) 2>/dev/null; do sleep 0.02; done; \
		rmdir $(LIBFT_TEST_OUTPUT_LOCK); \
		$(RM) "$$log_file"; \
	else \
		if [ ! -d "$(dir $@)" ]; then \
			$(MKDIR) $(dir $@); \
		fi; \
		$(CXX) $(EFFICIENCY_CFLAGS) -MMD -MP -c $< -o $@ && \
	$(FIXDEP); \
		status=$$?; \
		if [ $$status -ne 0 ]; then exit $$status; fi; \
		progress_lock="$(EFFICIENCY_BUILD_PROGRESS_FILE).lock"; \
		while ! mkdir "$$progress_lock" 2>/dev/null; do sleep 0.02; done; \
		current_count=$$(sed -n '1p' "$(EFFICIENCY_BUILD_PROGRESS_FILE)"); \
		total_count=$$(sed -n '2p' "$(EFFICIENCY_BUILD_PROGRESS_FILE)"); \
		current_count=$$((current_count + 1)); \
		printf '%s\n%s\n' "$$current_count" "$$total_count" > "$(EFFICIENCY_BUILD_PROGRESS_FILE)"; \
		rmdir "$$progress_lock"; \
		printf '%b        [Test Efficiency] Building file %s (%d/%d)%b\n' "$(LIBFT_TEST_LIGHT_BLUE)" "$<" $$current_count $$total_count "$(LIBFT_TEST_COLOR_OFF)"; \
	fi

$(EFFICIENCY_ARCHIVE_STAMP): $(EFFICIENCY_ARCHIVE_SOURCE_FILES) | $(EFFICIENCY_OBJDIR)
	@$(MAKE) -C $(LIBFT_ROOT_DIR) $(EFFICIENCY_LIBFT_RELATIVE_ARCHIVES) \
		COMPILE_FLAGS="$(EFFICIENCY_COMPILE_FLAGS)" OPT_LEVEL=$(OPT_LEVEL) \
		LIBFT_ARCHIVE_SUFFIX="$(EFFICIENCY_ARCHIVE_SUFFIX)"
	@touch $@

$(LIBFT_ARCHIVE):
	@need_build=0; \
	if $(MAKE) -C $(LIBFT_ROOT_DIR) -q $(TEST_LIBFT_ARCHIVE_TARGET) COMPILE_FLAGS="$(COMPILE_FLAGS)" \
			OPT_LEVEL=$(OPT_LEVEL); then \
		:; \
	else \
		status=$$?; \
		if [ $$status -eq 1 ]; then \
			need_build=1; \
		else \
			exit $$status; \
		fi; \
	fi; \
	if [ $$need_build -eq 1 ] || [ ! -f $@ ]; then \
		        $(MAKE) -C $(LIBFT_ROOT_DIR) $(TEST_LIBFT_ARCHIVE_TARGET) COMPILE_FLAGS="$(COMPILE_FLAGS)" \
				OPT_LEVEL=$(OPT_LEVEL); \
	fi

$(LIBFT_DEBUG_ARCHIVE): FORCE
	@need_build=0; \
	if $(MAKE) -C $(LIBFT_ROOT_DIR) -q $(TEST_LIBFT_DEBUG_ARCHIVE_TARGET) COMPILE_FLAGS="$(COMPILE_FLAGS)" \
			OPT_LEVEL=$(OPT_LEVEL); then \
		:; \
	else \
		status=$$?; \
		if [ $$status -eq 1 ]; then \
			need_build=1; \
		else \
			exit $$status; \
		fi; \
	fi; \
	if [ $$need_build -eq 1 ] || [ ! -f $@ ]; then \
		        $(MAKE) -C $(LIBFT_ROOT_DIR) $(TEST_LIBFT_DEBUG_ARCHIVE_TARGET) COMPILE_FLAGS="$(COMPILE_FLAGS)" \
				OPT_LEVEL=$(OPT_LEVEL); \
	fi

_clean_common:
	@status=0; \
	for build_dir in objs*; do \
		if [ -d "$$build_dir" ]; then \
			if ! find "$$build_dir" -type f \( -name '*.o' -o -name '*.d' -o -name '*.gcda' -o -name '*.gcno' \) -delete 2>/dev/null; then \
				status=1; \
			fi; \
		fi; \
	done; \
	if ! $(RM) $(LIBFT_TEST_OUTPUT_LOGS); then \
	        status=1; \
	fi; \
	if ! $(RMDIR) .libft_progress; then \
	        status=1; \
	fi; \
	if ! $(RMDIR) $(CLEAN_DIRS) $(LIBFT_TEST_OUTPUT_LOCK); then \
	        status=1; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '        [Test] clean: failed\n'; \
		exit 1; \
	fi

clean: _clean_common
	@if [ -z "$(LIBFT_PARENT_CLEAN)" ]; then \
		if ! $(MAKE) -C $(LIBFT_ROOT_DIR) clean; then \
			exit 1; \
		fi; \
	fi
	@printf '        [Test] clean: success\n'

fclean: _clean_common
	@status=0; \
	if ! $(RM) $(TARGET) $(DEBUG_TARGET) $(EFFICIENCY_TARGET); then \
		status=1; \
	fi; \
	if [ -z "$(LIBFT_PARENT_CLEAN)" ]; then \
		if ! $(MAKE) -C $(LIBFT_ROOT_DIR) fclean; then \
			status=1; \
		fi; \
	fi; \
	if [ $$status -ne 0 ]; then \
		printf '        [Test] fclean: failed\n'; \
		exit 1; \
	fi; \
	printf '        [Test] fclean: success\n'

re:
	@$(MAKE) fclean
	@$(MAKE) all

both: all debug

-include $(DEPS) $(DEBUG_DEPS) $(EFFICIENCY_DEPS)

.PHONY: all clean fclean re debug both build_tests Basic re_Basic CMA re_CMA \
	Efficiency run_Efficiency performance_benchmarks run_performance_benchmarks \
	get_next_line re_get_next_line GPGR re_GPGR FORCE

print_test_compile_flags:
	@printf '%s\n' "$(COMPILE_FLAGS)"

print_test_ldflags:
	@printf '%s\n' "$(LDFLAGS)"

print_test_objdir:
	@printf '%s\n' "$(OBJDIR)"

FORCE:
