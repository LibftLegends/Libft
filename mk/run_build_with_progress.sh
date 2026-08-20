#!/usr/bin/env sh

set -eu

make_command=$1
build_target=$2
script_directory=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
progress_session_directory=$(mktemp -d "${TMPDIR:-/tmp}/libft-build-progress.XXXXXX")

cleanup()
{
    rm -rf "$progress_session_directory"
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

sh "$script_directory/print_build_plan.sh" \
    "$make_command" "$build_target" "$progress_session_directory"

"$make_command" --no-print-directory -s \
    BUILD_PROGRESS_ACTIVE=1 \
    BUILD_PROGRESS_SESSION_DIR="$progress_session_directory" \
    "$build_target"
