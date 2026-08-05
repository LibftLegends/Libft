BASIC_TEST_FILES := \
	Test/test_basic_atoi.cpp \
	Test/test_basic_atol.cpp \
	Test/test_basic_additional_coverage.cpp \
	Test/test_basic_bzero.cpp \
	Test/test_basic_isalnum.cpp \
	Test/test_basic_isalpha.cpp \
	Test/test_basic_isdigit.cpp \
	Test/test_basic_islower.cpp \
	Test/test_basic_isprint.cpp \
	Test/test_basic_isspace.cpp \
	Test/test_basic_isupper.cpp \
	Test/test_basic_memchr.cpp \
	Test/test_basic_memcmp.cpp \
	Test/test_basic_memcpy.cpp \
	Test/test_basic_memcpy_s.cpp \
	Test/test_basic_memmove.cpp \
	Test/test_basic_memset.cpp \
	Test/test_basic_strchr.cpp \
	Test/test_basic_strcmp.cpp \
	Test/test_basic_hash_string31.cpp \
	Test/test_basic_strcpy_s.cpp \
	Test/test_basic_striteri.cpp \
	Test/test_basic_strlcat.cpp \
	Test/test_basic_strlcpy.cpp \
	Test/test_basic_ascii_utf8_trim.cpp \
	Test/test_basic_string_predicates.cpp \
	Test/test_basic_strlen.cpp \
	Test/test_basic_strmapi.cpp \
	Test/test_basic_strncmp.cpp \
	Test/test_basic_strncpy.cpp \
	Test/test_basic_strnstr.cpp \
	Test/test_basic_strrchr.cpp \
	Test/test_basic_strstr.cpp \
	Test/test_basic_strtok.cpp \
	Test/test_basic_strtol.cpp \
	Test/test_basic_strtoul.cpp \
	Test/test_basic_tolower.cpp \
	Test/test_basic_toupper.cpp \
	Test/test_basic_utf8.cpp \
	Test/test_basic_validate_int.cpp \
	Test/test_basic_wstrlen.cpp \
	Test/test_basic_size_add_checked.cpp \
	Test/test_basic_size_multiply_checked.cpp \
	Test/test_basic_is_power_of_two.cpp \
	Test/test_basic_align_up_checked.cpp \
	Test/test_basic_isxdigit.cpp \
	Test/test_basic_ispunct.cpp \
	Test/test_basic_isgraph.cpp \
	Test/test_basic_iscntrl.cpp \
	Test/test_basic_isblank.cpp \
	Test/test_basic_memswap.cpp \
	Test/test_basic_memmem.cpp \
	Test/test_basic_parse_uint32.cpp \
	Test/test_basic_parse_int64.cpp \
	Test/test_basic_utf8_validate.cpp

TIME_TEST_FILES := \
	Test/test_time_active_clock.cpp

CMA_TEST_FILES := \
	Test/test_cma_alloc.cpp \
	Test/test_cma_arena.cpp \
	Test/test_cma_backend.cpp \
	Test/test_cma_block_size.cpp \
	Test/test_cma_global_new.cpp \
	Test/test_cma_limits.cpp \
	Test/test_cma_metadata.cpp \
	Test/test_cma_stats.cpp \
	Test/test_cma_strings.cpp \
	Test/test_cma_scma_secure_wipe.cpp \
	Test/test_scma_accessor.cpp \
	Test/test_scma_accessor_lifecycle.cpp \
	Test/test_scma_accessor_proxy_chain_errors.cpp \
	Test/test_scma_accessor_uninitialized_abort.cpp \
	Test/test_scma_lifecycle.cpp \
	Test/test_scma_memory.cpp \
	Test/test_scma_recursive_mutex.cpp \
	Test/test_scma_thread_safety_controls.cpp \
	Test/test_scma_resize.cpp

GET_NEXT_LINE_TEST_FILES := \
	Test/test_get_next_line.cpp \
	Test/test_get_next_line_strjoin.cpp

BUFFER_TEST_FILES := \
	Test/test_buffer_byte_buffer.cpp

FILE_TEST_FILES := \
	Test/test_file_copy.cpp \
	Test/test_file_copy_directory.cpp \
	Test/test_file_directory_filters.cpp \
	Test/test_file_hash_metadata.cpp \
	Test/test_file_secure_temp_file.cpp \
	Test/test_file_utils.cpp

CONFIG_TEST_FILES := \
	Test/test_config.cpp

FILESYSTEM_TEST_FILES := \
	Test/test_filesystem.cpp \
	Test/test_filesystem_glob.cpp \
	Test/test_filesystem_path_helpers.cpp

STORAGE_TEST_FILES := \
	Test/test_storage_kv_store.cpp \
	Test/test_storage_kv_store_entry.cpp \
	Test/test_storage_kv_store_block_cipher_metadata.cpp

VOXEL_TEST_FILES := \
	Test/test_voxel_block_metadata.cpp \
	Test/test_voxel_generator.cpp \
	Test/test_voxel_mesh.cpp \
	Test/test_terrain_script_register_api.cpp \
	Test/test_terrain_script_execute.cpp \
	Test/test_terrain_runtime_blocks.cpp

CLI_TEST_FILES := \
	Test/test_cli.cpp \
	Test/test_cli_completion.cpp \
	Test/test_cli_config.cpp \
	Test/test_cli_get_bool.cpp \
	Test/test_cli_get_double.cpp \
	Test/test_cli_get_int64.cpp \
	Test/test_cli_get_string.cpp \
	Test/test_cli_get_uint64.cpp \
	Test/test_cli_help.cpp \
	Test/test_cli_tree.cpp

APPLICATION_TEST_FILES := \
	Test/test_application_auth_service.cpp \
	Test/test_application_auth_service_path_safety.cpp \
	Test/test_application_auth_service_approval_rules.cpp \
	Test/test_application_auth_service_login_signal.cpp \
	Test/test_application_settings.cpp

ENCRYPTION_TEST_FILES := \
	Test/test_encryption_basic.cpp \
	Test/test_encryption_block_hooks.cpp \
	Test/test_encryption_hash_algorithms.cpp \
	Test/test_encryption_hardware_acceleration.cpp \
	Test/test_encryption_key.cpp \
	Test/test_encryption_key_management.cpp \
	Test/test_encryption_rsa.cpp \
	Test/test_encryption_secure_wipe.cpp \
	Test/test_encryption_vectors.cpp \
	Test/test_encryption_aead.cpp \
	Test/test_encryption_aead_copy_move.cpp

LOGGER_TEST_FILES := \
	Test/test_logger.cpp \
	Test/test_logger_async_logging.cpp \
	Test/test_logger_network.cpp \
	Test/test_logger_file.cpp \
	Test/test_logger_control.cpp \
	Test/test_logger_sink.cpp

SINK_TEST_FILES := \
	Test/test_sink.cpp

GPGR_TEST_FILES := \
	Test/test_gpgr_loader.cpp \
	Test/test_gpgr_platform.cpp \
	Test/test_gpgr_shader.cpp \
	Test/test_gpgr_window.cpp
