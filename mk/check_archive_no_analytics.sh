#!/usr/bin/env sh

set -eu

archive_path=${1-}
if [ -z "$archive_path" ] || [ ! -f "$archive_path" ]; then
    printf 'archive does not exist: %s\n' "$archive_path" >&2
    exit 1
fi

analytics_members="$(ar t "$archive_path" | \
    grep -E '(^|/)analytics([_.]|$)' || true)"
if [ -n "$analytics_members" ]; then
    printf 'analytics objects leaked into normal archive: %s\n' \
        "$archive_path" >&2
    printf '%s\n' "$analytics_members" >&2
    exit 1
fi

printf 'normal archive contains no analytics objects: %s\n' "$archive_path"
