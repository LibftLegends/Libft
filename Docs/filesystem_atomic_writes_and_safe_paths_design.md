# Filesystem Atomic Writes and Safe Path Design

## Scope

This document defines the safety contract for atomic persistence and root-bound
path validation in Libft. It separates atomic visibility from crash durability
and separates lexical path checks from checks against the filesystem objects
that a path actually resolves to.

## Atomic write contract

`filesystem_atomic_write()` and `file_write_all_atomic()` must never use a shared name such as
`<target>.tmp`. It creates a random, exclusive temporary file in the target's
directory, writes the complete payload, closes it, and then moves it over the
target. Keeping the temporary file in the same directory preserves the
same-filesystem requirement for an atomic rename.

The write loop must accept short writes and continue until the requested byte
count has been written. A successful return means that readers observe either
the old complete file or the new complete file; they must not observe a
partially written replacement. It does not, by itself, promise that the
replacement survives a sudden power loss.

On Windows, concurrent replacement can transiently report sharing, lock, or
access-denied errors while another writer is replacing the destination. The
compatibility move layer retries only those transient errors for a bounded
period; a persistent access failure is still returned to the caller.

`file_replace_safe()` is the durable variant. It flushes the temporary file
before replacement and flushes the containing directory where the platform
supports that operation. A directory-sync failure is returned to the caller;
it must not be silently discarded.

## Temporary-file error handling

`file_secure_temp_file()` retries only when the exclusive open failed because
the generated name already exists. Permission errors, missing directories,
invalid paths, descriptor exhaustion, read-only filesystems, and path-length
errors are returned immediately through the normal Libft error mapping. After
the collision retry budget is exhausted, `FT_ERR_ALREADY_EXISTS` is valid;
unrelated errors must never be converted to that code.

## Root containment

`filesystem_safe_join_path()` is a lexical helper. It rejects absolute paths
and parent traversal after normalization, but it is not a filesystem sandbox:
a symlink or Windows junction below the root can redirect a lexically safe path
elsewhere.

Security-sensitive access must use canonical containment validation. The root
and candidate are resolved through the platform filesystem:

- POSIX uses `realpath()` and therefore follows symlinks in existing paths.
- Windows opens the object with backup semantics and compares
  `GetFinalPathNameByHandleA()` results, resolving symlinks and reparse-point
  redirections before comparison.

`file_validate_regular_file_inside_root()` additionally requires the
candidate to be an existing regular file. The canonical comparison must occur
before access, and callers must understand that path validation followed by a
separate open is still vulnerable to a replace-between-check-and-use race.
Future sandbox APIs should use descriptor/handle-relative operations (`openat`
with no-follow restrictions on POSIX and equivalent handle-relative Windows
APIs) when the threat model includes an attacker who can modify the directory.

## Required tests

1. Run concurrent atomic writers against one destination. Every final result
   must equal one complete writer payload, never a mixture or truncation.
2. Run concurrent `file_replace_safe()` writers against one destination as
   well. Verify that every completed replacement is a complete payload and
   that the durable path does not regress to the shared-temp-file race.
3. Exercise empty payloads, short writes, missing parents, invalid paths,
   permission failures, and target replacement.
4. Force secure-temp name collisions and verify retry behavior; force each
   other open failure and verify it is returned immediately and is not reported
   as `FT_ERR_ALREADY_EXISTS`.
5. Validate lexical traversal, sibling-prefix escapes, symlink escapes, and
   Windows junction/reparse-point escapes. Include a positive canonical
   descendant case and reject directories, missing files, and redirected files.
6. Test durable replacement's file-sync and directory-sync failures with the
   compatibility-layer failure hooks. Atomic-visibility and durable-result
   semantics must remain distinguishable.
7. Run the filesystem tests under ASan/UBSan and TSan, including the concurrent
   writer test. No test may rely on the aggregate test executable being rebuilt
   while another process has it open.

## Current implementation status

The unique temporary-file write path, short-write loops, selective retry/error
mapping, directory-sync propagation, and platform-aware canonical containment
are implemented. Lexical safe-join remains intentionally non-security-boundary
API surface, and handle-relative open/validation is a future hardening step for
TOCTOU-resistant sandboxes.
