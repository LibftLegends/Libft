#!/bin/sh

set +e

child_process_id=""

abort_child_on_cancel()
{
    if [ -n "$child_process_id" ]; then
        printf '%s\n' "CI cancellation received; sending SIGABRT to test process ${child_process_id}." >&2
        kill -ABRT "$child_process_id" 2>/dev/null
        sleep 5
        kill -0 "$child_process_id" 2>/dev/null
        if [ "$?" -eq 0 ]; then
            printf '%s\n' "Test process did not exit after SIGABRT; sending SIGKILL." >&2
            kill -KILL "$child_process_id" 2>/dev/null
        fi
    fi
    exit 128
}

trap abort_child_on_cancel TERM INT HUP
ulimit -c unlimited
"$@" &
child_process_id="$!"
wait "$child_process_id"
test_status="$?"
child_process_id=""
trap - TERM INT HUP
exit "$test_status"
