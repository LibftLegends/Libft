#!/usr/bin/env sh

set -eu

archive_path=$1
shift

if [ ! -f "$archive_path" ]; then
    exit 1
fi

archive_members()
{
    ar t "$1" | tr -d '\r' | sed \
        -e '/^__\.SYMDEF$/d' \
        -e '/^__\.SYMDEF SORTED$/d'
}

actual_members=$(archive_members "$archive_path" | LC_ALL=C sort)
if [ "$#" -eq 0 ]; then
    expected_members=''
elif [ "$1" = "--from-archives" ]; then
    shift
    expected_members=$(printf '%s\n' "$@" | while IFS= read -r module_archive; do
        archive_members "$module_archive"
    done | LC_ALL=C sort)
else
    expected_members=$(printf '%s\n' "$@" | while IFS= read -r object_path; do
        basename "$object_path"
    done | LC_ALL=C sort)
fi

if [ "$actual_members" != "$expected_members" ]; then
    exit 1
fi
if [ -n "$(printf '%s\n' "$actual_members" | LC_ALL=C sort | uniq -d)" ]; then
    exit 1
fi
exit 0
