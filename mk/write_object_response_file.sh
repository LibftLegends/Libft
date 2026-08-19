#!/usr/bin/env sh

set -eu

response_file=$1
object_root=$2
source_root=$3
shift 3

: > "$response_file"

object_directories=
while [ "$#" -gt 0 ]
do
    if [ "$1" = "--" ]; then
        shift
        break
    fi
    object_directories="$object_directories $1"
    shift
done
excluded_objects=$*

append_objects()
{
    if [ ! -d "$1" ]; then
        return 0
    fi
    find "$1" -maxdepth 1 -type f -name '*.o' -print | sort | while IFS= read -r object_file
    do
        relative_object=${object_file#"$object_root"/}
        source_file="$source_root/${relative_object%.o}.cpp"
        excluded=0
        for excluded_object in $excluded_objects
        do
            if [ "$object_file" = "$excluded_object" ]; then
                excluded=1
            fi
        done
        if [ "$excluded" -eq 0 ] && [ -f "$source_file" ]; then
            printf '%s\n' "$object_file" >> "$response_file"
        fi
    done
}

append_objects "$object_root"
for object_directory in $object_directories
do
    append_objects "$object_root/$object_directory"
done
