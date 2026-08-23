#!/usr/bin/env sh

set -eu

if [ "$#" -lt 2 ]; then
    printf '%s\n' 'archive no-op check requires a mode and state file' >&2
    exit 1
fi

check_mode="$1"
shift
state_path="$1"
shift

archive_timestamp()
{
    timestamp="$(stat -c '%Y' "$1" 2>/dev/null || true)"
    if [ -n "$timestamp" ]; then
        printf '%s\n' "$timestamp"
        return 0
    fi
    stat -f '%m' "$1"
}

if [ "$check_mode" = "capture" ]; then
    : > "$state_path"
    for archive_path in "$@"; do
        if [ ! -f "$archive_path" ]; then
            printf 'archive does not exist: %s\n' "$archive_path" >&2
            exit 1
        fi
        printf '%s|%s\n' "$archive_path" "$(archive_timestamp "$archive_path")" \
            >> "$state_path"
    done
    exit 0
fi

if [ "$check_mode" != "verify" ]; then
    printf 'unknown archive no-op check mode: %s\n' "$check_mode" >&2
    exit 1
fi

while IFS='|' read -r archive_path before_timestamp; do
    if [ -z "$archive_path" ]; then
        continue
    fi
    after_timestamp="$(archive_timestamp "$archive_path")"
    if [ "$before_timestamp" != "$after_timestamp" ]; then
        printf 'no-op build changed archive timestamp: %s\n' \
            "$archive_path" >&2
        exit 1
    fi
done < "$state_path"

printf '%s\n' 'archive no-op timestamp check passed'
