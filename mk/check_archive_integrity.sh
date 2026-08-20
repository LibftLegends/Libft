#!/usr/bin/env sh

set -eu

archive_path="$1"
shift

if [ ! -f "$archive_path" ]; then
    printf 'archive does not exist: %s\n' "$archive_path" >&2
    exit 1
fi

archive_members()
{
    ar t "$1" | tr -d '\r' | sed \
        -e '/^__\.SYMDEF$/d' \
        -e '/^__\.SYMDEF SORTED$/d'
}

actual_members="$(archive_members "$archive_path")"
expected_members=""
if [ "$1" = "--from-archives" ]; then
    shift
    for module_archive in "$@"; do
        expected_members="$expected_members$(archive_members "$module_archive")\n"
    done
else
    for object_path in "$@"; do
        expected_members="$expected_members$(basename "$object_path")\n"
    done
fi

actual_sorted="$(printf '%s\n' "$actual_members" | LC_ALL=C sort)"
expected_sorted="$(printf '%b' "$expected_members" | LC_ALL=C sort)"

archive_checksum()
{
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}

if [ "$actual_sorted" != "$expected_sorted" ]; then
    printf 'archive members do not match the current object graph: %s\n' \
        "$archive_path" >&2
    printf '%s\n' 'expected:' >&2
    printf '%s\n' "$expected_sorted" >&2
    printf '%s\n' 'actual:' >&2
    printf '%s\n' "$actual_sorted" >&2
    exit 1
fi

duplicate_members="$(printf '%s\n' "$actual_members" | LC_ALL=C sort | \
    uniq -d)"
if [ -n "$duplicate_members" ]; then
    printf 'duplicate archive members: %s\n' "$archive_path" >&2
    printf '%s\n' "$duplicate_members" >&2
    exit 1
fi

temporary_archive="$archive_path.integrity-failure.tmp"
rm -f "$temporary_archive"
original_checksum="$(archive_checksum "$archive_path")"
if ar rcs "$temporary_archive" \
    "$archive_path.integrity-object-that-does-not-exist.o" 2>/dev/null; then
    printf 'archiver unexpectedly accepted a missing object: %s\n' \
        "$archive_path" >&2
    rm -f "$temporary_archive"
    exit 1
fi
current_checksum="$(archive_checksum "$archive_path")"
rm -f "$temporary_archive"
if [ "$original_checksum" != "$current_checksum" ]; then
    printf 'failed archive replacement changed the valid archive: %s\n' \
        "$archive_path" >&2
    exit 1
fi

printf 'archive integrity passed: %s\n' "$archive_path"
