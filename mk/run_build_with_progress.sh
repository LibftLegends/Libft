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

# GNU Make otherwise forwards output from parallel recipes in separate chunks.
# Target-level synchronization keeps a progress record from being merged with
# the beginning or end of a different recipe in CI log collectors.
make_output_sync=''
if "$make_command" --help 2>/dev/null | grep -F 'output-sync' >/dev/null
then
    make_output_sync='--output-sync=target'
fi
"$make_command" --no-print-directory $make_output_sync -s \
    BUILD_PROGRESS_ACTIVE=1 \
    BUILD_PROGRESS_SESSION_DIR="$progress_session_directory" \
    "$build_target"
