#!/usr/bin/env sh

set -eu

make_command=$1
build_target=$2
progress_session_directory=${3-}
plan_output=$(mktemp "${TMPDIR:-/tmp}/libft-build-plan.XXXXXX")

cleanup()
{
    rm -f "$plan_output"
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

if "$make_command" --no-print-directory -n BUILD_PLAN_MODE=1 "$build_target" >"$plan_output"
then
    :
else
    status=$?
    cat "$plan_output"
    exit "$status"
fi

if [ -n "$progress_session_directory" ] && [ ! -d "$progress_session_directory" ]
then
    printf '%s\n' "[BUILD PLAN] progress session directory does not exist: $progress_session_directory" >&2
    exit 1
fi

awk -v state_directory="$progress_session_directory" '
index($0, "__BUILD_PLAN__|") == 0 {
    next
}
{
    line = $0
    sub(/^.*__BUILD_PLAN__\|/, "", line)
    sub(/^"/, "", line)
    sub(/".*$/, "", line)
    split(line, fields, "|")
    if (fields[1] == "compile")
    {
        total_compile += 1
        project_compile[fields[2]] += 1
        module_key = fields[2] SUBSEP fields[3]
        module_compile[module_key] += 1
        if (!(module_key in module_seen))
        {
            module_seen[module_key] = 1
            module_order[++module_count] = module_key
        }
    }
    else if (fields[1] == "archive")
    {
        total_archive += 1
    }
    else if (fields[1] == "link")
    {
        total_link += 1
    }
}
END {
    printf "[BUILD PLAN] %d source files require rebuilding\n", total_compile + 0
    printf "[BUILD PLAN] %d archives and %d link targets require work\n", total_archive + 0, total_link + 0
    if ((project_compile["minecraft"] + 0) > 0)
        printf "Minecraft: %d\n", project_compile["minecraft"] + 0
    if ((project_compile["libft"] + 0) > 0)
        printf "Libft: %d\n", project_compile["libft"] + 0
    for (position = 1; position <= module_count; position += 1)
    {
        split(module_order[position], module_fields, SUBSEP)
        printf "%s/%s: %d\n", module_fields[1], module_fields[2], module_compile[module_order[position]] + 0
    }
    if (state_directory != "")
    {
        state_file = state_directory "/total.archive"
        printf "%d\n", total_archive + 0 > state_file
        close(state_file)
        for (position = 1; position <= module_count; position += 1)
        {
            split(module_order[position], module_fields, SUBSEP)
            state_file = state_directory "/total.compile." module_fields[1] "." module_fields[2]
            printf "%d\n", module_compile[module_order[position]] + 0 > state_file
            close(state_file)
        }
    }
}
' "$plan_output"
