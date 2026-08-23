#!/usr/bin/env sh

set -eu

archive_path="$1"
shift
manifest_path="$1"
shift

if [ "$#" -eq 0 ]; then
    printf 'archive rebuild check requires at least one object: %s\n' \
        "$archive_path" >&2
    exit 1
fi

archive_directory="$(dirname "$archive_path")"
obsolete_object="$archive_directory/archive_integrity_obsolete.o"
backup_archive="$archive_path.archive-integrity-backup.tmp"
backup_manifest="$manifest_path.archive-integrity-backup.tmp"
temporary_manifest="$manifest_path.archive-integrity-manifest.tmp"

restore_archive()
{
    if [ -f "$backup_archive" ]; then
        mv -f "$backup_archive" "$archive_path"
    fi
    if [ -f "$backup_manifest" ]; then
        mv -f "$backup_manifest" "$manifest_path"
    fi
    rm -f "$temporary_manifest"
    rm -f "$obsolete_object"
}

cleanup_success()
{
    mv -f "$backup_archive" "$archive_path"
    mv -f "$backup_manifest" "$manifest_path"
    rm -f "$temporary_manifest" "$obsolete_object"
}

trap restore_archive EXIT INT TERM HUP

cp -p "$archive_path" "$backup_archive"
cp -p "$manifest_path" "$backup_manifest"
cp "$1" "$obsolete_object"
ar rcs "$archive_path" "$obsolete_object"

if ! ar t "$archive_path" | tr -d '\r' | grep -Fx \
        "archive_integrity_obsolete.o" >/dev/null 2>&1; then
    printf 'could not seed stale archive member: %s\n' "$archive_path" >&2
    restore_archive
    exit 1
fi

# Remove one real source from the manifest so the test verifies that a stale
# archive member disappears when source membership changes.  The manifest is
# restored by both cleanup paths below, including after an interrupted build.
object_basename="$(basename "$1")"
source_stem="${object_basename%.o}"
source_file=""
for source_extension in cpp c mm; do
    candidate_source="${source_stem}.${source_extension}"
    if grep -Eq "(^|[[:space:]])${candidate_source}[[:space:]]*(\\\\)?[[:space:]]*$" \
            "$manifest_path"; then
        source_file="$candidate_source"
        break
    fi
done
if [ -z "$source_file" ]; then
    printf 'could not find source for archive object: %s\n' "$1" >&2
    restore_archive
    exit 1
fi
awk -v source_file="$source_file" '
index($0, source_file) == 0 {
    print
    next
}
index($0, ":=") != 0 {
    sub(source_file, "")
    print
    next
}
' "$manifest_path" > "$temporary_manifest"
mv -f "$temporary_manifest" "$manifest_path"
# Windows filesystems can report the archive and the temporarily rewritten
# manifest with the same timestamp.  Remove the backed-up archive so the
# exact rebuild is driven by target absence rather than timestamp resolution;
# restore_archive() still recovers it if the rebuild fails.
rm -f "$archive_path"
env -u MAKEFLAGS -u MFLAGS -u MAKE_JOBSERVER_FDS \
    make --no-print-directory -j1 "$archive_path"

if ar t "$archive_path" | tr -d '\r' | grep -Fx \
        "archive_integrity_obsolete.o" >/dev/null 2>&1; then
    printf 'stale archive member survived exact rebuild: %s\n' \
        "$archive_path" >&2
    restore_archive
    exit 1
fi

cleanup_success
printf 'archive stale-member rebuild passed: %s\n' "$archive_path"
