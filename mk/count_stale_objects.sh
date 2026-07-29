#!/usr/bin/env bash

stale_count=0

count_object_path()
{
    object_path="$1"
    dependency_path="${object_path%.o}.d"
    object_is_stale=0
    if [ ! -f "$object_path" ] || [ ! -f "$dependency_path" ]; then
        object_is_stale=1
    else
        dependency_list=$(sed ':a;N;$!ba;s/\\\n/ /g' "$dependency_path")
        dependency_list=${dependency_list#*:}
        for dependency in $dependency_list; do
            dependency=${dependency%:}
            if [ ! -f "$dependency" ] || [ "$dependency" -nt "$object_path" ]; then
                object_is_stale=1
                break
            fi
        done
    fi
    if [ "$object_is_stale" -eq 1 ]; then
        stale_count=$((stale_count + 1))
    fi
}

if [ "$#" -eq 2 ] && [ "$1" = "--file" ]; then
    while IFS= read -r object_path; do
        if [ -n "$object_path" ]; then
            count_object_path "$object_path"
        fi
    done < "$2"
else
    for object_path in "$@"; do
        count_object_path "$object_path"
    done
fi

printf '%s\n' "$stale_count"
