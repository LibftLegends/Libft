#!/bin/sh

set -eu

manifest_path="${1:-docs/test_coverage/test_failure_controller.tsv}"

if [ ! -f "$manifest_path" ]; then
    printf '%s\n' "coverage manifest is missing: $manifest_path" >&2
    exit 1
fi
line_number=0
entry_count=0
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

printf '%s\n' "test coverage manifest passed: $manifest_path ($entry_count entries)"
