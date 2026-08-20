#!/usr/bin/env sh

set -u

progress_session_directory=${1-}
operation_type=${2-}
project_name=${3-}
module_name=${4-}

if [ -z "$progress_session_directory" ] || [ -z "$operation_type" ] \
    || [ -z "$project_name" ] || [ -z "$module_name" ]
then
    exit 0
fi

lock_directory="$progress_session_directory/lock"
attempt_count=0

while ! mkdir "$lock_directory" 2>/dev/null
do
    attempt_count=$((attempt_count + 1))
    if [ "$attempt_count" -ge 500 ]
    then
        printf '%s\n' '[BUILD PROGRESS] counter update skipped after lock timeout' >&2
        exit 0
    fi
    sleep 0.01
done

release_lock()
{
    rmdir "$lock_directory" 2>/dev/null || true
}

trap release_lock EXIT
trap 'exit 0' HUP INT TERM

if [ "$operation_type" = "compile" ]
then
    total_file="$progress_session_directory/total.compile.$project_name.$module_name"
    completed_file="$progress_session_directory/completed.compile.$project_name.$module_name"
    label="$project_name/$module_name"
    unit_name='files'
else
    total_file="$progress_session_directory/total.archive"
    completed_file="$progress_session_directory/completed.archive"
    label='archives'
    unit_name='archives'
fi

if [ ! -f "$total_file" ]
then
    exit 0
fi

total_count=$(sed -n '1p' "$total_file" 2>/dev/null || true)
if [ -z "$total_count" ]
then
    exit 0
fi

completed_count=0
if [ -f "$completed_file" ]
then
    completed_count=$(sed -n '1p' "$completed_file" 2>/dev/null || true)
fi
if [ -z "$completed_count" ]
then
    completed_count=0
fi

completed_count=$((completed_count + 1))
printf '%s\n' "$completed_count" > "$completed_file"
printf '\033[1;32m[BUILD PROGRESS][%s] %s/%s %s completed\033[0m\n' \
    "$label" "$completed_count" "$total_count" "$unit_name"
