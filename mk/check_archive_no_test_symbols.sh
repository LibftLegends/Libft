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
        grep -E 'crypto_test_|networking_test_failure_|networking_test_hooks' || true)"
    if [ -n "$leaked_symbols" ]; then
        printf 'tester symbols leaked into production archive: %s\n' \
            "$archive_path" >&2
        printf '%s\n' "$leaked_symbols" >&2
        exit 1
    fi
done

printf '%s\n' 'production archive tester-symbol audit passed'
