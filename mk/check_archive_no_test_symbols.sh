#!/usr/bin/env sh

set -eu

if command -v llvm-nm >/dev/null 2>&1; then
    symbol_reader="llvm-nm"
elif command -v nm >/dev/null 2>&1; then
    symbol_reader="nm"
else
    printf '%s\n' 'no nm or llvm-nm available for production symbol audit' >&2
    exit 1
fi

for archive_path do
    if [ ! -f "$archive_path" ]; then
        printf 'archive does not exist: %s\n' "$archive_path" >&2
        exit 1
    fi
    leaked_symbols="$($symbol_reader -g "$archive_path" 2>/dev/null | \
        grep -E '(_test_|_for_tests|reset_for_tests|destroy_for_tests)|crypto_test_|networking_test_failure_|test_secret_state_is_wiped|test_(is_wiped|state_is_wiped)|networking_test_hooks|voxel_json_test_|su_(set|reset)_write_syscall_hook|su_force_file_stream_allocation_failure|su_service_force_no_fork|cmp_(force|set_force|clear_force)_|rsa_set_force_mod_inverse_failure|be_set_save_game_hooks|pt_rwlock_test_failure_stage|gnl_(set|reset)_leftover_alloc_hook|nw_set_socket_hook|time_(set|reset)_clock_now_hook|api_request_set_downgrade_wait_hook|test_failure_controller|ft_dumb_controls_.*test_|ft_(compress|decompress)_stream_set_.*_hook' || true)"
    if [ -n "$leaked_symbols" ]; then
        printf 'tester symbols leaked into production archive: %s\n' \
            "$archive_path" >&2
        printf '%s\n' "$leaked_symbols" >&2
        exit 1
    fi
done

printf '%s\n' 'production archive tester-symbol audit passed'
