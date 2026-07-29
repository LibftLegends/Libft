#!/usr/bin/env bash
set -u

total_modules="$1"
progress_index="$2"
module_path="$3"
log_file="$4"
shift 4
if [ "${1:-}" = "--" ]; then
    shift
fi

batch_output="${LIBFT_BATCH_OUTPUT:-0}"
light_blue=""
purple=""
reset=""
if [ -t 1 ]; then
    light_blue=$'\033[1;94m'
    purple=$'\033[1;35m'
    reset=$'\033[0m'
fi

status_file="Test/.libft_build_status_$$_${progress_index}"
progress_state_dir="Test/.libft_progress"
completion_count_file="$progress_state_dir/completion_count"
progress_lock_dir="Test/.libft_progress.lock"
raw_log_file="${log_file}.raw.$$"
child_pid=""

cleanup_all_files() {
    if [ -n "$child_pid" ]; then
        if command -v taskkill.exe >/dev/null 2>&1; then
            taskkill.exe /T /F /PID "$child_pid" >/dev/null 2>&1 || true
        else
            kill "$child_pid" 2>/dev/null || true
        fi
        wait "$child_pid" 2>/dev/null || true
        child_pid=""
    fi
    rm -f "$status_file" "$log_file"
    rm -f "$raw_log_file"
    rmdir "$progress_lock_dir" 2>/dev/null || true
}

trap cleanup_all_files EXIT
trap 'cleanup_all_files; exit 130' HUP INT TERM

: > "$log_file"
rm -f "$status_file"
rm -f "$raw_log_file"

module_name=$(basename "$module_path")
module_name=${module_name%.a}
module_name=${module_name%_debug}
module_name=${module_name%_test}

"$@" > "$raw_log_file" 2>&1 &
child_pid=$!
wait "$child_pid"
status=$?
child_pid=""
printf '%s\n' "$status" > "$status_file"

stale_total=$(sed -n '/Building file /p' "$raw_log_file" | wc -l | tr -d ' ')
if [ "$stale_total" -eq 0 ]; then
    stale_total=1
fi
stale_count=0

while IFS= read -r line; do
    printf '%s\n' "$line" >> "$log_file"
    case "$line" in
        *" is up to date.")
            continue
            ;;
        *"Building file "*)
            file=$(printf '%s\n' "$line" | sed -n 's/.*Building file \(.*\) ([0-9][0-9]*\/[0-9][0-9]*).*/\1/p')
            stale_count=$((stale_count + 1))
            if [ "$batch_output" -ne 1 ]; then
                printf '%s        [%s] Building file %s (%d/%d)%s\n' \
                    "$light_blue" "$module_name" "$file" "$stale_count" "$stale_total" "$reset"
            fi
            ;;
        *"Module archive ready "*)
            :
            ;;
        *)
            if [ "$batch_output" -ne 1 ]; then
                printf '%s\n' "$line"
            fi
            ;;
    esac
done < "$raw_log_file"
rm -f "$raw_log_file"

if [ "$status" -eq 0 ]; then
    while ! mkdir "$progress_lock_dir" 2>/dev/null; do
        sleep 0.02
    done
    if [ ! -f "$completion_count_file" ]; then
        mkdir -p "$progress_state_dir"
        printf '%s\n' "0" > "$completion_count_file"
    fi
    completion_index=$(cat "$completion_count_file")
    if [ -z "$completion_index" ]; then
        completion_index=0
    fi
    completion_index=$((completion_index + 1))
    printf '%s\n' "$completion_index" > "$completion_count_file"
    rmdir "$progress_lock_dir" 2>/dev/null || true
    printf '%s[LIBFT BUILD]%s (%d/%d) Built %s%s\n' \
        "$purple" "$reset" "$completion_index" "$total_modules" "$(basename "$module_path")" "$reset"
    rm -f "$log_file"
else
    printf '%s[LIBFT BUILD]%s (%d/%d) Failed %s%s\n' \
        "$purple" "$reset" "$progress_index" "$total_modules" "$(basename "$module_path")" "$reset"
    cat "$log_file"
    rm -f "$log_file"
fi
exit "$status"
