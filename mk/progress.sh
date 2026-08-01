#!/usr/bin/env bash
set -u

state_dir="Test/.libft_progress/build"
lock_dir="Test/.libft_progress.build.lock"
completion_count_file="$state_dir/completion_count"
bar_width=24
if [ -t 1 ]; then
    color_on=$'\033[1;35m'
    color_off=$'\033[0m'
else
    color_on=""
    color_off=""
fi

lock_progress() {
    while ! mkdir "$lock_dir" 2>/dev/null; do
        sleep 0.02
    done
}

unlock_progress() {
    rmdir "$lock_dir" 2>/dev/null || true
}

make_bar() {
    local current="$1"
    local total="$2"
    local filled empty

    if [ "$total" -le 0 ]; then
        total=1
    fi
    filled=$((current * bar_width / total))
    if [ "$filled" -gt "$bar_width" ]; then
        filled="$bar_width"
    fi
    empty=$((bar_width - filled))
    printf '%*s' "$filled" '' | tr ' ' '#'
    printf '%*s' "$empty" '' | tr ' ' '-'
}

short_name() {
    local module="$1"
    module=${module#Modules/}
    module=${module%/*.a}
    module=${module%_debug}
    module=${module%_test}
    printf '%s' "$module"
}

init_progress() {
    local total="$1"
    local key="${2:-build}"

    state_dir="Test/.libft_progress/$key"
    lock_dir="Test/.libft_progress.$key.lock"
    completion_count_file="$state_dir/completion_count"

    rm -f Test/.libft_build_status_* Test/.libft_build_*.raw.*
    rmdir "$lock_dir" 2>/dev/null || true
    mkdir -p "$state_dir"
    lock_progress
    if [ "$total" -eq 0 ]; then
        printf '%s[LIBFT BUILD]%s All modules are up to date\n' "$color_on" "$color_off"
    else
        printf '%s[LIBFT BUILD]%s Work required in %s module(s)\n' "$color_on" "$color_off" "$total"
    fi
    : > "$state_dir/initialized"
    printf '%s\n' "0" > "$completion_count_file"
    unlock_progress
}

render_line() {
    local total_modules="$1"
    local index="$2"
    local module="$3"
    local current="$4"
    local total_files="$5"
    local state="$6"
    local file="$7"
    local bar name percent

    if [ "$index" -le 0 ]; then
        return 0
    fi
    bar="$(make_bar "$current" "$total_files")"
    name="$(short_name "$module")"
    if [ "$total_files" -le 0 ]; then
        percent=0
    else
        percent=$((current * 100 / total_files))
    fi
    lock_progress
    printf '%s[LIBFT %02d/%02d]%s %-22s [%s] %3d%% %s %d/%d' \
        "$color_on" "$index" "$total_modules" "$color_off" "$name" "$bar" "$percent" "$state" "$current" "$total_files"
    if [ -n "$file" ]; then
        printf ' :: %.58s' "$file"
    fi
    printf '\n'
    unlock_progress
}

finish_progress() {
    local key="${1:-build}"

    state_dir="Test/.libft_progress/$key"
    lock_dir="Test/.libft_progress.$key.lock"
    if [ -d "$state_dir" ]; then
        lock_progress
        rm -rf "$state_dir"
        unlock_progress
    fi
    rmdir "$lock_dir" 2>/dev/null || true
}

case "${1:-}" in
    init)
        shift
        init_progress "$@"
        ;;
    update)
        shift
        render_line "$@"
        ;;
    finish)
        finish_progress
        ;;
    *)
        printf 'usage: %s init|update|finish ...\n' "$0" >&2
        exit 2
        ;;
esac
