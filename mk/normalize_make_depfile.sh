#!/usr/bin/env sh

set -eu

normalize_dependency_file()
{
    dependency_file=$1
    already_matched=${2:-0}
    if [ ! -f "$dependency_file" ] \
        || { [ "$already_matched" != "1" ] \
            && ! grep -E '(^|[[:space:]])[[:alpha:]]:/' \
                "$dependency_file" >/dev/null 2>&1; }; then
        return 0
    fi

    if sed --version >/dev/null 2>&1; then
        sed -i -E 's/(^|[[:space:]])([[:alpha:]]):\//\1\2\\:\//g' \
            "$dependency_file"
    else
        sed -i '' -E 's/(^|[[:space:]])([[:alpha:]]):\//\1\2\\:\//g' \
            "$dependency_file"
    fi
    return 0
}

if [ "$#" -eq 2 ] && [ "$1" = '--tree' ]; then
    dependency_root=$2
    if [ ! -d "$dependency_root" ]; then
        exit 0
    fi
    migration_stamp="$dependency_root/.windows_depfile_colons_normalized_v1"
    if [ -f "$migration_stamp" ]; then
        exit 0
    fi
    find "$dependency_root" -type f -name '*.d' \
        -exec grep -lE '(^|[[:space:]])[[:alpha:]]:/' {} + \
        | while IFS= read -r dependency_file; do
            normalize_dependency_file "$dependency_file" 1
        done
    : >"$migration_stamp"
    exit 0
fi

if [ "$#" -ne 1 ]; then
    printf '%s\n' 'usage: normalize_make_depfile.sh <file> | --tree <root>' >&2
    exit 2
fi
normalize_dependency_file "$1"
