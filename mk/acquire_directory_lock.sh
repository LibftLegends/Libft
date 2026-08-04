#!/usr/bin/env bash

set -u

lock_directory="$1"
retry_limit="${2:-500}"
retry_count=0

while ! mkdir "$lock_directory" 2>/dev/null; do
    retry_count=$((retry_count + 1))
    if [ "$retry_count" -ge "$retry_limit" ]; then
        rmdir "$lock_directory" 2>/dev/null || true
        retry_count=0
    fi
    sleep 0.02
done
