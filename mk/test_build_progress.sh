#!/usr/bin/env sh

set -eu

make_command=${1:-make}
source_file='Modules/Basic/basic_memcpy.cpp'
backup_file=$(mktemp "${TMPDIR:-/tmp}/libft-progress-source.XXXXXX")

cleanup()
{
    if [ -f "$backup_file" ]
    then
        cp -p "$backup_file" "$source_file"
    fi
    rm -f "$backup_file"
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

cp -p "$source_file" "$backup_file"
touch "$source_file"

build_output=$(sh mk/run_build_with_progress.sh \
    "$make_command" Modules/Basic/Basic.a)
printf '%s\n' "$build_output"

printf '%s\n' "$build_output" \
    | grep -F '[BUILD PLAN] 1 source files require rebuilding' >/dev/null
printf '%s\n' "$build_output" \
    | grep -F '[BUILD PROGRESS][libft/Basic] 1/1 files completed' >/dev/null
printf '%s\n' "$build_output" \
    | grep -F '[BUILD PROGRESS][archives] 1/1 archives completed' >/dev/null

plan_output=$(sh mk/print_build_plan.sh \
    "$make_command" Modules/Basic/Basic.a)
printf '%s\n' "$plan_output"
printf '%s\n' "$plan_output" \
    | grep -F '[BUILD PLAN] 0 source files require rebuilding' >/dev/null
printf '%s\n' "$plan_output" \
    | grep -F '[BUILD PLAN] 0 archives and 0 link targets require work' >/dev/null
