#!/usr/bin/env sh

set -eu

timeout_seconds=$1
shift
if [ "$#" -eq 0 ]; then
    printf '%s\n' 'ci_run_with_timeout: missing command' >&2
    exit 2
fi

"$@" &
command_pid=$!
elapsed_seconds=0
heartbeat_seconds=${LIBFT_CI_HEARTBEAT_SECONDS:-30}
heartbeat_elapsed=0

while kill -0 "$command_pid" >/dev/null 2>&1; do
    sleep 1
    elapsed_seconds=$((elapsed_seconds + 1))
    heartbeat_elapsed=$((heartbeat_elapsed + 1))
    if kill -0 "$command_pid" >/dev/null 2>&1; then
        if [ "$heartbeat_elapsed" -ge "$heartbeat_seconds" ]; then
            printf '%s\n' "CI heartbeat: command still running after ${elapsed_seconds}s: $*"
            heartbeat_elapsed=0
        fi
    fi
    if [ "$elapsed_seconds" -ge "$timeout_seconds" ]; then
        printf '%s\n' "CI timeout: terminating command after ${timeout_seconds}s: $*" >&2
        kill -TERM "$command_pid" >/dev/null 2>&1 || true
        sleep 5
        kill -KILL "$command_pid" >/dev/null 2>&1 || true
        wait "$command_pid" >/dev/null 2>&1 || true
        exit 124
    fi
done

wait "$command_pid"
