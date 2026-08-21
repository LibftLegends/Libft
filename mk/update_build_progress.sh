#!/usr/bin/env sh

set -u

progress_session_directory=${1-}
operation_type=${2-}
project_name=${3-}
module_name=${4-}
display_path=${5-}

if [ -z "$progress_session_directory" ] || [ -z "$operation_type" ] \
    || [ -z "$project_name" ] || [ -z "$module_name" ]
then
    exit 0
fi

lock_directory="$progress_session_directory/lock"
attempt_count=0

print_archive_ready()
{
    if [ "$project_name" = "libft" ]
    then
        if [ "$module_name" = "Full_Libft" ]
        then
            printf '\033[1;35m[LIBFT] Archive ready: %s\033[0m\n' \
                "$display_path"
        else
            printf '\033[1;35m[LIBFT][%s] Archive ready: %s\033[0m\n' \
                "$module_name" "$display_path"
        fi
    else
        printf '\033[1;35m[%s] Archive ready: %s\033[0m\n' \
            "$project_name" "$display_path"
    fi
}

while ! mkdir "$lock_directory" 2>/dev/null
do
    attempt_count=$((attempt_count + 1))
    if [ "$attempt_count" -ge 500 ]
    then
        printf '%s\n' '[BUILD PROGRESS] counter update skipped after lock timeout' >&2
        if [ "$operation_type" = "archive" ] && [ -n "$display_path" ]
        then
            print_archive_ready
        fi
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
    if [ "$operation_type" = "archive" ] && [ -n "$display_path" ]
    then
        print_archive_ready
    fi
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
if [ "$operation_type" = "compile" ] && [ -n "$display_path" ]
then
    if [ "$project_name" = "libft" ]
    then
        printf '[LIBFT][%s] Compiling %s (%s/%s files completed)\n' \
            "$module_name" "$display_path" "$completed_count" "$total_count" >&2
    else
        printf '[MINECRAFT] Compiling %s (%s/%s files completed)\n' \
            "$display_path" "$completed_count" "$total_count" >&2
    fi
else
    if [ "$operation_type" = "archive" ] && [ -n "$display_path" ]
    then
        if [ "$project_name" = "libft" ]
        then
            if [ "$module_name" = "Full_Libft" ]
            then
                printf '\033[1;35m[LIBFT] Archive ready: %s\033[0m %s/%s archives completed\n' \
                    "$display_path" "$completed_count" "$total_count"
            else
                printf '\033[1;35m[LIBFT][%s] Archive ready: %s\033[0m %s/%s archives completed\n' \
                    "$module_name" "$display_path" "$completed_count" "$total_count"
            fi
        else
            printf '\033[1;35m[%s] Archive ready: %s\033[0m %s/%s archives completed\n' \
                "$project_name" "$display_path" "$completed_count" "$total_count"
        fi
    else
        printf '\033[0m %s/%s %s completed\n' \
            "$completed_count" "$total_count" "$unit_name" >&2
    fi
fi
