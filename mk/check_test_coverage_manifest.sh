#!/bin/sh

set -eu

manifest_path="${1:-Docs/test_coverage/test_failure_controller.tsv}"
test_executable="${2:-}"

if [ ! -f "$manifest_path" ]; then
    printf '%s\n' "coverage manifest is missing: $manifest_path" >&2
    exit 1
fi
line_number=0
entry_count=0
test_paths_file="$(mktemp)"
runtime_log_file="$(mktemp)"
cleanup_files()
{
    rm -f "$test_paths_file" "$runtime_log_file"
    return 0
}
trap cleanup_files EXIT HUP INT TERM
while IFS='|' read -r kind symbol declaration_path test_path coverage; do
    line_number=$((line_number + 1))
    case "$kind" in
        ''|'#'*)
            continue
            ;;
    esac
    if [ -z "$symbol" ] || [ -z "$declaration_path" ] \
        || [ -z "$test_path" ] || [ -z "$coverage" ]; then
        printf '%s\n' "malformed coverage manifest row: $line_number" >&2
        exit 1
    fi
    if [ ! -f "$declaration_path" ]; then
        printf '%s\n' "coverage declaration file is missing at row $line_number: $declaration_path" >&2
        exit 1
    fi
    if [ ! -f "$test_path" ]; then
        printf '%s\n' "coverage test file is missing at row $line_number: $test_path" >&2
        exit 1
    fi
    if ! grep -F "$symbol" "$declaration_path" >/dev/null 2>&1; then
        printf '%s\n' "manifest symbol is not declared: $symbol" >&2
        exit 1
    fi
    if ! grep -F "$symbol" "$test_path" >/dev/null 2>&1; then
        printf '%s\n' "manifest symbol is not exercised by $test_path: $symbol" >&2
        exit 1
    fi
    if ! grep -E '^[[:space:]]*FT_TEST\([A-Za-z0-9_]+\)' \
        "$test_path" >/dev/null 2>&1; then
        printf '%s\n' "coverage test file has no registered tests: $test_path" >&2
        exit 1
    fi
    printf '%s\n' "$test_path" >> "$test_paths_file"
    entry_count=$((entry_count + 1))
done < "$manifest_path"

if [ "$entry_count" -eq 0 ]; then
    printf '%s\n' "coverage manifest has no entries: $manifest_path" >&2
    exit 1
fi

duplicate_symbol="$({
    awk -F'|' '/^[^#|]+\|[^|]+\|/ { print $2 }' "$manifest_path" \
        | sort | uniq -d
} || true)"
if [ -n "$duplicate_symbol" ]; then
    printf '%s\n' "duplicate coverage symbols:" >&2
    printf '%s\n' "$duplicate_symbol" >&2
    exit 1
fi

if [ -n "$test_executable" ]; then
    if [ ! -f "$test_executable" ]; then
        printf '%s\n' "coverage test executable is missing: $test_executable" >&2
        exit 1
    fi
    sort -u "$test_paths_file" | while IFS= read -r test_path; do
        test_names="$(sed -n 's/^[[:space:]]*FT_TEST(\([A-Za-z0-9_]*\)).*/\1/p' \
            "$test_path")"
        if [ -z "$test_names" ]; then
            printf '%s\n' "coverage test file has no executable tests: $test_path" >&2
            exit 1
        fi
        for test_name in $test_names; do
            FT_TEST_NAME_FILTER="$test_name" \
                FT_TEST_HIDE_SUCCESSFUL=1 "$test_executable" \
                > "$runtime_log_file" 2>&1 || true
            if grep -F '0/0 tests passed' "$runtime_log_file" \
                >/dev/null 2>&1; then
                continue
            fi
            if ! grep -E '[0-9]+/[0-9]+ tests passed' "$runtime_log_file" \
                >/dev/null 2>&1; then
                printf '%s\n' "coverage test did not execute successfully: $test_name" >&2
                cat "$runtime_log_file" >&2
                exit 1
            fi
            if ! grep -E '^[^0-9]*[1-9][0-9]*/[1-9][0-9]* tests passed' \
                "$runtime_log_file" >/dev/null 2>&1; then
                printf '%s\n' "coverage test failed: $test_name" >&2
                cat "$runtime_log_file" >&2
                exit 1
            fi
        done
    done
fi

printf '%s\n' "test coverage manifest passed: $manifest_path ($entry_count entries)"
