# Server-Authoritative World Delta Design

## Status and implementation boundary

The chunk-state and wire-format portions of this design are implemented in the
Game module. The implementation includes compact current-state overrides,
revisioned requests and deltas, transactional chunk loading, version-4
migration, and bounded recent-delta retention. The surrounding game server is
still responsible for authenticating requests, resolving world/chunk objects,
maintaining client interest sets, and publishing accepted deltas through its
network transport.
The transport-neutral `game_world_delta_interest_set` now implements the
snapshot-pending to live-subscription handoff and live-client collection; the
socket/server adapter can use it without coupling chunk state to a transport.
`game_world_delta_channel` combines those pieces with one chunk's authoritative
mutation, recent-delta recovery, and snapshot serialization boundary.
Snapshot helpers wrap the chunk payload with a bounded length and checksum and
reject corruption before invoking transactional chunk replacement.

The goal is to replace the current chunk dirty-edit journal with a compact
representation of the current player-modified world state and a reliable,
server-authoritative synchronization protocol. The chunk journal has been
removed after the replacement persistence and network tests were added.
The chunk authority path also retains the most recent request result for
session/request-id retries; an identical retry returns the original delta,
while reusing that pair with a different payload is rejected.

The ordinary `_dirty` flag on `game_voxel_chunk` is not part of the dirty-edit
journal. It remains useful for deciding whether a chunk needs persistence and
must not be removed.

## Goals

- The server is the only authority allowed to commit map changes.
- Clients request changes but never directly author the canonical world state.
- Every interested client eventually observes the same current block state.
- A newly connected or recovered client can obtain a full chunk snapshot and
  then consume newer deltas without a race window.
- The system distinguishes naturally generated blocks from blocks whose current
  state was selected by a player.
- Only the current override is retained. Historical intermediate edits are not
  retained by this design.
- Player overrides survive chunk unloading, saving, loading, regeneration, and
  server restarts.
- Generated terrain can be reconstructed from the world seed, generator
  version, and configuration signature.
- The protocol detects missing, duplicated, stale, and out-of-order messages.
- Storage and network formats use explicit fixed-width fields and bounded input.

## Non-goals

- Undo/redo history.
- Replaying every edit a player ever performed.
- Client-authoritative or peer-to-peer world mutation.
- Conflict-free replicated data types.
- Encoding modification provenance by reducing the supported block registry to
  127 entries.
- Reusing the persistence dirty flag as a network acknowledgement mechanism.

## Core world-state model

Each block has two conceptual values:

1. The generated baseline value.
2. The current authoritative value.

A block is player-modified when the current value differs from the generated
baseline because the server accepted a player edit. The implementation stores
only the final override, not every transition.

Example:

```text
generated baseline: stone
accepted player value: air
current authoritative value: air
player modified: true
```

If the player later places dirt, the override changes from air to dirt. The air
transition is not retained. If the player restores stone, the override is
removed because the current value once again equals the generated baseline.

### Compact representation

Do not overload the public block registry ID by adding 128 or setting a bit in
the registry ID itself. Libft block IDs are wider than eight bits, and tagged
IDs can accidentally enter registry lookups, palette comparisons, scripting,
or serialization as if they were real block IDs.

Use one of these internal representations:

- Preferred: the normal current block palette plus a one-bit modified mask for
  each block in a materialized section.
- Alternative for very sparse changes: a sorted sparse array keyed by the
  12-bit section-local block index and containing the current block ID.

The preferred bit mask costs 512 bytes for a fully materialized 4,096-block
section. Allocate it lazily only after the first player override. Uniform and
unmodified sections allocate no provenance mask.

If the generated baseline can be reproduced exactly, the current block palette
and modified mask are sufficient at runtime. If the generator implementation
cannot promise exact reproduction forever, persist either the generated
baseline snapshot or enough versioned generation data to invoke the original
generator implementation.

### Required generation identity

Every persisted chunk snapshot must retain:

- world seed;
- generator version;
- terrain configuration signature;
- world chunk coordinates;
- completed generation-stage mask;
- biome identity where relevant.

Changing generator behavior requires incrementing the generator version. A
server must not silently reconstruct an old baseline with a newer generator.
When the old generator is unavailable, loading must use a persisted baseline
snapshot or return an explicit incompatibility error.

## Authoritative mutation API

There must be one authoritative server-side operation for player map changes:

```cpp
int32_t apply_authoritative_block_change(
    const game_block_change_request &request,
    game_block_delta *accepted_delta) noexcept;
```

The operation must transactionally:

1. Validate the request and player permissions.
2. Resolve and load the target chunk.
3. Read the generated baseline and current block value.
4. Apply the new current block value.
5. Set or clear the player-modified marker.
6. Increment the chunk revision.
7. Mark the chunk persistence-dirty.
8. Produce the accepted authoritative delta.
9. Queue the delta for interested clients.

If any fallible step before commit fails, the chunk, revision, and outgoing
queues remain unchanged. Queue allocation must be prepared before committing
the block write, or the committed state must be recoverable through an
immediate snapshot-required marker.

World generation uses a separate API such as `write_generated_block()` and
must never set the player-modified marker. Administrative changes must specify
their provenance explicitly instead of accidentally appearing as generation.

## Client request protocol

A client sends intent rather than a committed delta:

```text
game_block_change_request
    protocol_version:        u16
    connection_session_id:   u64
    request_id:              u64
    world_id:                u64
    chunk_x:                 i32
    chunk_z:                 i32
    local_x:                 u8
    local_y:                 u16
    local_z:                 u8
    expected_chunk_revision: u64
    expected_current_block:  u32
    requested_block:         u32
```

The request ID is monotonically increasing within a connection session and is
used for idempotency. Repeating the same request ID must return the original
result without applying the change twice.

The server validates at least:

- authenticated player and active session;
- world and chunk existence;
- coordinate bounds;
- registered requested block ID;
- distance/reach constraints;
- permissions and game-mode rules;
- rate limits;
- inventory or resource requirements;
- expected current block and chunk revision;
- chunk readiness and generation state.

Rejected requests return a reason and the current chunk revision. When useful,
the response also includes the authoritative current block so the client can
repair local prediction.

## Authoritative delta protocol

After acceptance, the server assigns the delta's revision and broadcasts it to
all clients whose interest sets contain the chunk, including the originating
client.

```text
game_block_delta
    protocol_version:       u16
    world_id:               u64
    chunk_x:                i32
    chunk_z:                i32
    local_x:                u8
    local_y:                u16
    local_z:                u8
    previous_chunk_revision:u64
    chunk_revision:         u64
    current_block:          u32
    player_modified:        u8
    server_tick:            u64
    originating_session_id: u64
    originating_request_id: u64
```

`chunk_revision` increases by exactly one for each committed authoritative
change to that chunk. The client applies a delta only when
`previous_chunk_revision` equals its current local revision.

- An older or duplicate revision is ignored and acknowledged again.
- A future revision indicates a gap and triggers delta recovery or a snapshot.
- An invalid modified marker or coordinate rejects the entire message.
- The block ID is validated before modifying client state.

Block deltas require reliable, ordered delivery. They may share the Libft
message transport with unreliable traffic, but must use a reliable ordered lane
whose priority is above bulk chunk snapshots and below latency-critical control
messages such as disconnect or authentication failure.

## Interest management and broadcasting

The server maintains an interest set for every client. A chunk enters the set
when it is within the configured view/subscription distance and leaves after a
small hysteresis margin to prevent subscription thrashing.

The server broadcasts a delta only to clients currently subscribed to the
chunk. A client entering an interest set receives a snapshot before receiving
live deltas for that chunk.

Snapshot handoff must avoid a race:

1. Server captures chunk snapshot at revision `R`.
2. Server marks the client subscription as snapshot-pending.
3. Deltas newer than `R` are retained for that client or chunk.
4. Client validates and installs the snapshot.
5. Client acknowledges snapshot revision `R`.
6. Server sends retained deltas beginning at `R + 1`.
7. Subscription becomes live after the client reaches the current revision.

Do not broadcast world changes while holding the chunk mutation lock. Produce
an immutable delta, commit it to the server queue, release world locks, and then
perform network work.

## Client prediction

Client prediction is optional. If enabled, predicted changes are stored outside
the authoritative chunk representation and keyed by request ID. They must not
set the authoritative player-modified mask.

When the accepted delta arrives:

- matching prediction: remove the prediction and install the authoritative
  revision;
- changed result: replace the prediction with the server value;
- rejection: remove the prediction and restore the authoritative value;
- timeout: request current block or chunk state rather than assuming success.

Other clients never receive predicted values.

## Delta retention, acknowledgement, and recovery

The authoritative chunk state and compact overrides are the source of truth.
The network delta queue is only a bounded delivery optimization and may be
discarded after acknowledgement or snapshot recovery.

Maintain a bounded per-chunk recent-delta ring keyed by revision. It must never
silently affect persistence correctness. If a client requests a revision older
than the retained range, send a snapshot instead.

Clients periodically acknowledge the highest contiguous revision for each
subscribed chunk. Acknowledgements may be batched:

```text
chunk_revision_ack
    world_id
    entries[]: {chunk_x, chunk_z, highest_contiguous_revision}
```

Recovery behavior:

- Gap still in recent-delta ring: retransmit the missing range.
- Gap outside retained range: send a full chunk snapshot.
- Snapshot checksum mismatch: reject snapshot and request it again.
- Repeated recovery failure: disconnect with a protocol/state-desynchronization
  error rather than allowing divergent map state.

Transport reliability does not replace revision checks. Reconnects, queue
resets, application bugs, and snapshot races still require application-level
revision validation.

## Persistence format

The persisted chunk represents current state, not an event log. Persist:

- generation identity and metadata;
- current section palettes/indices;
- player-modified mask or sparse override table;
- current chunk revision;
- checksum over the complete payload.

No network acknowledgement state, client request ID, or recent-delta ring is
persisted in the chunk.

Serialization must be transactional: build the complete output in temporary
storage and commit it only after success. Deserialization must read into a
temporary chunk and replace the destination only after complete validation.

The modified marker must agree with the serialized current state. If the
current value equals the reconstructed baseline, loading may canonicalize the
entry by clearing the marker. Invalid counts, duplicate sparse positions,
unregistered block IDs, truncated masks, and arithmetic overflow are hard
errors.

## Removing the current dirty-edit journal

Remove only the journal/history mechanism. Preserve `_dirty`,
`clear_persistence_dirty()`, generation protection, biome metadata, and normal
chunk serialization.

### Production code removal

Remove from `game_voxel_chunk`:

- `_dirty_edits`;
- `_dirty_edit_count`;
- `_dirty_edit_capacity`;
- `grow_dirty_edits()`;
- `record_dirty_edit()`;
- `get_dirty_edit_count()`;
- `get_dirty_edit()`;
- `clear_dirty_edits()`;
- `acknowledge_dirty_edits()`.

Remove allocation, transfer, and cleanup of these members from constructor,
`initialize()`, `destroy()`, and `move()`.

Change `write_block()` and `game_voxel_region::write_block()` so they apply a
current block value without allocating or recording a journal entry. Preserve
the existing persistence-dirty and generation-protection behavior until the new
override marker is implemented.

Remove dirty-edit count and edit records from new chunk serialization. Bump the
chunk format version. Do not reinterpret old version-4 bytes as the new format.

### Compatibility with version-4 chunks

Choose and document one migration policy before merging:

- Preferred: the new reader accepts version 4, reads and validates its edit
  records into temporary values, discards the history, and keeps the fully
  serialized current chunk sections. It then saves as the new version.
- Acceptable only before public persistence exists: reject version 4 explicitly
  with a documented incompatible-version error and provide an offline migration
  tool.

The preferred migration does not need to retain `game_block_edit_op` as a
public chunk API. A private compatibility decoder can consume the fixed v4
record fields. All counts and multiplication must be bounded before advancing
the buffer.

`game_block_edit_op` may be retained temporarily only if another implemented
consumer uses it. Do not claim that it is a shared networking/undo contract
until those consumers exist. The new network request and authoritative delta
types should have explicit names and separate schemas because client intent and
server-accepted state are different concepts.

### Tests to remove or replace

Remove tests whose only contract is the dirty-edit list:

- recording/listing dirty edits;
- growing the dirty-edit allocation;
- acknowledging or clearing dirty edits;
- round-tripping dirty-edit history.

Replace them with tests for current-state persistence, provenance masks,
version-4 migration, chunk revision round trips, authoritative application, and
snapshot/delta synchronization.

Update the Game module manifest when a source/header becomes unused. Run the
AGENTS policy scan and verify that release archives contain no test-only hooks.

## Concurrency and lock ordering

Recommended lock order:

1. world/region lookup lock;
2. chunk state lock;
3. persistence queue lock;
4. network delta queue lock.

Do not hold a network queue lock while acquiring a world or chunk lock. Prefer
preallocating the immutable delta and required queue node, committing the chunk
under its lock, then publishing the prepared node after releasing the chunk
lock.

Chunk revision, current block value, modified marker, and persistence-dirty
state form one transaction. Readers must not observe a new block with an old
revision or the reverse.

## Security requirements

- Never trust client-provided world coordinates, block IDs, revisions, ticks,
  or identities.
- Use the authenticated connection identity, not an identity inside the request,
  as the acting player.
- Apply per-player and per-chunk rate limits.
- Bound message counts and lengths before allocation.
- Reject arithmetic overflow while converting coordinates or calculating
  section indices.
- Authenticate and encrypt requests, responses, snapshots, and deltas through
  the Networking secure channel.
- Bind protocol version, message type, connection/session ID, and packet number
  into authenticated associated data.
- Preserve replay protection across all block-change messages.
- Log rejected edits without allowing untrusted strings or unlimited log volume.

## Error handling

Define explicit protocol/application errors for:

- stale chunk revision;
- current-block mismatch;
- invalid block ID;
- out-of-bounds coordinate;
- permission denied;
- chunk not ready;
- request rate limited;
- duplicate request with mismatched payload;
- delta gap;
- unsupported snapshot or generator version;
- snapshot checksum failure;
- allocation or queue capacity failure.

Errors sent to clients must not expose server filesystem paths, internal pointer
values, cryptographic material, or private diagnostics.

## Required test matrix

### Unit tests

- First player edit creates one override marker.
- Repeated edits replace the current override without retaining history.
- Restoring the generated value removes the override.
- Generated writes never create player provenance.
- Invalid coordinates and block IDs leave state unchanged.
- Allocation failure leaves block, marker, revision, and queues unchanged.
- Chunk revision increments exactly once per accepted edit.
- Rejected and duplicate requests do not increment revision.
- Serialization preserves current state, provenance, generation identity, and
  revision.
- Truncation at every byte boundary leaves the destination unchanged.
- Version-4 migration consumes old dirty-edit records safely and preserves the
  current blocks already stored in the snapshot.

### Protocol tests

- Client request acceptance and rejection.
- Idempotent duplicate request handling.
- Duplicate, reordered, missing, and corrupted deltas.
- Delta recovery from the retained ring.
- Snapshot fallback after ring eviction.
- Snapshot-at-revision handoff while edits continue concurrently.
- Originating client reconciliation with and without prediction.
- Interest entry/exit and no broadcast outside the interest set.
- Reconnect with stale client chunk revisions.
- Server restart followed by snapshot and new deltas.
- Reliable lane congestion while unrelated unreliable traffic continues.

### Multi-client integration tests

Use at least three clients and one authoritative server:

- Client A edits; server commits; A, B, and C converge.
- A and B request conflicting edits at the same revision; exactly one ordering
  chosen by the server becomes canonical and all clients converge.
- One client disconnects before a delta and recovers after reconnect.
- One client deliberately drops or reorders application messages through the
  deterministic impairment simulator.
- A client joins while a chunk is changing and receives a consistent snapshot
  plus post-snapshot deltas.
- A malicious client sends invalid IDs, coordinates, revisions, duplicate IDs,
  and oversized batches without changing server state.

### Persistence and regeneration tests

- Save/unload/load preserves current player overrides.
- Regeneration with the same generator identity reconstructs the same baseline.
- Regeneration preserves player overrides by default.
- Explicit administrative reset removes overrides and exposes generated values.
- Generator-version mismatch follows the documented migration/error path.
- Corrupted provenance data cannot allocate unbounded memory.

### Concurrency and sanitizer tests

- Concurrent edits to different chunks.
- Conflicting edits to the same block.
- Snapshot capture concurrent with accepted edits.
- Client unsubscribe/disconnect concurrent with broadcast.
- Queue allocation failure and shutdown during publication.
- Run the complete suite under ASan, UBSan, and TSan on supported platforms.

## Implementation sequence

1. Freeze and document the current version-4 chunk layout.
2. Add compatibility fixtures containing version-4 chunks with zero, one, and
   many dirty-edit records.
3. Remove the dirty-edit journal API and allocation from `game_voxel_chunk`.
4. Introduce the new chunk format version without edit history.
5. Implement and test the version-4 migration reader.
6. Add chunk revisions and current-state provenance storage.
7. Add transactional authoritative server mutation.
8. Add request, response, authoritative delta, acknowledgement, and snapshot
   schemas.
9. Implement per-client interest sets and snapshot handoff.
10. Implement bounded recent-delta recovery and snapshot fallback.
11. Add multi-client, failure-injection, impairment, persistence, and sanitizer
    coverage.
12. Update Game and Networking documentation and only then remove obsolete
    compatibility code after the supported migration window.

## Acceptance criteria

The work is complete only when:

- no dirty-edit journal members or APIs remain in `game_voxel_chunk`;
- ordinary persistence dirty tracking remains functional;
- current player-modified state survives save/load and regeneration;
- clients cannot directly commit map state;
- all subscribed clients converge through revisions, deltas, and snapshots;
- duplicate or missing messages cannot duplicate or silently lose a block
  change;
- old version-4 data follows the documented migration policy;
- failure paths are transactional;
- all unit, multi-client, persistence, impairment, and sanitizer tests pass;
- release builds contain no tester-only diagnostics or failure-injection code.
