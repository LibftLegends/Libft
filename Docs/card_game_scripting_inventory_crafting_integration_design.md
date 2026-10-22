# CardGame, Scripting, Inventory and Recipe-Book Integration

Status: implementation in progress. The design remains the contract; the
implementation ledger below records only verified slices and must be kept
current until every required Libft and Minecraft item is implemented.
Original design date: 2026-10-02. Status checkpoint: 2026-10-22.

Reviewed branches and fetched revisions:

- Libft: `agent/compression-analytics-cardgame-scripting`, `280328f153193e7a544a7d6f00176ed9721b1ec6`.
- Minecraft: `agent/analytics-performance`, `c422dcc43bd758394637555d300e67cd17380f9b` (pins the Libft commit above).

This checkpoint was refreshed after pulling both branches. The older design
sections below remain the target contract; implementation-status claims must
be read together with the ledger and dated updates, not inferred from proposed
API names or from source presence alone. In particular, source-only changes
are not considered validated, and the remaining-work list is not a claim that
the full backend is complete.

### Backend status at the fetched commits (source inspection, 2026-10-22)

| Area | Present in current source | Still required before calling it complete |
|---|---|---|
| Libft CardGame and scripting | Typed host intents; session-scoped, request-idempotent commands; draw/reshuffle/burn and mandatory-discard mechanics; snapshot/replay support; script bridge and manifest/context primitives. | Finish and validate the action/host transaction boundary, trusted manifest provisioning, bounded durable result history, and cross-platform persistence/replay. A committed Libft action must not be described as an atomic Minecraft world mutation until the shared journal/recovery contract proves it. |
| Minecraft card adapter | `RegenerationCardSession` is wired into `GameSession`; source contains the starter-deck multiset, typed choices, draw/discard paths, several starter and craft-only effect mappings, and regeneration rule/feature planning. | Prove exhaustive enabled-card coverage, target/no-effect behavior, effect composition and session reset through validators; complete any missing content rules and failure recovery. Source presence is not end-to-end card validation. |
| World Loom backend API | Revisioned selection and quote APIs plus `GameSession::commit_regeneration_start`/finalization source for actor/Loom/request-scoped quote validation, debit, opening deal, chunk reservation/barriers, and the decision deadline. | Production identity/access and occupancy providers, occupied-neighbor exclusion refresh, multiplayer reservation/deadline lifecycle, durable idempotency and host-intent recovery, and crash-safe chunk publication. No UI is part of this design. |
| Inventory, recipe book, crafting | Libft has revisioned snapshots, transactional add/transfer and prepared craft APIs, recipe knowledge persistence/query; Minecraft source has item/catalogue, discovery, hand/station craft, and session persistence/recovery paths. | Complete registry-wide item/drop/recipe coverage, physical-instance authority across all stores, real station ownership/reach and multiplayer checks, full save/recovery failure tests, and gameplay/content validators. |
| Validation state | The current Libft worktree test executable passes 6,507/6,507 tests when run from `Libft/Test` outside the sandbox. | Minecraft normal and analytics builds and `--validate-item-catalog` pass. The latest `make validate-all` attempt failed at block-edit emissive-delete convergence (dirty target chunk with remesh in flight) and did not complete; do not treat this as a full Minecraft validation pass. |

Treat the “Present” column as source evidence only. The acceptance criteria in
Sections 13–15 remain authoritative; update this matrix and the Minecraft
roadmap after each feature is tested on the actual branch revisions.

### Post-fetch worktree update (2026-10-22)

The following changes are uncommitted worktree changes on the branches named
above; they are not included in the recorded commit hashes. Libft now exposes
`game_recipe_unlock_result`, separating `event_newly_recorded` from
`recipe_newly_known`. Minecraft's recipe catalogue carries that result through
discovery, and `RecipeDiscoveryTransaction` checkpoints whenever a new source
event receipt is recorded—even if the recipe was already known. An exact
receipt retry does not write another checkpoint. If persistence fails, the
recipe-book snapshot is restored and both result flags are cleared. The legacy
boolean APIs continue to report only whether the recipe became newly known.

Regression coverage checks a second, distinct event for an already-known
recipe, exact retry without another save, and save-failure rollback followed by
a successful retry. The Libft unit test covers distinct events, duplicate
retry, conflicting reuse, and persistence round-trip. Minecraft's
`ItemCatalogValidator` source covers checkpoint count and rollback behavior,
but that validator has not executed on this worktree. The current Libft test
executable passes 6,507/6,507 tests on Linux when run from `Libft/Test` outside
the sandbox. The earlier Minecraft root `make test` failure remains historical;
the current normal and analytics executables both pass
`--validate-item-catalog`, while full `make validate-all` and the complete
Minecraft integration remain unverified.

### Implementation ledger (started 2026-10-06; refreshed 2026-10-22)

These entries describe work on the checked-out branches and the evidence
available for each slice. They do not imply that the full design or Minecraft
integration is complete. Later dated updates below refine earlier ledger
entries; where evidence is only source inspection, compilation/runtime status
must remain explicitly open.

| Slice | Current status | Evidence / remaining contract |
|---|---|---|
| Typed host-intent schemas and transactional callback output | Implemented in Libft | Focused host-intent tests cover schema validation, ordered acknowledgement, and rollback when a callback fails. |
| Pending host-intent queue in snapshots/deltas, byte serialization and state hash | Implemented in Libft; in-memory state format v2 and `GCGS` snapshot v1 | `card_game_engine::serialize_snapshot` writes each sequenced pending intent and `restore_snapshot` validates and restores them. `test_card_game_host_intents_survive_snapshot_delta_ack_and_revert` covers snapshot/delta transfer; `test_card_game_host_intents_survive_byte_snapshot_restore` now checks byte round-trip, pending record identity, state-hash equality, checksum rejection and no mutation on failed restore. The byte snapshot test passes a focused direct MinGW runtime harness. The serialized replay test now compares every pending intent sequence, schema field type, and field value after replay deserialization, and passes a focused direct MinGW runtime harness. The current full Libft suite passes 6,505/6,505 on Linux; this does not validate the separate Minecraft integration. |
| Script effect adapter | Context reads, typed intent output, match-owned bounded RNG and RCS1 v5 persistence implemented; script catalogue loading and trusted manifest comparison implemented; host provisioning and policy wiring remain open | `emit_host_intent` is type/range checked and uses the event transaction. `card_actor_id()`, `card_instance_id()`, and `session_phase()` expose invocation-scoped actor, physical source card and current phase; calls outside execution fail. `random_bounded(bound)` is effect-scoped and advances the snapshot/hash-backed match RNG with unbiased rejection sampling; event rollback restores its state on later failure. `choice_selected_id(choice_id)` reads a resolved selected option only when the choice belongs to the active player. `scripting_engine::get_native_manifest_hash` hashes canonical ordered native IDs/names; the bridge `get_manifest_hash` also hashes event bindings and serialized script programs, excluding pointers. Focused bridge tests pass 9/9. The bridge now provides a bounded registered-key/value policy-context primitive whose schema is included in its manifest; bridge-level context serialization is implemented, and the RCS1 v5 adapter snapshot persists its registered key/value state, starting-deal request and card response metadata; catalogue checksum/verifier and trusted hash comparison are implemented, while authenticated expected-hash provisioning, generation-policy wiring and persistence compatibility remain open. The bridge unregisters callbacks when destroyed; callers must destroy it before its engine. |
| Hand-instance discard and policy-aware draw | Libft primitives and versioned session persistence implemented; remaining action contract open | `CARD_GAME_INTENT_DISCARD_HAND` records physical instance and destination. `configure_draw_policy` validates owner-scoped discard/burn zones and registered two-U32 host-event schemas. Draw actions accept a bounded count up to 100, process each attempt independently, reshuffle discard through persisted engine RNG, burn each physical instance drawn while the hand is full, and queue typed empty/overflow host intents. Command-log v5 preserves draw count and accepts v1–v4. Mandatory discard obligations are represented in snapshots, deltas, hashes, command records and replay; while pending, only the obligated actor can discard a real hand instance to the configured discard zone. Empty-hand resolution creates no impossible obligation. Command-log v6 reads v1–v5. Required discard configuration is independent of whether the deck may reshuffle. The runtime RegenerationCard validator now confirms persisted Double Draft play/draw/discard retries and physical-card transfer identity. The engine now resolves expired choices to validated deterministic defaults transactionally; authoritative clock/session recovery and atomic inventory/host journal coordination remain open. |
| Request-idempotent action submission | First Libft slice implemented; complete action contract remains open | `card_game_action` and `submit_action` retain accepted request IDs in version-4 command records, return the committed state hash for exact retries, reject a reused ID with a different command, and preserve IDs through command-log serialize/deserialize and deterministic command replay. Focused test `test_card_game_submit_action_is_idempotent_and_serialized` passes. Engine and adapter state plus their bounded replay caches now persist in GameSession v7. Choice selection and expiry are sequenced host commands with stable request IDs and authoritative epochs; command-log/replay versions preserve them, and retries reuse the original command epoch. World Loom finalization and the scheduler-facing GameSession API now route expiry through this command path and checkpoint the result. Session IDs, bounded action-result history, authenticated epoch provisioning, and host transaction coordination remain open. |
| Inventory add, transfer, and snapshot APIs | Libft transaction APIs implemented; value-only snapshots and physical item IDs added | Single-item `add_item` stages deep-copied stacks and publishes only on success. `transfer_to` verifies both expected inventory revisions, stages source/destination contents, enforces slot/weight limits and preserves item metadata before publishing; successful removals and transfers advance revisions. `game_item` now separates its 64-bit physical instance ID from its definition ID, preserves identity in copies/transfers and inventory saves, and requires identified instances to be single-item stacks. Inventory add/transfer reject a duplicate ID already in the destination inventory. The complete inventory snapshot includes container metadata, all serialized item fields, instance IDs and the revision captured under inventory/map locks; world save uses this value snapshot rather than iterating raw item pointers. The world persistence roundtrip passes with a physical ID above `INT64_MAX`. Older saves without the optional instance-ID field load with ID 0. GameSession now raises the persisted output-ID allocator above physical IDs found in character inventory, ground items, item entities, and physical card items before startup transaction recovery, preventing craft recovery from reusing an ID that only exists in an entity or card store. This is an implementation-only source change; compile/runtime verification remains deferred. Complete cross-session/global uniqueness and item-definition policy for stackable versus unique items remain host/catalog responsibilities. Production builds no longer expose mutable `get_items()`; it remains available only to test fixtures. `clear_items()` clears slot contents and accounting under the inventory lock while advancing the revision, and persistence/reset callers use it. Add and transfer paths stage item values, so caller-owned handles do not mutate the inventory after insertion. Inventory revision now round-trips in character snapshots, with legacy snapshots defaulting to a load-generated revision. The shared journal recovers single-player world-item pickup, hand crafting, timed-processing start, and physical CardGame item transfers; other inventory operations, CardGame host-intent delivery, and World Loom start still lack coordinated transactions. |
| Recipe book persistence/default grants | Foundational Libft API implemented; unknown IDs quarantined | `game_recipe_book` registers stable numeric IDs, seeds caller-configured default unlocks, records idempotent grants by nonzero source-event ID, and imports/exports sparse version-1 state transactionally. `unlock_recipe_with_result` distinguishes a newly recorded source event from a recipe newly becoming known; legacy boolean APIs retain their previous “newly known” meaning. Unknown imported recipe IDs remain unavailable in the current catalogue, are counted as quarantined unlock events, and round-trip so a later catalogue can recognize them. |
| Recipe unlock, read-only query and inventory crafting transaction | Libft foundation implemented; richer recipe contract remains | `game_crafting` supports explicit locked registration, recipe-owned output definitions, batch `prepare_craft`/`commit_craft`, stale-inventory rejection, and non-mutating `query_recipe` with cumulative rarity-aware ingredient deficits. Prepared crafts capture and compare the exact inventory revision, reject revision overflow before publication, and advance inventory revision once on successful commit. Recipe output definitions must have instance ID 0; the host can pass a minted primary output ID at commit for a unique one-item result, and duplicate IDs in the source/result inventory are rejected. Recipes accept up to 16 value-copied byproduct definitions; preparation places all byproducts in the staged inventory, and capacity failure leaves ingredients and outputs untouched. The prepared-craft suite passes 11/11; broader `test_game_crafting_`, `test_prepared_craft_`, and `test_game_inventory_` filters pass 135/135. Existing focused tests cover locked/missing recipes, duplicate inputs, output-full rollback, batch overflow, recipe-owned outputs, stale prepared state, query deficits and query non-mutation. Recipe station constraints are enforced by station-aware query and prepare APIs; explicit alternative item IDs are supported in ingredient groups. A catalog-wide read-only query combines recipe-book knowledge, station availability and per-recipe ingredient deficits. Hand and timed-processing actions recheck authoritative recipe-book knowledge before accepting a craft. Timed processing is integrated: each advance is journal-prepared and checkpointed before mutation, saves committed processing/inventory/journal state together, and restores all three in memory if commit/save fails. The GameSession tick path first recovers pending advances and invokes the journalled advance transaction. Named Minecraft item tags drive the wood, crystal, ore-sample and flower-pigment ingredient groups; completing the full item/recipe catalog remains open. These transaction-path findings are source inspection only; no build or runtime verification was performed during this pass. |
| Minecraft revision selection, opening-deal quote and generation queue admission | Selection, quotes, queue admission, in-process commit path, and v10 atomic session persistence of revision metadata implemented; durable multiplayer lifecycle remains open | `update_revision_chunk_selection` provides compare-and-set selection, a configured cap up to the hard 256-chunk limit, loaded/unprotected eligibility and a 64-bit selection revision. The 256 limit accommodates the paid 9-by-9 Volcano Mark footprint. Quotes bind selection and policy revisions. `GameSession::commit_regeneration_start` prepares a WLS1 journal record, checkpoints, removes quoted currency, deals the quoted 4–7 opening cards, marks the record committed and gates card actions to the pending decision window; finalization applies card intents and starts generation. The server binds drafts to authenticated actor/Loom IDs, requires both client and station-access authorizers before initialization, rechecks access on quotes/actions/retries, validates the exact configured station block ID, and cancels an owned uncommitted draft on shutdown or station removal. Metadata v4 persists the exact signed station coordinates and expected block ID while retaining v1–v3 readers; session snapshot v10 embeds that metadata in the same atomically replaced `.wisp` file, with `.wrv` load fallback for older snapshots. The direct-linked `--validate-world-items` now exercises v10 and v11 session metadata/fluid round trips and corruption handling. Recovery aborts a prepared start against the pre-mutation snapshot; committed selection, barriers, and deadline restore from the session v10 lifecycle metadata. Startup recovery now aborts a prepared start against the pre-debit/pre-deal checkpoint, cancels orphaned uncommitted drafts, and clears their saved lifecycle restore markers; restoring the owning actor and request cache across restart, and crash-durable in-flight generation publication, remain open. Multiplayer occupancy feeds, persistent chunk reservations/barriers, production authentication/reach/claim provider and Loom block placement, crash-durable generation publication, and a production World Loom map UI remain open. |
| Minecraft item catalogue, ground items, action/session integration, regeneration, instability and recipe contents | Item catalogue, built-in mining drops, session-persisted ground-item store and character inventory, exact starter CardGame multiset, typed choices, and Double Draft draw/discard flow integrated; broader design remains open | Minecraft registers built-in block acquisition rules, block item definitions, magical dust and bucket item definitions, and configures a 30-slot character inventory. The current direct-linked `--validate-item-catalog` reports 76 built-in policies and 109 content policies; the latest --validate-world-items run passes the session, crafting, recovery and MCSP v16 persistence checks. A synthetic runtime block test checks missing-rule rejection and successful self-item coverage, then unregisters the block. This found a Libft `ft_map` growth bug that doubled capacity for every inserted key and prevented later item definitions from being stored; copy and move insertion now grow only when full. Content items have registered acquisition policies; runtime-added block rules remain caller-registered, so global Voxel-registry exhaustiveness still requires validation. `WorldItemStore` indexes dropped stacks by chunk, provides bounded nearby queries, enforces owner pickup locks and an 8-block reach ceiling, and transfers quantities through Libft's revision-checked inventory transaction with request-ID deduplication. The single-player tick picks up nearby items within 1.5 blocks. The break interaction reserves a hidden pending drop before editing, publishes it after success or after a failed edit is confirmed to have removed the block, and removes it when the block remains. Successful mining then tries the bounded nearby merge selector. Pending items are invisible to pickup/query and block snapshot save. This closes the live pickup race; durable atomic block/drop recovery remains open. `RegenerationCardSession` seeds 4 Biome Exclusion, 4 Deep Seam, 2 Village Charter, 2 Surface Magma, 2 Double Draft, 4 Biome Compass, and 2 Surveyor's Reach instances in Libft's native deck/hand. Version-2 play intents carry typed biome-ID, ore-ID, or target-chunk choices as required. Double Draft resolves only from its acknowledged card intent, records two draws plus a one-card discard obligation through Libft's sequenced command log, blocks other card plays until the obligation is met, and routes overflow/empty draws through typed host intents; play/draw/discard request retries are cached within the session. The validator covers wrong-choice rejection, exact retries, pending-intent acknowledgement, physical card movement and the Double Draft draw/discard flow. Biome and ore choices are validated against the active generation config, and the current consumer applies all seven starter effects plus Ore Overgrowth, Ultimate Greed, Biome Rupture, Fortune's Draw, and Volcano Mark. All six craft-only rare cards now have adapter mappings and generation feature plans. The chunk worker has deterministic placement routines for ruined villages, shipwrecks, desert pyramids, ancient canopies, and spider nests; the revision regenerator assigns bounded spawners and loot drops for those plans. This is implementation evidence only: full structural acceptance requirements and end-to-end runtime behavior remain unverified. Volatile Stabilizer has a persisted use count, private deterministic roll, retry behavior, and instability adjustment; an overload modifier now forces at least one deterministic spawner/reward opportunity in an eligible unreserved chunk during regeneration; terrain-site preflight now requires more suitable chunks than the upper bound reserved by pending structure plans before card consumption; exact assignment and runtime validation remain open. Authoritative entities, scheduled hostile spawners, deterministic loot tables, and checkpointed death-to-drop processing now exist, but mob AI remains open. CardGame engine/adapter state and play/draw/discard/import/export replay caches now persist through GameSession v8; RCS1 v6 also persists logged-choice-expiry results so exact retries survive restore. Runtime coverage confirms host expiry retries before/after restore leave the saved session state unchanged. Host-intent transaction coordination and durable World Loom recovery remain open. The adapter source is included in the Minecraft build. Ground stack merging now has a revision-checked selector and post-commit mining integration; item movement, rendering/replication, additional recipe/content balance, and durable World Loom recovery remain open. Continue with the ownership and test split in Sections 5–10; update the Minecraft roadmap as each slice is integrated and validated. |

Action-session scoping refresh (2026-10-22): the latest Libft branch adds
session IDs to version-3 `card_game_action` records and scopes request
idempotency to `(session_id, request_id)`. `RegenerationCardSession` accepts a
nonzero action-session ID, and GameSession supplies its persisted regeneration
output session ID before initializing the adapter. This supersedes the older
ledger wording that session IDs were wholly unimplemented. It does not provide
actor authentication: trusted server identity/authorization, durable bounded
result-history policy across every host action, and atomic host-intent delivery
remain separate requirements. This checkpoint did not compile or execute the
latest pulled branches, so the source wiring is not marked runtime-validated.

Card-catalogue count clarification (2026-10-22): the ledger's statement that
six craft-only rare cards had adapter mappings describes the earlier slice in
which those six definitions existed. The current source has seven starter
definitions plus twelve non-starter, discovery-locked card definitions:
definition IDs 11–22, including Fortune's Draw (ID 14). The earlier counts of
six or eleven rare definitions are historical; they are not the current
catalogue limit or proof that every current card passed end-to-end validation.

Choice-validation update (2026-10-06): the older item-catalogue row above
predates active-world validation. `RegenerationCardSession::play_card` now
requires the active `voxel_generation_config`; biome selections map to
one-based indices into that config, while ore selections resolve to enabled
ore block IDs. Invalid domain choices are rejected before card consumption,
and the regeneration validator checks this. The biome index mapping is
config-local because Voxel has no stable biome ID field yet; bind it to a
versioned generation/content manifest before persistence or cross-version
replay. Applying the resulting rules overlay to terrain generation remains open.

Session-rules update (2026-10-06): `RegenerationWorldRules` now folds typed
card intents into a bounded session-local overlay without mutating base Voxel
config. It applies biome exclusion, cumulative rational ore scaling, bounded
village and surface-magma plans, configured Compass weight adjustments only
to eligible biomes, and additional target scope. It enforces the final-biome
invariant, configured caps, ordered host-intent sequences and exact retry
deduplication. `consume_pending_card_intent` decodes only the CardGame queue's
actual pending card intent, applies it, and then acknowledges it, so a rejected
intent remains pending and an acknowledgement retry does not duplicate its
rule effect. Focused regeneration validation covers composition, final
biome rejection, rational caps, feature/scope caps, Compass exclusions and
intent conflicts. `apply_ore_scales` now materializes checked ore rates into a
caller-owned staged Voxel config and leaves the base config untouched. A new
`World::begin_world_revision` overload freezes the rules into the manager-owned
config before asynchronous regeneration: it materializes ore scaling and
wraps the original biome selector with manager-owned eligibility state. Allowed
baseline selections are preserved; an excluded selection is deterministically
remapped among eligible biomes using a hash of the seed and biome-zone
coordinates. The rules-aware overload requires full generation stages for each
selected chunk. The World Revision validator checks that exclusion never
returns the excluded biome across zone samples and that Deep Seam changes
committed ore frequencies. When Compass adjustments are present, active biome
weights are `max(1, 1 + accumulated_adjustment)` and a stable hash of the seed
and zone coordinates samples their normalized distribution. The original
selector remains in use when no weight adjustment is active, and excluded
biomes are omitted from the weighted set even if their configured adjustment
is positive. The validator checks deterministic repeated queries and a
minimum sample share for a positively weighted biome. World Loom server
actions still do not carry the card session/rules object; village/magma
planners, target scope, and durable overlay/request history also remain open.
Voxel stage dependency validation now runs after a requested base-terrain
clear, allowing a full rebuild to discard stale partial-stage metadata first.

Session-overlay persistence slice (2026-10-06): `RegenerationWorldRules` now
serializes its ordered accepted-intent journal to a bounded version-2
little-endian payload with an FNV-64 checksum and server-supplied
generation/content manifest hash. Restore rejects a mismatched manifest hash,
stages a fresh overlay, revalidates/replays every typed intent against the
caller-supplied base config and policy, and replaces the destination only
after full success. File save/load uses Libft atomic replacement and a bounded
read. The regeneration validator checks memory and file roundtrips and
confirms manifest mismatch and checksum failure leave restored state
unchanged. Atomic visibility is not crash durability; canonical manifest
production, persisting the base context, and atomic coordination with CardGame
session/request history remain open.

Validation recorded on 2026-10-02: after the action-envelope slice,
`make -j4 test-executable` and `make -j4 normal` succeeded. Focused
draw/replay/query/prepared-craft tests passed 22/22; focused action/replay
compatibility tests passed 3/3. The repository-supported `make -j4 run-tests`
completed 6,435/6,445. Its nine logged assertion sites are in file-path asset
validation, voxel block-registry setup, and terrain runtime-block/script asset
lookup. The focused file-path case passes from `Test/`, so these failures
appear dependent on full-suite state or asset resolution, but remain
unresolved. The broad suite is not green; diagnose and rerun after later
integration slices. Minecraft changed objects
`WorldRevisionManager.o`, `WorldRevisionFacade.o`, and
`WorldRevisionValidator.o` compile, but the full `ft_vox` target is currently
blocked by existing `-Werror` diagnostics in `src/validators/LightingTestHarness.cpp`
(format-type mismatches and useless casts). A first full build also reported
four useless casts in `BlockEditValidator.cpp`; those local diagnostic-only
edits were discarded because they do not resolve the remaining build blocker.
The new WorldRevision validator has not yet been executed in the application.
The current worktree also builds `Test/libft_tests`; targeted native/script
manifest tests pass 5/5, and the full CardGame filter passes 138/138. The full
repository test suite has not yet been rerun after these changes.

Latest validation on 2026-10-06: `make -j4 normal` completed successfully on
the Windows MinGW toolchain, compiling the current 162-source Minecraft
configuration and linking both `ft_vox.exe` and `ft_vox_analytics.exe`. This
includes the newly registered `src/cards/RegenerationCardSession.cpp` and the
current World Revision and item-store changes. The build initially exposed a
Windows `-Werror=shadow` failure because a CardGame script-bridge parameter was
named `byte`, colliding with the Windows SDK typedef; renaming it to
`byte_value` resolved the compile failure. No runtime validator or Libft test
suite was run during this continuation. The starter-deck adapter still needs
a dedicated runtime validator for the seven definition counts and exact
20-instance multiset. That validator is now wired to `--validate-all` and
`--validate-regeneration-cards`; the direct run passed for every opening size
from four through seven, checking unique physical instance IDs across draw
pile and hand. The direct card validator also confirms typed choice-shape
validation, choice values in the queued host-intent record, exact play-request
retry, mismatched-payload rejection, and host-intent acknowledgement;
world-specific biome/ore validation and effect application remain open. The
adapter now retains accepted play request records without eviction up to its
4,096-action session cap and rejects additional new plays before card
consumption, preserving retry identity for the session lifetime.

Validation update (2026-10-10): `choice_selected_id(choice_id)` is now a registered, manifest-covered scripting native. It returns a resolved option only for a choice owned by the active effect actor. The focused bridge suite passes 9/9, including success for an owned resolved choice and permission-denied rollback for another player’s choice. The focused runner linked the current bridge test object against the Libft archive; the repository-wide test link still selects stale Linux platform archives under Git Bash and is not counted as passed.

Validation update (2026-10-10): selected-chunk regeneration now stages its complete worker request set and submits it atomically to the generation queue. `make -j1 normal` succeeded with Windows platform selectors, linking both normal and analytics executables. `--validate-world-revision` passed with `world-revision: ok regenerated=1 skipped=0`; it covers successful batch submission and rejects a batch larger than the configured queue capacity without enqueueing a partial set. This establishes queue admission atomicity only; it does not yet debit resources, reserve world chunks, install barriers, start/deal the CardGame session, or persist a cross-system commit record.

### Current branch checkpoint (2026-10-22)

This report applies to the fetched tips named above and is source-inspection
status, not a claim that the tips have been rebuilt or end-to-end validated.
It supersedes the original October 2 push-batch summary; dated validation
entries remain historical evidence for the exact source/toolchain they name.

- Libft now contains the generic CardGame action/session machinery, typed host
  intents, snapshot/delta/replay state, scripting bridge and manifest/context
  support, policy-aware draw/burn/reshuffle and mandatory discard, plus
  revisioned inventory, recipe-book, and prepared-crafting foundations.
- Minecraft source now contains `RegenerationCardSession` integrated with
  `GameSession`, typed choices/effects and regeneration rule planning, a
  revisioned World Loom selection/quote path, a session start/finalization path
  for actor/Loom/request-scoped quote validation, resource debit, opening deal,
  chunk reservation/barriers and a persisted decision deadline, and item,
  recipe discovery, hand-craft and station-craft APIs.
- Remaining backend work includes production identity/access and occupancy
  providers, multiplayer reservation/deadline enforcement, durable host-intent
  and request recovery across all action paths, atomic chunk publication,
  exhaustive enabled-card/item/recipe coverage, and failure-injection plus
  cross-platform end-to-end validation. The detailed acceptance criteria and
  owner split remain in Sections 13–15 and the status matrix above.
- No UI is in scope. Keep this design and the Minecraft roadmap active; move
  this file to `Libft/Docs/old` only after the full contract is implemented and
  verified.

Validation update (2026-10-06): the current full Libft test suite passes
6,447/6,447, including all 144 CardGame tests. The suite exposed Windows stack
exhaustion in two draw-policy snapshot/replay tests; their large engine,
snapshot and delta fixtures now use heap storage, and the isolated replay case
passes. It also exposed that `configure_draw_policy` incorrectly required
reshuffling to configure a discard destination. Validation now always checks
the owner-scoped discard zone while `reshuffle_discard` independently controls
whether it is used as a draw source. The bridge test confirms the three
effect-context script natives reach typed host-intent fields. The Minecraft
validators `--validate-regeneration-cards` and `--validate-world-revision`
also pass on the current application executable. World Revision quote coverage
now updates the world-owned policy revision, confirms the quote arithmetic,
and rejects both stale policy updates and stale quote revisions. Revision
metadata format v2 atomically stores the policy and checksum; the validator
round-trips it, proves a corrupted checksum leaves current state intact, and
loads a v1 record without erasing the restored v2 policy.

## 1. Authority and deliverables

The gameplay source is the consumer's [Minecraft 1.0 roadmap](../../docs/minecraft_1_0_roadmap.md), especially the regeneration lifecycle, deck and card catalogue, instability, and inventory/crafting sections. Existing library designs remain useful background: [CardGame simulation](old/card_game_full_simulation_engine_design.md) and [compression/analytics/card engine](old/compression_analytics_card_engine_design.md).

This document defines how to implement those rules, not a second card engine. All new names below are **proposed APIs**, unless explicitly listed as existing. Balance quantities and proposed item names are placeholders. Do not mark roadmap work complete because an API design exists.

Implementation must update this document, the roadmap's corresponding task/status entries, relevant module READMEs, and `Docs/module_dependency_graph.md` when dependencies change. Record actual tests and revisions. Keep design, implemented, integrated, and validated statuses separate. Use roadmap heading names rather than fragile subsection numbers.

### Ownership

| Libft | Minecraft |
|---|---|
| Card identities, zones, command validation, choices, draw/discard policy, effect transactions, script adapter, replay and snapshots | Named cards, biome/ore IDs, terrain intents and legality, world rules, structures, instability balance and UI |
| Item identities, stacks, inventory transactions, recipe definitions, unlock book, crafting and timed processing | Actual item/recipe/drop catalogue, equipment stats, stations in the world, reach/permissions, loot and unlock triggers |
| Generic bounded typed effect outputs and persistence codecs | Chunk reservations, glowing barriers, five-minute session deadline, workers, chunk publication, saves/network transport |

CardGame must not include Minecraft `World`, renderer, voxel chunk objects, inventory UI, or terrain names. Game may depend on CardGame for an optional item-card bridge; neither CardGame nor Scripting depends on Minecraft. Prefer an optional `card_game_script_bridge` integration unit with explicit CardGame/Scripting dependencies; do not force Scripting into consumers that only use native effects.

## 2. Inspected capabilities and gaps

Existing sources: `Modules/CardGame/card_game.hpp`, `card_game_choices.hpp`, `card_game_zone_store.cpp`, `card_game_commands.cpp`, `card_game_snapshot.cpp`; `Modules/Scripting/scripting.hpp`; `Modules/Game/game_inventory.hpp`, `game_crafting.hpp`, `game_crafting.cpp`, `game_data_catalog.hpp`.

| Existing API | Reuse | Gap to implement |
|---|---|---|
| `register_effect`, `register_effect_callback`, `register_card_with_type`, `register_card_type` | Register stable, data-driven definitions and native callbacks | Generic non-combat resolution and typed host effect output |
| `submit_command`, command records and `replay_command_records` | Sequencing and replay | Current command enum only covers play, end turn, advance phase; add draw, discard, choices and transfers without pretending they exist |
| `zone_move_instance`, zone inspect/push/pop/shuffle APIs | One physical instance in one zone | Policy-enforced commands; zone mutation alone must not bypass draw or discard obligations |
| `draw_to_hand` | Baseline drawing | Existing full-hand failure is not burn-and-instability gameplay |
| `play_card_from_hand` | Existing compatible card games | Terrain cards must not require a combat-board permanent; new resolution-to-zone path |
| `open_choice`, `add_choice_option`, `choose_option` | Target and option identity | Mandatory multi-card discard, timeout handling through engine, snapshot/replay integration |
| `get_snapshot`, `apply_snapshot`, deltas, RNG state | Restore and deterministic replay | Include new choices, policies, obligations, outputs, idempotency and script/content manifests |
| `card_game_format` | Legality and copy caps | Enforce four copies over the entire owned deck, not only current hand or board |
| `scripting_engine::register_native`, `compile`, `verify_program`, `execute_program` | Bounded custom language; no external interpreter | Transactional typed CardGame bindings and deterministic native manifest |
| `game_inventory`, `game_item`, `game_data_catalog` | Existing ownership/catalogue foundations | Revisioned, all-or-nothing multi-container operations and safe snapshots |
| `game_recipe_blueprint`, `game_crafting::register_recipe`, `craft_item` | Reuse recipe infrastructure | Book unlocks, tag/alternative ingredients, timed processing, and host station validation; Libft station IDs now gate query/prepare |

Historical-gap note: this table records the pre-implementation inspection that
motivated the design. Its “Gap to implement” cells are not a current task list;
use the implementation ledger and dated status updates below for current
completion evidence and remaining work.

The legacy `craft_item` path consumed ingredients before `inventory.add_item(result)` succeeded. The newer `prepare_craft`/`commit_craft` path stages recipe-owned outputs and inventory changes transactionally; callers should use that path for authoritative crafting. Remaining work is to finish and validate the full Minecraft content catalogue and cross-system transactions, not to treat the legacy helper as the production transaction API. Keep compatibility wrappers explicit and ensure client requests never supply authoritative output definitions.

Follow `AGENTS.md`: fixed-width integers, `ft_bool`, explicit fallible lifecycle, checked errors, output parameters for lifecycle objects, and documented external synchronization. Preserve existing public contracts and version new serialization/commands; do not silently reinterpret old wire values.

## 3. Public action API contract

Extend `card_game_engine` rather than placing a parallel engine in Game or Minecraft. Add `card_game_session_policy`, command payload/result PODs, `card_game_effect_output`, `card_game_pending_obligation`, and a typed host callback interface. PODs contain stable IDs/scalars; dynamic owned buffers use explicit lifecycle containers.

Proposed entry points, all fallible operations returning `int32_t`:

| Entry point | Required contract |
|---|---|
| `configure_session(policy, content_manifest)` | Validate deck/hand limits, draw failure and overflow policy, zone IDs, choice/command/output limits before starting |
| `submit_action(const card_game_action &action, card_game_action_result *result)` | Only authoritative mutation path for the new model; reuse/extend sequenced command machinery |
| `query_legal_actions(player_id, output)` | Read-only legality plus unavailable/no-effect reason; never consumes RNG |
| `query_pending_choices(player_id, output)` | Redacted choices with stable option IDs and deadline |
| `query_card_effect_status(player_id, instance_id, context, output)` | Can play, legal targets, no-effect state, reason and safety caps |
| `resolve_expired_choices(authoritative_epoch, output)` | Deterministic defaults, recorded as server actions; no client-provided clock authority |
| `drain_committed_outputs(output)` | Read committed host-intent batches identified by action/session/sequence; retained until acknowledged |
| `acknowledge_outputs(batch_id)` | Idempotent acknowledgement after durable consumer acceptance |
| `export_session_snapshot(output)` / `import_session_snapshot(input, manifests)` | Extend existing snapshots; transactional validation, no pointers or callbacks serialized |

The target action contract carries schema version, session ID, actor ID, request ID, command sequence, expected state sequence, action kind, card **instance** ID, choice ID and stable selected option IDs. The current `card_game_action` is narrower: it stores schema version, session ID, request ID and an embedded `card_game_command`; the actor is `command.player_id`, ordering is in the command, and a single option choice currently maps `command.card_id` to the choice ID and `command.target_instance` to the option ID. A vector of selected option IDs and first-class actor/choice fields are not present. Extend/version that payload only if a real multi-select effect requires it; do not claim the proposed fields already exist. Definition ID alone cannot identify the physical card to consume. Validate sizes before allocating. Request retry returns the stored result; mismatching payload under the same request ID is rejected.

### Concrete Libft call map (checked against the current public header)

The names in the previous table describe the desired stable gameplay contract;
they are not all literal methods in the current API. The current engine already
provides the lower-level calls below. The eventual Minecraft/UI-facing service
should compose these calls and enforce the complete contract rather than expose
unrestricted zone mutation directly.

| Gameplay operation | Existing Libft entry points | Boundary / remaining design work |
|---|---|---|
| Construct a ruleset and register content | `initialize`, `register_zone`, `register_card`, `register_card_with_type`, `register_card_type`, `register_host_intent_type`, `register_effect_callback`, `configure_draw_policy`, `start_match` | A single validated content/session builder is not present. It must validate the complete manifest, zone ownership, copy caps and draw policy before publishing a playable session. |
| Submit a player card action | `submit_action(const card_game_action &, card_game_action_result *, void *)` | This is the request-idempotent, session-scoped command path. Build the action with schema version 3, nonzero session/request IDs, expected sequence, `CARD_GAME_INTENT_PLAY_EFFECT`, the exact `source_instance_id`, and the configured destination zone. Do not call `play_card_from_hand` or `zone_move_instance` as a substitute for the authoritative host-effect path. |
| Submit trusted choice/expiry actions | `submit_host_action(..., authoritative_epoch)`; `resolve_expired_choices` is also available | Only the trusted server supplies time. The application must route timeout decisions through the sequenced action path and persist its result; do not trust a client epoch or call direct choice mutation from UI code. |
| Read legal commands and pending choices | `query_legal_actions`, `query_pending_choices`, `get_choice` | Pending-choice views are owner-redacted. A typed card-specific preview/status query (including legal targets and a clear no-effect reason) is not currently provided and remains a facade/API addition. |
| Inspect cards in gameplay zones | `get_zone_count`, `zone_inspect`, `hand_inspect`, `get_deck_count`, `get_hand_count` | These are read-only primitives. The host must filter by actor/session and must not expose hidden deck order, secret modifiers, timeout defaults, or private script state. |
| Apply draws and mandatory discards | `submit_action` with `CARD_GAME_INTENT_DRAW_CARD` / `CARD_GAME_INTENT_DISCARD_HAND`; `configure_draw_policy` | Use the action commands so request replay, draw events, burn handling and discard obligations are sequenced. `draw_to_hand`, `discard_hand_instance`, raw zone pushes/pops, and `mulligan_hand` are lower-level compatibility/mechanics calls, not the consumer's network action API. |
| Deliver domain effects to Minecraft | `get_pending_host_intent_count`, `peek_pending_host_intent` or `inspect_pending_host_intent`, `acknowledge_host_intent` | Sequence IDs make reads repeatable, but the current acknowledge call alone does not create an atomic durable transaction with Minecraft world state. The consumer needs a journal/recovery protocol before acknowledging an intent whose effects must survive crashes. |
| Save, restore and replay | `serialize_snapshot`, `restore_snapshot`, `get_state_hash`, `serialize_command_records`, `deserialize_command_records`, `replay_command_records` | Snapshot and command-log formats are versioned mechanics primitives. The host still owns atomic save replacement, trusted manifest provisioning, bounded durable request-result retention, and cross-system recovery. |

There is no current `configure_session`, `query_card_effect_status`,
`drain_committed_outputs`, or single-call `import_session_snapshot` API with
the complete semantics in the contract table. Implement these as explicit
facade/engine additions only where composition cannot safely live in the
consumer. Preserve the current low-level APIs for existing card-game users;
the new adapter must not silently change their semantics.

Action kinds: draw; play hand instance; play authorized external consumable; mandatory discard selection; choose target/mode; confirm phase; deck-to-inventory/inventory-to-deck transfer through a prepared host transaction. Do not allow arbitrary client draws, arbitrary zone moves, or advancing past mandatory decisions. Permission/phase rules determine which actions are available.

An effect callback receives immutable session/card/actor context, current working rules view, deterministic RNG access, and a bounded output builder. It emits generic operations such as `request_draw`, `require_discard`, and typed domain intents. Minecraft registers domain schemas such as biome-weight change or feature request; Libft checks their registered type/version/size/budget, not their voxel meaning.

### Resolution transaction

1. Validate phase, actor, revision, instance ownership, inputs, station/world host authorization, and all hard limits.
2. Prepare a reversible engine transaction and host proposal. Reserve resources/output space before accepting a card.
3. Resolve native/script effects against the **current** session rules, including deterministic draws and choices.
4. Validate every emitted domain intent; no arbitrary string commands and no live-world mutations in callbacks.
5. Commit engine changes and durable consumer proposal through one journalled transaction identity. Persist RNG and result with it.
6. Publish the outcome; replay/retry cannot emit duplicate cards, resources, instability or structures.

Host callbacks use prepare/commit/abort with explicit recovery records. Commit must be non-failing after preparation or recoverable through the journal. An in-memory CardGame rollback cannot undo already written Minecraft saves: do not describe that as atomic. On script/validation/allocation failure roll back card zones, obligations, outputs, RNG and host reservations together.

### Durable action-to-host-intent protocol

Delivery is at-least-once; the world effect must be effectively-once through a
stable transaction key and durable deduplication. Do not claim exactly-once
message delivery. The producer identity for each emitted intent is:

```text
(world_id, session_epoch, actor_id, request_id,
 action_sequence, intent_sequence)
```

Libft's generic action/outbox identity contributes the session, request,
action-sequence, and intent-sequence fields; the Minecraft adapter adds the
world epoch and authenticated actor namespace. Persist the resulting identity
with the intent schema/version, canonical payload bytes or payload hash, source
action result, and any deterministic outputs/RNG state. `source_event_id` or
recipe ID alone is not a sufficient key for host delivery. The consumer must
reject the same key with a different payload hash and must not silently
reinterpret an older schema. Keep ordering by action/intent sequence; a
dependent action cannot pass an earlier unresolved host intent.

Use this state progression:

| State | Durable meaning | Externally visible behavior |
|---|---|---|
| `UNSEEN` | No accepted action record exists. | Retry may be validated and accepted normally. |
| `PRODUCER_PENDING` | The accepted CardGame transition, deterministic result, and complete outbox are saved; host effect is not yet committed. | Report pending, not successful completion. Block later dependent actions and preserve the card/choice resolution. |
| `HOST_PREPARED` | Minecraft has written a journal record binding the intent key/hash to all participants, expected revisions, and staged after-state. Canonical world/inventory/rules state is not yet published. | Still pending; retries may resume only this identical prepared intent. |
| `HOST_COMMITTED` | Participant state and the committed journal/result are durably checkpointed together. The effect may now be observed. | Return committed outcome; exact replay returns the retained result. |
| `ACKNOWLEDGED` | Libft's pending intent is acknowledged and that acknowledgement is durably saved. | Action is complete; result remains available for the defined retry horizon. |

The host prepares from immutable inputs and expected revisions. Preparation
must reserve capacity and validate authorization, caps, feature compatibility,
and every participant before changing canonical state. The commit boundary
must include the participant after-state and `HOST_COMMITTED` record in one
atomic session/world checkpoint. If participants live in separate files, use a
shared write-ahead journal that makes recovery deterministic; independent
atomic file replacements are not an atomic multi-store transaction. Only
after that durable commit may the consumer acknowledge the Libft intent.
Persist the producer's acknowledged state after the consumer commit. If that
last save fails, leave the intent pending so retry can read the committed
consumer record, return its exact result without reapplying effects, and retry
the acknowledgement.

On restart, reconcile by the stable intent key before accepting new dependent
actions:

| Durable observation | Required recovery |
|---|---|
| No producer record | Treat the request as unseen; no host effect may exist. |
| Producer pending, no host journal | Revalidate the stored payload and prepare the same intent; never regenerate its random result. |
| `HOST_PREPARED`, no committed participant checkpoint | Resume from its stored after-state or abort to the stored before-state. Do not expose partial mutations. |
| `HOST_COMMITTED`, producer intent still pending | Verify the payload hash and committed revision, return the stored result, then persist the producer acknowledgement without reapplying the effect. |
| Producer says acknowledged but host commit/result is absent within the declared retention horizon | Treat as corruption/invariant violation and fail closed; do not guess or replay an untracked mutation. Expired requests outside that horizon must be rejected by the persisted high-water mark, not treated as new actions. |

For an action with several intents, commit them in sequence and retain the
completed prefix. If a later intent fails permanently, the action cannot be
reported fully committed; either the enclosing domain transaction must roll
back the entire unexposed batch, or the action contract must explicitly define
partial completion and a resumable remaining suffix. World Loom card effects
that jointly change rules, instability, reservations, and feature plans use
one enclosing transaction, not independent per-intent commits. Chunk generation
after those plans are accepted is a separate asynchronous transaction: its
generation epoch, rules hash, reserved chunk identity, and publication result
must be revision-checked and recoverable without charging or consuming cards
again.

Keep committed result records at least as long as the producer can redeliver
their intent or the caller can retry the action. Any bounded compaction policy
must persist a high-water mark and reject expired IDs; eviction must never make
an old request look unseen. A `COMMITTED` result includes stable output IDs,
new inventory/resource revisions, rules/instability revision, and accepted
feature-plan IDs as relevant. The client-facing action result is not final
until the transaction's declared participants have crossed this commit
boundary.

Acceptance coverage must inject failure before producer persistence, after
producer persistence, after host prepare, between each participant mutation,
before the committed checkpoint, after commit but before acknowledgement,
after acknowledgement persistence but before response delivery, and during
restart reconciliation. Assert one committed effect, unchanged state on
pre-commit abort, exact payload-conflict rejection, stable result replay,
ordered intent handling, and no duplicate card/resource/instability/feature
changes. Libft tests cover transactional outbox state, ordering, snapshots and
acknowledgement. Minecraft validators cover the shared journal and actual
inventory/world/session participants. Neither layer's unit test alone proves
the combined crash-recovery contract.

Mandatory choices can span actions: persist a resolving-card record, queued follow-up effects, and obligation. Do not retain callback stack frames across frames. While a discard is pending, only permitted choice/timeout actions are accepted. Confirm waits until the engine resolves obligations.

## 4. Deck, hand and discard rules

Minecraft policy: initial owned deck 20 cards; configurable accepted deck size 20–100; maximum four copies per definition; hand cap 10. Audit existing engine deck/zone limits against the 100-card maximum before choosing storage. Higher library hard limits do not change gameplay limits.

The 20 cards are the player's complete initial deck, not the opening hand. At
the default World Loom deal the session starts with 4 cards in hand and 16 in
the draw pile; selecting 5, 6 or 7 cards moves only that many into the opening
hand, leaving 15, 14 or 13 in the draw pile. The additional-card price changes
the opening deal only; it does not create extra cards or enlarge the owned
deck. Imported/crafted cards may increase the deck up to the configured 100
card maximum, while the four-copy rule is checked across all owned deck zones.

Zones: draw pile, hand, resolution, discard, burned-for-session, and optionally consumed external cards. The CardGame instance ID follows a card through every session zone. A crafted card also has a distinct 64-bit inventory-item instance ID; the Minecraft adapter persists a one-to-one mapping between that physical ID and the session's CardGame instance ID. Never assume the two numeric IDs are interchangeable. Session burns prevent repeated draw/burn cycling, but do not silently delete a player's permanent collection. Permanent consumption of crafted items is an explicit separate economic rule. Deck-copy validation includes cards across all deck zones and any persisted deck membership.

Draw procedure resolves one requested draw at a time:

- Draw from the draw pile using engine RNG/order.
- If the pile is empty and discard reshuffling is allowed, shuffle the eligible discard once using persisted RNG, then retry. Burned/resolving cards are excluded.
- If no eligible card remains, record an empty-deck attempt and emit the configured instability event; do not fabricate a card or recursively draw forever.
- If the hand is already full, move the drawn instance to the burn zone and emit overflow instability. Do not simply return the existing `FT_ERR_FULL` and lose the draw.
- A normally successful draw moves that instance into hand. Each event has its own stable identity and exact resulting counts.

This is the proposed explicit resolution of roadmap reshuffle wording: update the roadmap to the same policy at implementation. Instability amounts remain versioned Minecraft configuration. Empty-draw and overflow costs accumulate alongside a card's own instability cost.

For Double Draft, move the played instance to resolution first, resolve two draws, then require one discard from the resulting hand. If the hand is empty because no draws succeeded, resolve the impossible obligation with the documented no-card outcome; otherwise require exactly one valid owned instance. Discard is not burn and does not grant another draw. Expiry selects a deterministic valid default, never a random UI choice.

Crafted cards are not inserted into the starter draw pool automatically. An
authorized import moves one physical card item from inventory into the
session's draw pile; it does not duplicate the payload. Export may return a
physical card only when the session deck remains at or above its 20-card
minimum. If additional reserve zones or import of cards into a newly composed
deck are later allowed, that is an explicit world policy and still respects
copy/size constraints. Ownership transfer removes the item from its source
exactly once.

## 5. Minecraft regeneration adapter

Suggested consumer files (new names are proposals): `src/cards/RegenerationCardCatalog.*`, `RegenerationCardSession.*`, `RegenerationEffectHost.*`, `src/inventory/ItemCatalog.*`, `src/crafting/RecipeCatalog.*`, and focused validators. Integrate existing World Loom/session and generation classes instead of creating competing state machines.

Expose the start flow through the Minecraft-owned World Loom/session service,
not through `card_game_engine` (which must not know chunk coordinates or dust
economy). Proposed calls are:

| API | Contract |
|---|---|
| `begin_regeneration_selection(actor, loom_id, request_id, session_out)` | Authorize use of this loom and open a bounded, revisioned selection draft. It reserves neither chunks nor resources and does not yet start the CardGame session. Exact retry returns the same draft. |
| `select_regeneration_chunk(session_id, actor, coordinate, expected_revision, request_id, result)` | Add one chunk if it is in bounds, known/eligible, unprotected, unoccupied, outside every occupied-player exclusion ring, not already reserved, and below the selection cap. Increment selection revision only on change. |
| `unselect_regeneration_chunk(session_id, actor, coordinate, expected_revision, request_id, result)` | Remove exactly the requested selected coordinate; reject a stale selection revision, and make an exact retry idempotent so it cannot remove a different selection. |
| `query_regeneration_start_cost(session_id, actor, expected_selection_revision, expected_policy_revision, starting_card_count, request_id, result)` | Read-only calculation from the server-owned selected set and versioned pricing config. Reject stale revisions. Return paid/bonus chunk counts, chunk cost, starting-card cost, total, currency ID, the revisions used and a structured rejection reason; no dust/deck mutation. |
| `cancel_regeneration_draft(session_id, actor, expected_selection_revision, request_id, result)` | Cancel only `DRAFT_OPEN`; idempotent owner-authorized cancellation clears selection without spending. Reject stale selection and all attempts after start commit. |
| `commit_regeneration_start(session_id, actor, expected_selection_revision, expected_policy_revision, starting_card_count, request_id, result)` | Revalidate both quote revisions and all eligibility, reserve chunks, debit/reserve exact dust, initialize the CardGame with the requested opening hand, persist the idempotency result and install barriers in one recoverable transaction. Exact retry returns the same outcome. This is the non-cancellable start boundary. |
| `query_regeneration_status(session_id, actor, result)` | Return lifecycle state, selection/policy revisions, deadline, committed request, progress/failure code, and whether player cancellation is allowed; redact other actors' data. |

The current `World` quote groundwork stores one server-owned
`WorldRevisionCostPolicy`. Policy changes use compare-and-set revisions, and a
quote must present both the current selection revision and cost-policy
revision. World Revision entries now track paid versus host-authorized bonus
chunks separately; `select_revision_bonus_chunk` applies the same eligibility,
protection, cap, and revision checks as ordinary selection. Quotes price only
paid chunks and return both counts. The session coordinator still needs to own
actor, loom, request and reservation state around this lower-level API.

Current source call anchors are `World::update_revision_chunk_selection`,
`World::select_revision_bonus_chunk`, `WorldRevisionServer::query_start_cost`,
and `GameSession::commit_regeneration_start` / `finalize_regeneration_start`.
The owner-bound server also exposes authorizer, occupancy-provider,
start-committer and checkpoint-committer hooks. There is not yet a single
public C++ World Loom service exposing the complete proposed begin/select/
unselect/quote/commit API to a future UI caller; selection currently spans the
World facade and server request path. Keep the table above as the target
backend contract and reconcile these call boundaries before UI work begins.

The implementation map for that future service is:

| Future operation | Current implementation path | Missing facade/guarantee |
|---|---|---|
| Open a selection draft | The authenticated `begin` request claims the single draft by actor/Loom after station authorization; `WorldRevisionServer::initialize` only initializes the server after required authorizers/providers are configured. | A direct `begin_regeneration_selection` method with an explicit session/draft handle is absent. Draft ownership and lifecycle must remain in the server/session, never in a UI-owned object. |
| Select or unselect | World Revision `select`/`unselect` request handling calls `World::update_revision_chunk_selection(expected_revision, cap, x, z, selected, revision_out)` after authorization, occupancy refresh and request-ID checks. | Expose typed request/result values rather than requiring UI code to construct protocol JSON. Preserve compare-and-set revisions and exact retry behavior. |
| Cancel a draft | `WorldRevisionServer` accepts owner-authorized `cancel` only while its draft is owned and not committed; `WorldRevisionManager::cancel` rejects if barriers are active or generation is running. | Add the typed draft-only cancel method; ensure its result is durable/idempotent with the session coordinator. A committed decision window must never be routed to this operation. |
| Add a host-authorized bonus chunk | `World::select_revision_bonus_chunk(...)` applies eligibility and selection-revision checks while marking the chunk as bonus. | Keep bonus authorization server-only; clients may request a quote but cannot label a paid chunk as bonus. |
| Quote the start | `WorldRevisionServer::query_start_cost(actor, loom, request, selection_revision, policy_revision, opening_count, quote_out)` delegates to the world quote and binds retries to the owner/request. | Return the complete quote/rejection object through the same typed service as selection; the quote must be read-only. |
| Commit the non-cancellable start | `WorldRevisionServer` invokes its configured start committer, bound by `GameSession::bind_world_revision_server`; `GameSession::commit_regeneration_start(...)` validates the quote and journals the currency/card/start state. `finalize_regeneration_start(epoch)` resolves accepted card effects and starts generation. | One recoverable transaction still has to cover durable chunk reservations, barriers, session/deadline metadata, resource debit, opening deal and publication recovery. The direct call is an internal host callback, not a UI endpoint. |
| Query lifecycle status | `World::world_revision()` exposes pending/regenerating flags and selection metadata; the server/session retain additional owner, deadline and committed-request state. | Add one authorized typed result spanning draft, committed decision window, queued/generating, complete and retryable failure, including whether cancel is permitted. |

The public façade should return a stable draft/session ID, selection and policy
revisions, structured eligibility/rejection reasons, paid/bonus counts, exact
cost breakdown and committed lifecycle state. It must not return mutable world
or CardGame internals. Keep a pure quote operation separate from the irreversible
commit operation, and make cancellation a draft-only operation; after successful
commit, player cancellation is rejected while server recovery remains possible.

The starting deal is selectable from **4 through 7 cards**, defaulting to 4.
Let `B4` be the configured total dust price for a four-card opening hand. The
total opening-deal price doubles for each additional starting card:

```text
opening_card_cost(n) = B4 * 2^(n - 4),  n in [4, 7]
```

Thus prices are `B4, 2*B4, 4*B4, 8*B4` for 4, 5, 6, 7 cards. These are total
prices for the opening deal, not a per-card marginal sequence. Compute with
checked integer arithmetic and a configured cap; reject invalid counts,
overflow, insufficient deck size, insufficient dust, stale selection revision,
or changed eligibility without partial debit, chunk reservation, hand deal,
or barrier placement. The query returns a structured authoritative quote for a
future client to present; UI layout and implementation are explicitly out of
scope here. The starting deal uses the existing `start_match` opening-hand
configuration; subsequent paid draws continue to use the separate draw-price
schedule in the roadmap.

Chunk selection edits are allowed only in the draft stage. Once
`commit_regeneration_start` succeeds, the selection is frozen and the existing
no-cancellation policy applies. Card effects that add scope go through the
same chunk-eligibility/reservation checks; paid and explicitly bonus chunks
remain separately reported and priced according to versioned world policy.

Each session starts from a fresh immutable base-world rules snapshot. Build a session-local working rules value; apply cards sequentially to its current state. Freeze the final value for workers. Never mutate authoritative base-world rules, and never carry yesterday's temporary biome/ore changes into a new session. Persist provenance and permanent per-chunk instability/modifiers separately.

### World Loom lifecycle state contract

Treat these as externally meaningful logical states even if persistence stores
them as a smaller set of flags/revision fields. The API must return the current
state with every successful mutation and allow a caller to query it after
reconnect. A pair of booleans such as `pending`/`regenerating` is not a
sufficient public lifecycle result because it cannot distinguish a mutable
draft from a committed decision window or a retryable failure.

| Logical state | Allowed player actions | Durable invariants / next transition |
|---|---|---|
| `NO_DRAFT` | Begin a draft | No selection owned by this World Loom session; no spend or barrier. |
| `DRAFT_OPEN` | Select/unselect eligible chunks, request quote, cancel draft | Selection and pricing revisions are authoritative. Cancellation clears draft selection and spends nothing. Exact request retry returns the same result. |
| `START_PREPARED` (internal recovery state) | No further player mutation | A journal entry is checkpointed before currency/deck mutation. If commit fails or recovery finds only this prepared entry, restore the pre-debit/pre-deal state and mark it aborted; it is not an accepted regeneration and may be retried with a new request ID. |
| `DECISION_WINDOW` | Play legal cards, complete required discards/choices; no selection edits or cancel | Start is committed, currency is spent once, the selected set is frozen, the opening hand is dealt once, barriers are active, and the authoritative five-minute deadline is persisted. Disconnect/restart does not reset the deadline or refund resources. |
| `GENERATION_QUEUED` | No cancel; continue status queries | Deadline has expired or all decisions are resolved; deterministic defaults resolve remaining choices/discards, accepted effects are applied, and the final rules snapshot is frozen before work admission. |
| `GENERATING` | No cancel; query progress | Worker results are revision-checked; barriers stay active until the authoritative replacement/repair policy permits release. No worker reads mutable session state. |
| `COMPLETE` | Normal play resumes | All selected chunks and required multi-chunk features are published consistently; barriers are removed only after publication and durable completion state. |
| `FAILED_RETRYABLE` | No player reroll/cancel; trusted recovery/retry only | Preserve the committed cost, accepted cards, reservations and barriers. Retry the same committed generation plan; do not silently choose a new seed/rules snapshot. If safe retry is impossible, use an explicit repair/refund policy with an audit record (still an open production decision). |

The current source implements parts of this contract: `WorldRevisionServer`
allows owner cancellation only before start commit; `WorldRevisionManager`
rejects cancel once barriers are active or generation is running;
`GameSession::commit_regeneration_start` writes a prepared journal/checkpoint,
debits the quote and deals the opening hand, then activates barriers/deadline
and marks the start committed; `finalize_regeneration_start` waits until the
authoritative deadline, resolves pending choices/discards and intents, then
starts generation. `retry_failed_regeneration` preserves the barrier and
retries the failed revision. These are source-level findings, not proof that
all lifecycle fields and transaction state recover correctly across a process
crash. In particular, the public façade still needs one stable status result
covering every state above, and completion/failure/barrier-release behavior
needs end-to-end save/restart validators. The current `commit_regeneration_start`
also always sets a five-minute deadline; the product requirement names that
window for multiplayer. Add an explicit session mode/policy and make the
single-player behavior deliberate/configurable rather than accidentally
inheriting the multiplayer timer.

The irreversible player-cancellation boundary is the successful
`commit_regeneration_start`, not selection, quote, or the later worker start.
Any error before the journal reaches committed state must restore the
pre-start inventory and card session and must not leave a live barrier or
deadline. Once committed, user cancellation, selection edits, and resource
refund-by-retry are forbidden; only idempotent server recovery can advance or
repair the same operation.

Occupied chunks and all eight adjacent chunks are ineligible until players vacate; validate again atomically when reserving. The World API now accepts a host-fed occupied-chunk set, removes newly unsafe selections, and increments the selection revision when occupancy changes; the live player-location feed and commit-time refresh remain to be integrated. Selection expansion from cards repeats occupancy/protection checks. Barriers reject entry, teleports, vehicles and other authoritative movement paths, not just walking. Existing players cannot be trapped inside. Multiple sessions cannot reserve overlapping chunks or unsafe adjacent areas.

The default is no player cancellation once regeneration has started; implement the exact server lifecycle boundary and communicate it before resource spending. Volatile Stabilizer cannot reopen cancellation. Five minutes is measured by authoritative monotonic server time with restart recovery, not a user clock. Expiry resolves pending choices by defaults and generates with all accepted effects; spent resources are not refunded. Server safety failures use recoverable abort/repair rules, not a player-facing reroll mechanism.

Generation never queries a mutable session. Feature footprints are reserved atomically, including required halo; multi-chunk structures cannot half-publish. Stale results cannot replace current chunks. Rendering priorities remain accepted block edits first, light updates second, new chunk publication third; this design does not move generation/lighting into the render loop.

Persist per chunk: initial instability (default zero, optional world challenge setting), current instability, permanent modifier IDs/versions, feature outcome IDs, generation revision, rules hash and reward/danger bands. No floating global number may substitute for per-chunk records. Kiss/curse tables provide real reward opportunities coupled to danger, monotonic tiers and hard caps; increased instability does not hand out unconditional free loot.

#### Current per-chunk persistence and behavior (source-audited)

`WorldChunkInstabilityStore` is a World-owned sparse map keyed by signed chunk
coordinates. Each materialized record stores initial/current integer values,
reward and danger bands, record revision, generation revision, rules hash, and
versioned permanent-modifier/feature-outcome IDs. Its bounded `WCI1` v1 codec
checksums the payload, allows up to 100,000 records and 64 modifier/outcome
entries per record, and stores a fingerprint of `WorldChunkInstabilityPolicy`.
The latest `GameSessionPersistence` format is MCSP v18; the instability payload
first appeared in v9 and remains an optional opaque section in later versions.
Legacy snapshots before v9 therefore contain no such records.

The fingerprint is **not** the policy itself. The world creation/save layer
must persist the complete policy (maximum, initial default, threshold arrays
and counts, and its schema version) and configure it before `GameSession`
restores WCI1; the current MCSP payload cannot reconstruct those settings on
its own. The World rejects a payload configured under a different policy
fingerprint. A new world should explicitly configure `default_initial_value`
to zero unless the player selects a challenge value; an unconfigured store is
an error, not an implicit zero policy. When importing a pre-v9 save, define an
explicit migration that applies the saved/world-selected default to existing
chunks and does not silently raise challenge or erase recorded outcomes.

`get` can synthesize the configured default for a coordinate without storing a
record. The current source explicitly materializes records in the World
Revision regeneration path; ensure other initial-generation/publication paths
also create a record when a nonzero challenge default or durable generation
provenance requires it. `WorldRegenerationResultApplier` records generation
revision, rules hash, and feature outcomes and rolls back the in-memory chunk
publication if the instability-store commit fails. That rollback does not
prove crash-atomic persistence between chunk data and the MCSP/WCI1 state; a
single recoverable world-save transaction or equivalent repair journal remains
required. `WorldChunkInstabilityPublicView` exposes coordinates plus reward and
danger bands, not exact instability values or hidden modifiers; the
WorldRevisionServer requires a configured authorizer for that query.

One gameplay decision needs resolution before Volatile Stabilizer is finalized:
the current store makes **both** reward and danger bands monotonic unlocks, so
reducing `current_value` does not lower either band. This preserves earned
rewards, but it also means the card's stabilizing branch may not make a chunk
safer to traverse after it has crossed a danger threshold. Recommended contract:
keep earned reward opportunity monotonic, but derive active danger/spawn
pressure from the current value (or a separately reversible danger tier),
while never deleting already committed features or loot. If permanent danger
is intended instead, state that explicitly on the card and in the preview.
Add reduction-crossing tests either way; do not let the store's current
implementation silently decide the game rule.

## 6. Complete current card catalogue and call mapping

Every row registers one stable definition and effect ID via existing registration APIs. Gameplay enters through the adapter path below; generic draw/discard operations stay in Libft, while domain-effect implementation stays in Minecraft. Names such as `biome_remove` and `feature_add` are proposed typed Minecraft **host-intent schemas**, not existing Libft functions. `request_draw` and `require_discard` instead name generic CardGame operations produced by an effect; they are not Minecraft intents. Parameters must be validated IDs, quantities and versioned bounds. The current enabled catalogue comprises seven starter definitions plus twelve non-starter definitions (IDs 11–22); only the seven starter cards seed the 20-instance opening deck.

The currently exposed Minecraft backend path is `GameSession`, not a UI:

| Caller need | Current API / sequence | Contract and gap |
|---|---|---|
| Ask whether a physical card/choice is valid | `query_regeneration_card_effect_status(instance_id, choice, ...)`; `query_regeneration_card_effect_choices(instance_id, chunk_candidates, choices_out)` | Read-only preflight returns status/no-effect information; candidate chunks are suggestions and are rechecked by the session. A future caller must pass a hand instance ID, not a definition ID. |
| Play a card | `play_regeneration_card(request_id, card_instance_id, choice, intent_out)` | Requires active committed regeneration and a pending decision phase. It consumes/records the exact card action idempotently and returns the typed `RegenerationPolicyIntent`; it does not expose mutable World or generation-config internals. |
| Apply the resolved domain effect | `apply_pending_regeneration_card_intent(replayed_out)` | Consumes the pending card intent while checkpointing CardGame and regeneration-rule state together. Host output delivery/recovery is a separate path and must not be acknowledged before durable acceptance. |
| Resolve draw/discard effects | `draw_regeneration_cards(request_id, source_host_intent_sequence, count, required_discard_count, result)` then `discard_regeneration_hand_card(request_id, source_draw_request_id, card_instance_id, result)` | Preserve the source host-intent sequence, mandatory-discard obligation, exact physical instance and retry identity. Do not expose an independent UI-side draw implementation. |
| Move a crafted card into or out of the deck | `import_regeneration_card_item_to_deck(slot, expected_inventory_revision, request_id, ...)` / `export_regeneration_card_to_inventory(instance_id, expected_inventory_revision, request_id, ...)` | Import consumes one inventory item and appends its definition to the session draw pile; export returns a card to inventory only if the 20-card minimum remains. Both preserve the 64-bit physical item identity through a session mapping and check inventory revision. GameSession routes both through a prepared/committed `WorldItemCardTransferTransaction` and startup recovery; public multiplayer authorization and durability/validation gates remain open. |

These are callable backend APIs, not a claim that network routing, UI, or all
multiplayer authorization is complete. The eventual UI should call a
server-authorized service that exposes these operations with stable request IDs,
structured error/status results, and no direct access to Libft zones or world
state. Keep no-effect preflight advisory: the authoritative play path must
revalidate every target and rule against the current session snapshot.

Card-transfer source checkpoint (Minecraft `c422dcc`, source inspected):
`WorldItemCardTransferTransaction` checkpoints a prepared journal record before
mutating inventory/deck state, persists the terminal record with the combined
session snapshot, and replays a pending prepared record during startup
recovery. `RegenerationCardSession` persists the physical-item-to-card-instance
mapping and bounded import/export retry caches. Startup checks physical IDs
across inventory, equipment, ground items, entities, and session card mappings,
and raises the output-ID allocator above those IDs. This is session-local
transaction/recovery machinery, not a proof of host-wide uniqueness or
fsync-level crash durability. The validator source covers prepared import
recovery and exact retry; public GameSession wrapper behavior, export crash
boundaries, and save-failure rollback on the fetched branch still need focused
runtime validation. Network actor authorization remains outside these current
single-player session methods.

Concrete per-card call sequence (current function names; the typed names in
the card tables below describe the policy operation, not a C++ symbol):

```text
GameSession::query_regeneration_card_effect_status
  -> RegenerationWorldRules::preflight_card_effect       (read-only hint)

GameSession::play_regeneration_card(request_id, card_instance_id, choice)
  -> resolve physical hand instance and enabled recipe/unlock
  -> RegenerationWorldRules::preflight_card_effect       (authoritative recheck)
  -> RegenerationCardSession::play_card                  (sequenced Libft action)
  -> persist action and return RegenerationPolicyIntent

GameSession::apply_pending_regeneration_card_intent
  -> snapshot card/rules/instability state
  -> RegenerationWorldRules::consume_pending_card_intent
       -> apply_intent                                    (rules overlay)
       -> acknowledge the source host-intent sequence
  -> update pending World Revision's frozen rules view
  -> perform card-specific follow-up draw, if any
  -> apply configured per-chunk instability, if this card has a cost
  -> checkpoint; restore all snapshots if any step fails
```

#### Current CardGame host-intent encoding (source-audited)

The current adapter does **not** serialize one host-intent schema per name
such as `biome_remove` or `feature_add`. It registers a compact envelope and
decodes it into `RegenerationPolicyIntent`; Minecraft then dispatches by
`effect_kind`. Preserve this distinction when adding card definitions. The
following is the current Libft host-intent representation at the reviewed
Minecraft/Libft tips, not a proposed replacement wire format:

| Record | Type ID | Version | Fields in order |
|---|---:|---:|---|
| Card policy intent | `0x4D435250` | `CONTENT_VERSION == 2` | `U32 card_definition_id`, `U32 card_instance_id`, `U32 effect_kind`, `U32 choice_kind`, `U32 choice_primary OR private_result`, `U32 choice_secondary OR private_ordinal`, `U32 content_version`, `U64 request_id` |
| Empty-draw notice | `0x4D435245` | `CONTENT_VERSION == 2` | `U32 player_id`, `U32 zero` |
| Hand-overflow notice | `0x4D43524F` | `CONTENT_VERSION == 2` | `U32 player_id`, `U32 overflow_count` (nonzero) |

The engine record's monotonic `sequence` is outside the payload. For policy
intents, fields 0–6 must be U32, field 7 U64, request and card-instance IDs
must be nonzero, and the card definition must map to the exact registered
effect kind. Field 6 must match both the outer version and the supported
content version. Choice kinds are `0` none, `1` biome, `2` ore, and `3` target
chunk; target chunk coordinates use their 32-bit modulo representation.
Fields 4–5 decode as choice values for ordinary cards. For Volatile Stabilizer
they instead hold the private overload result and use ordinal; for Unstable
Catalyst they hold the private selected modifier ID and use ordinal. Those
private values must never be sent in an ordinary client view. The no-effect
choice mask is response metadata, not part of this engine payload.

Definition IDs and effect-kind enum values are separate stable namespaces.
The current mapping is:

| Card definition ID | Effect-kind value | Card |
|---:|---:|---|
| 1–7 | 1–7 respectively | Starter cards in the order in the starter table |
| 11 | 8 | Ore Overgrowth |
| 12 | 9 | Ultimate Greed |
| 13 | 10 | Biome Rupture |
| 14 | 11 | Fortune's Draw |
| 15 | 12 | Volcano Mark |
| 16 | 13 | Volatile Stabilizer |
| 17 | 14 | Ruined Village Relic |
| 18 | 15 | Shipwreck Chart |
| 19 | 16 | Unstable Catalyst |
| 20 | 17 | Desert Pyramid Seal |
| 21 | 18 | Ancient Canopy Seal |
| 22 | 19 | Spider Nest Seal |

Do not derive one ID from the other. Adding or changing a field, field type,
choice interpretation, or enum mapping requires a content/intent version
migration and replay compatibility tests. Existing draw notices are separate
from policy effects and must retain their source sequence through the
`draw_regeneration_cards` action path.

The current GameSession calls `consume_pending_card_intent` inside its
integration transaction; that helper applies the overlay intent and
acknowledges the matching sequence. Do not acknowledge it a second time.
Double Draft's queued draw notice is resolved by the separate
`draw_regeneration_cards` / `discard_regeneration_hand_card` action path.
Ultimate Greed and Fortune's Draw instead perform their follow-up draw through
`RegenerationCardSession::draw_cards` inside `apply_pending_regeneration_card_intent`,
keyed by the original request and host-intent sequence. Keep those routes
explicitly idempotent; do not synthesize a UI-side draw. Before workers start,
the World Revision manager materializes ore scales into a staged generation
config with `RegenerationWorldRules::apply_ore_scales` and freezes the rules;
never query mutable rules from a worker.

Resolution order is deliberately split across the two owners. The card play
enters Libft through `RegenerationCardSession::play_card` (called by
`GameSession::play_regeneration_card`) and returns a sequenced card intent; it
does not directly mutate World. The host then routes the result by kind:
domain policy effects are validated/applied by
`GameSession::apply_pending_regeneration_card_intent`, while a queued draw
notice is resolved through `GameSession::draw_regeneration_cards` with its
source host-intent sequence. Libft processes that draw as a sequenced command,
including hand-cap burns and any required-discard obligation; the caller
fulfills that obligation with `discard_regeneration_hand_card`. Other generic
host outputs use `query_pending_regeneration_host_outputs` and
`acknowledge_regeneration_host_output` only after durable acceptance. Never
re-enter `draw_regeneration_cards` from inside an effect callback: it is a
separate authoritative action that resolves a queued notice, not a callback
primitive, and invoking it recursively would break the command transaction.

### Starter deck: exactly 20 instances

| Definition ID — card / copies | Ordered effect and host calls | Validation / no-effect behavior |
|---|---|---|
| 1 — Biome Exclusion / 4 | Choice selects one biome; emit `biome_remove(id)` | Remove only from current eligible list. Already absent has no effect; reject removal of the final viable biome. Return a structured no-effect reason; UI wording/layout are out of scope |
| 2 — Deep Seam / 4 | Choice selects ore; emit `ore_scale(id, 3, 2)` | Increase **current** selected spawn rate by 50%, checked integer rational arithmetic and configured cap, not ambiguous extra ore count |
| 3 — Village Charter / 2 | Emit `feature_add(village, 1)` | Each use adds another village plan; validate full safe footprint/cap. Do not collapse multiple uses to a boolean |
| 4 — Surface Magma / 2 | Emit `feature_add(surface_lava_pool, configured_count)` | Surface-only supported, bounded pools; reserve feasible positions, contain fluid, exclude protected areas |
| 5 — Double Draft / 2 | Emit generic `request_draw(2)`, then `require_discard(1)` | Hand-cap burns and empty draws apply; required discard blocks further normal actions |
| 6 — Biome Compass / 4 | Emit `biome_weight_adjust(configured_set)` | Only currently eligible biome IDs; cannot restore excluded biomes implicitly; normalize deterministically within caps |
| 7 — Surveyor's Reach / 2 | Emit `target_scope_add(1)` and required chunk choice | One additional chunk this session, subject to occupancy/protection and budget; reserve before accepting |

The starter multiset is `4 + 4 + 2 + 2 + 2 + 4 + 2 = 20`. Cards with targets offer server-generated choices bound to the current rules/config revision. Important current-source limitation: biome choices are one-based indices into `voxel_generation_config`, not globally stable biome IDs. They may be used only with the exact generation/content manifest that produced the choice; never persist or replay them against a different biome ordering. Before cross-version save/network replay, migrate to stable biome IDs or reject with an explicit migration-required result. Ore choices currently use block IDs and still require validation against the active ore table. Reject impossible structural guarantees before consuming their card; optional generation rolls cannot satisfy a guaranteed feature promise. A capped numerical effect can remain playable as an explicitly shown no-op if world policy permits it.

### Craft-only rare cards

Each instability-bearing rare use consumes its authorized physical item and
emits a configured instability adjustment in addition to its listed effect;
Volatile Stabilizer uses its own reduction/overload rule, while Fortune's Draw
has no instability cost in the current definition. Do not silently apply a
common default amount to all cards. World policy sets footprint and progression
requirements.

| Definition ID — card | Ordered effect / host calls | Constraints |
|---|---|---|
| 15 — Volcano Mark | `feature_add(volcano, 1)`, instability adjustment | Regeneration-only; minimum planned 9×9 chunk footprint and late progression/boss gates; confirm against roadmap before shipping |
| 12 — Ultimate Greed | `request_draw(3)`, instability adjustment | Overflow burns and empty-deck events add their own costs; no compulsory discard unless configured separately |
| 13 — Biome Rupture | Two distinct biome choices, `biome_remove` twice, instability adjustment | Cannot remove last viable biome; absent targets are clearly identified as no-effect, not silently substituted |
| 11 — Ore Overgrowth | `ore_scale(selected, 2, 1)`, instability adjustment | Doubles current rate, checked arithmetic and safety cap; composes in play order with Deep Seam |
| 18 — Shipwreck Chart | `feature_add(shipwreck, 1)`, underwater-hostile plan, instability adjustment | Ocean only, navigable underwater structure; loot/danger use final instability tier |
| 17 — Ruined Village Relic | `feature_add(ruined_village, 1)`, instability adjustment | Hostile spawners and rare loot; biome-adapted variant only where explicitly supported |
| 19 — Unstable Catalyst | Persist hidden deterministic `permanent_modifier_add(selected)`, instability adjustment | Backend knows result; client does not. Never removable by ordinary cards, reload, cancellation or reselecting a seed |
| 20 — Desert Pyramid Seal | `feature_add(desert_pyramid, 1)`, instability adjustment | Desert, sandstone, reachable central loot and bounded spawners |
| 21 — Ancient Canopy Seal | `feature_add(giant_forest_tree, 1)`, instability adjustment | Forest, climbable ladder interior, trapdoors, deliberately dark hostile chambers and loot |
| 22 — Spider Nest Seal | `feature_add(cave_spider_nest, 1)`, instability adjustment | Must connect to an existing cave system; bounded spider spawners, loot-bearing corpses |
| 16 — Volatile Stabilizer | Increment use counter; roll configured overload chance; emit reduction or irreversible surge plus danger/reward opportunity | Example risk curve 5%, 15%, 35%, 65%, 100%; illustrative only. No fixed instability numbers in this design. Immediate resolution, persisted use count and RNG, no cancellation/reroll |
| 14 — Fortune's Draw | `request_draw(1)` through the configured draw policy | Discovery-locked recipe 2019; draw/empty/overflow effects are sequenced and idempotent; no instability cost unless a future content version explicitly adds one |

Unstable Catalyst's validated dangerous/potentially dangerous pool includes shipwreck, volcano and ruined village variants plus eligible generic hostile overlays. Cards do not unlock unsafe placement. Generic instability content includes hostile towers, camps, spawner complexes, cursed mines, fortifications, nests and vaults. Towers may use a taller ocean-supported variant above water. Docks are not a required gameplay feature. Biome-specific reward structures remain card content; generic danger remains instability content, with explicit shared feature planners permitted.

Hidden results must be redacted from client snapshots, previews, ordinary logs and accessible RNG state. Server replay retains them. Hashing a small secret option alone does not provide secrecy; use redacted views and authenticated server records rather than exposing enumerable hidden hashes.

### Later cards (not enabled content)

Biome Guarantee is a future definition: it would emit a feasible guaranteed
biome plan and optionally `request_draw(1)` only if that behavior is explicitly
part of its versioned definition. Keep it disabled until its feasible biome
planner and recipe are implemented. Fortune's Draw is already an enabled
non-starter definition (ID 14) with discovery-locked recipe 2019, as described
above; do not count it as an unimplemented future card or add its effect to a
starter definition.

## 7. Scripting extension

Use Libft's existing custom runtime directly, not Lua or the legacy line-command adapter. The runtime supports integer/string/boolean/null values; use bounded integer rational/fixed-point quantities, not invented floating-point script values.

Proposed `card_game_script_bridge` lifecycle: initialize with immutable definitions/bindings, register a stable native manifest, compile/verify each effect, bind its program to a registered CardGame effect, execute under the engine transaction, destroy owned state. Keep native callbacks as an equally supported path. Examples below are proposed bridge native functions, not current `scripting_engine` methods:

| Binding | Contract |
|---|---|
| `card_actor_id()`, `card_instance_id()`, `session_phase()` | Immutable context queries |
| `choice_selected_id(choice_id)` | Only resolved authorized choices; no trusting arbitrary client IDs |
| `draw_cards(count)`, `require_discard(count)` | Append engine operations; no bypass of policy |
| `emit_domain_intent(type_id, target_id, amount, numerator, denominator)` | Typed registered schema and bounded integer payload; no arbitrary world writes |
| `random_bounded(bound)` | Engine-owned persisted RNG with unbiased bounded sampling and recorded outcome |
| `context_value(key_id)` | Implemented in the optional bridge for up to 64 host-registered signed-integer keys. Key IDs are increasing/nonzero and enter the bridge manifest; values are set between actions and excluded from content identity. The native exposes no save-file or live-World access. The bridge provides bounded checksummed `serialize_context_values` and schema-checked transactional `restore_context_values`; the consuming session remains responsible for storing and restoring that payload. |

A Deep Seam script can obtain its already selected ore ID and emit `ore_scale` with 3/2. Double Draft uses draw and obligation natives. Feature placement and instability tier computation remain host functions, not giant scripts that traverse voxel arrays.

Use existing `register_native`, `compile`, `verify_program`, `execute_program`, `serialize_program` and `deserialize_program`. Compiled native IDs depend on binding registration order: specify canonical order and persist a native-ABI manifest hash. Include source/program version, CardGame content version, host schemas and policy in the rules manifest. Refuse unknown or mismatching bindings on load; never silently execute the same numeric ID as another native.

Existing bounded runtime constants include 128 native functions, 32 arguments/locals, 65,536 source bytes, and bounded tokens/instructions/string storage. Validate actual header limits during implementation. Configure tighter per-effect operation/output/RNG budgets and charge nested/native host work too. An instruction limit alone cannot bound an expensive callback. No filesystem, network, wall clock, renderer, threads, recursion into another action, or unbounded allocation in card natives.

Each mutable scripting engine has one owner. Share only demonstrably immutable verified program data; callbacks hold invocation-scoped context with safe lifetime. Errors include source diagnostics and an explicit result; do not swallow failures or keep a dangling `user_data` pointer after the action.

## 8. Inventory and item-instance foundations

Extend existing Game classes with immutable item definitions and revisioned transactions. Separate item definition ID, physical instance ID, block ID and card instance ID. A physical card item/deck entry shares one ownership identity or a journalled bijection; never two independent copies. Stackable commodity lots need not each allocate unique per-unit objects. Durable tools, boats, cards and stateful buckets require appropriate instance identity.

Item definition fields: stable namespaced ID, catalogue version, max stack, tags, weight/size, kind, optional placed-block ID, equipment slot/tier/durability, consumable stats, fluid capacity/kind, card definition link and acquire/drop policy. Presentation strings/icons are not authoritative IDs. Validate all references and integer capacities at registration.

Proposed Game APIs:

- `game_inventory::prepare_transaction(request, expected_revision, prepared_output)` plans split/merge/add/remove/transfer against capacity, weight and instance state.
- `commit_transaction(prepared, transaction_id, result)` performs a pre-reserved non-failing commit or journalled recoverable commit; duplicate IDs return the same result.
- `get_inventory_snapshot(output)` returns a checked immutable view; do not expose mutable container pointers to clients.
- `game_item_catalog::validate_references(block_policy_callback, output)` checks consumer-supplied references without importing Minecraft.

Transfers lock participants in the library's prescribed order or run under a documented single simulation owner. Do not hold inventory locks during script execution, generation or rendering. Save immutable snapshots asynchronously; persist an authoritative journal before acknowledging cross-system economic changes.

Minecraft applies reach, ownership, claim, tool, station and chunk revision checks. Block destruction and ground-item creation commit together; pickup can be partial with exact remainder. Fluid collection and boat item/entity conversion are also transactions. Character-bound versus portable inventories/unlocks obey the world's join policy; imported progress cannot bypass world restrictions.

### 8.1 Character/world scope at the Minecraft boundary

The world-save roadmap defines `WORLD_BOUND` and `PORTABLE` characters and a
per-world admission policy. Apply that policy to these systems explicitly;
do not infer portability merely because two records have the same character
ID:

| State | Default ownership/scope | Admission behavior |
|---|---|---|
| Active World Loom regeneration session (selected chunks, dealt hand, draw/discard/burn zones, pending choices, RNG, locked snapshot and host-intent results) | World + one regeneration-session ID | Never imported into another world or resumed as a portable character session. It completes or recovers only in its owning world. |
| World Loom deck collection and physical card instances | Character inventory/profile under its declared scope; each physical instance has one canonical owner | Apply destination inventory/card policy and the character's ownership mode. A join must not create a second card instance or duplicate deck membership. The active session's temporary zones are not the collection. |
| Ordinary inventory/equipment/physical items | Character- or world-owned according to the persisted ownership record and admission policy | Deny, keep source-world, or transfer only through an explicit versioned policy. Do not silently copy a physical instance into a destination save. |
| Recipe-book unlocks and discoveries | Each grant has stable recipe ID, source-event ID, and an explicit character/world scope | Import only explicitly portable grants accepted by the destination policy. World-local discoveries remain in the source world; destination-disabled recipes stay unusable even if a portable grant is retained. |
| Card definitions/deck legality and world rules | Catalogue/ruleset-versioned; world-specific legality is evaluated by the destination | Revalidate every imported card against destination format, copy limit, unlock and content-version rules. Never trust a client-supplied deck or unlock list. |

The durable record for an unlock or physical item must retain enough scope and
source identity to decide admission after a save/reload. Libft should preserve
stable IDs and unknown versioned fields without knowing what a Minecraft world
is. Minecraft owns the scope labels, admission decision, source/destination
transaction, and migration policy. An unmarked legacy record is not implicitly
portable: use the documented migration/default policy and report quarantined
or denied state instead of silently granting it.

The backend join path should expose value-only, revision-bound operations along
these lines (names are illustrative, contracts are required):

1. `query_character_admission(character_id, world_id)` returns the effective
   world-policy and character/profile revisions, the state categories allowed,
   retained in the source, denied, or requiring approval, plus migration
   warnings. It has no side effects and reveals no hidden source-world data.
2. `prepare_character_join(request_id, character_id, world_id, expected_revisions)`
   validates admission and builds an immutable transfer plan. It checks the
   per-character lease, physical-ID uniqueness, inventory/card caps, recipe
   scope, and content compatibility before reserving a spawn or mutating data.
3. `commit_character_join(prepared_id, request_id)` atomically publishes the
   admitted session and its ownership/lease record with durable idempotency.
   Exact retries return the same outcome; changed payloads or stale revisions
   are rejected. Failure leaves source ownership and destination state
   unchanged. Leave/disconnect/recovery releases or restores the lease only
   after the latest authoritative state is saved.

Whether a portable inventory is a single account-owned canonical store or is
transferred into and back out of a world-owned save is an implementation
decision that must be resolved before serialization/API freeze. Either model
must enforce one authoritative owner for every physical instance across
concurrent sessions, crash recovery, retries, and world backup/restore. A
per-character active-session lease is necessary but not by itself proof against
duplication after rollback or restore.

Current Libft building blocks are `game_inventory::add_item_revisioned`,
`remove_item_instance`, `remove_item_quantity`, `transfer_to`,
`get_snapshot`, and `restore_snapshot`. These cover revision checks, bounded
in-process request replay for selected operations, and value snapshots; they do
not yet provide the proposed general `prepare_transaction` object or a durable
cross-container journal. Keep the proposed transaction API as the target
instead of treating separate remove/add calls as atomic. Minecraft still owns
global physical-ID uniqueness, world-item/block-edit coordination, and the
durable journal spanning save files.

## 9. Recipe-book API and unlocks

Extend `game_recipe_blueprint`/catalogue rather than inventing an unrelated crafting grid. Proposed fields: stable recipe/version ID, exact tagged/exact-ID ingredients, quantities, bound output definitions and quantities, returned containers/byproducts, station tags, unlock policy, processing ticks, fuel units, limits and compatible variants. Matching duplicate ingredient groups must allocate each input unit once. Alternatives are resolved deterministically and shown before confirmation.

Proposed APIs:

| API | Behavior |
|---|---|
| `game_recipe_book::initialize(catalogue, defaults)` | Validate and grant only configured bootstrap unlocks |
| `game_recipe_book::unlock_recipe_with_result(recipe_id, source_event_id, result)` | Idempotent receipt; separately reports `event_newly_recorded` and `recipe_newly_known`. The compatibility `unlock_recipe(..., newly_unlocked)` reports only whether knowledge changed. Minecraft remains responsible for source namespace/owner and authorization. |
| `query_recipes(filter, inventory_view, station_context, output)` | Known/unknown/available/missing ingredients; no mutation |
| `game_crafting::prepare_craft(request, inventory_view, book, station_context, output)` | Recipe-owned results, checked batch quantities, all capacity and access checks |
| `commit_craft(prepared, transaction_id, result)` | Atomic input consumption, outputs/byproducts and optional unlock grant |
| `start_processing(...)`, `advance_processing(server_ticks, budget, output)` | Reserved input/fuel, bounded timed furnace work, completion pending if output full |
| `export_book(output)` / `import_book(input, catalogue)` | Stable-ID unlock persistence, migration and world policy validation |

Minecraft supplies verified station context: actual station entity/block revision, usable range and permissions. The library must not accept an arbitrary client `has_workbench=true`. Unknown recipes remain locked even if the player guesses ingredients.

The current recipe-book calls are `game_recipe_book::initialize`,
`unlock_recipe_with_result(recipe_id, source_event_id, result)` (with the
legacy `unlock_recipe(..., newly_unlocked)` wrapper),
`is_recipe_known`, `export_book`, and `import_book`. Crafting uses
`game_crafting::register_item_tag`, `register_recipe`, `unlock_recipe`,
`query_recipe` / `query_recipe_at_station`,
`prepare_craft` / `prepare_craft_at_station`, and `commit_craft`. Timed jobs
use `prepare_processing_at_station` and `commit_processing_inputs`, with job
advancement/completion owned by the host. The recipe book and crafting registry
remain separate Libft objects, so the Minecraft catalog must keep their
knowledge states consistent and roll back a partial unlock. Minecraft's
`MinecraftRecipeCatalog::query_recipes` already combines known state, station
availability and ingredient deficits into staged value outputs; do not
reimplement that aggregation in Libft. There is no corresponding public
`GameSession` list-query entry point or unified revision-bearing result yet,
and no generic source-event transaction spanning book grant and
world/inventory persistence; those remain explicit integration work.

### 9.1 Snapshot-consistent query boundary

The current Libft `game_crafting::query_recipe_at_station` accepts a live
`game_inventory` and locks/reads it while evaluating one recipe. Minecraft's
catalog currently calls that once per recipe. This is safe only under the
catalog's single simulation-owner convention; it does not guarantee one view
across a whole recipe list for concurrent readers. Constructing a temporary
mutable inventory just to reuse the query is not the intended solution.

Add a generic, read-only Libft overload that evaluates against the existing
value snapshot, conceptually:

```cpp
query_recipe_at_station_snapshot(
    recipe_id,
    const game_inventory_snapshot &inventory_snapshot,
    station_id,
    result_out,
    missing_groups_out);
```

It must use only snapshot values (`item_id`, quantity, rarity and item-state
fields needed by matching), allocate no `game_item` handles, never mutate the
snapshot or inventory, and return the same ingredient/station answer as the
live-inventory overload for an equivalent state. Validate malformed snapshots
and preserve deterministic recipe-order allocation when ingredient groups
share alternative item IDs. Keep the current overload for compatibility.

Minecraft's `MinecraftRecipeCatalog::query_recipes` should then capture one
`game_inventory_snapshot` through `game_inventory::get_snapshot`, hold a
stable catalogue/recipe-book read view for the duration of the list query, and
evaluate every row against that exact snapshot. Add a monotonic catalogue
revision that changes when unlock knowledge or query-relevant content changes;
include it and the captured inventory revision in every list result. For a
station query, validate the authenticated station once against its supplied
station revision and echo that revision; hand crafting uses the explicit hand
station ID and no station revision. If any captured revision cannot be held
stable for the query, return a stale-state error rather than mixing rows from
different states.

Libft unit tests own the generic snapshot matcher: equivalent live/snapshot
answers, recipe groups with shared alternatives, rarity filters, malformed
snapshot rejection, unchanged inventory/snapshot, and deterministic missing
group ordering. Minecraft validators own the cross-system contract: a full
catalog result has one inventory revision and one catalogue revision, station
authorization is checked, and a concurrent inventory/recipe update cannot
produce a mixed-revision list. This snapshot overload is a missing Libft API;
the existing `game_inventory_snapshot` alone does not make the current
`game_crafting` query snapshot-consistent.

Minecraft already exposes the core backend routes on `GameSession`:
`query_hand_recipe` / `query_station_recipe` for read-only availability,
`craft_hand_recipe` / `craft_station_recipe` for journalled craft actions,
`grant_recipe_discovery` for a trusted source event,
`research_card_recipe` for the Card Press research action, and
`start_recipe_processing` for timed jobs. These methods carry request IDs and
inventory/station revisions; station calls also carry station identity and
coordinates for host validation. `MinecraftRecipeCatalog::query_recipes`
already produces a combined catalogue result, but only code with catalog and
inventory access can call it; `GameSession` exposes neither that list query
nor a public immutable inventory-view query for a future caller. The existing
single-recipe `GameSession` read calls take no explicit actor ID and do not
return the inventory/catalogue revision used for the answer. Station query
does recheck the configured station authorizer and revision; hand query only
checks that the session is active. Add actor-authorized, value-returning
service calls at the host boundary; do not expose the character's mutable
inventory or `MinecraftRecipeCatalog` internals to a client.

Required future caller sequence (backend contract only):

1. Proposed `GameSession::query_inventory_view(authenticated_actor_id,
   output)` returns a value snapshot with stable item-definition IDs,
   quantities, physical instance IDs/modifiers where applicable, equipped
   slots, and capacity/weight totals. It is actor-scoped and never returns
   mutable `game_item` handles. Because Libft's `game_inventory_snapshot`
   covers inventory containers but not character equipment, the host view must
   combine both under the simulation-owner boundary and include both
   `inventory_revision` and a `character_state_revision`; do not imply one
   inventory revision versions equipment unless the host explicitly guarantees
   that invariant. `authenticated_actor_id` is supplied by trusted server
   authentication, not accepted as an untrusted client identity claim.
2. Proposed `GameSession::query_recipe_catalog(authenticated_actor_id,
   known_filter, optional_station_ref, output)` wraps the existing catalog
   aggregation and returns stable recipe IDs plus known/locked,
   station-available, craftable, missing ingredient groups, and the inventory,
   catalogue, character, and (when applicable) station revisions used for the
   answer. A station argument is an authenticated server-side reference; the
   client does not assert that a station exists or is in reach. The filter and
   destination-world information policy must not reveal hidden recipe IDs or
   ingredients when that world forbids such disclosure.
3. The caller submits `craft_hand_recipe` or `craft_station_recipe` with a
   nonzero request ID, recipe ID, positive batch count, the exact inventory
   revision from its view/query, and (for stations) station ID, coordinates
   and station revision. An authenticated actor identity must be bound by the
   multiplayer service before invoking the mutation; the current GameSession
   craft methods do not take an actor ID and are not, by themselves, a
   multiplayer authorization boundary. The host rechecks authorization,
   recipe knowledge, ingredients, output capacity and station state under the
   same transaction.
4. The result identifies the request, whether it was replayed, committed
   inventory revision, produced definition IDs/quantities and physical IDs for
   unique outputs, or a structured rejection reason. An exact retry returns
   the original result; changed payload under the same request ID is rejected.
   A stale inventory/station revision requires a fresh query, not a blind retry
   with new inputs under the old request ID.

The current `MinecraftCraftActionResult` does not include the committed output
definition/quantity delta or newly minted physical instance IDs, so the result
contract above requires an additive versioned result/API change. Do not recover
the output by diffing a later inventory query: another action could intervene,
and unique output IDs are part of the exact replay result.

The current `query_hand_recipe` / `query_station_recipe` only return the
Libft aggregate `game_crafting_query_result` and missing groups; that result
does not include the inventory revision or complete item view. The current
`MinecraftRecipeCatalog::query_recipes` output entry contains recipe ID,
`known`, and `game_crafting_query_result`; its separate missing-group list is
keyed by recipe ID. Neither output carries the inventory revision or a
catalogue revision. `GameSession` craft results carry the request ID and
committed inventory revision, but the read API cannot currently tell a caller
which revision it queried. The existing catalog loop queries recipes one at a
time against the live inventory rather than an explicitly captured immutable
snapshot; the future service must capture one inventory value snapshot first
so every recipe row describes the same revision. Extend result/version
contracts before a UI or network layer relies on them.

#### Revision tuple and stale-read behavior

Treat revisions as equality tokens scoped to one world/session epoch; never
compare a revision from one character, world, catalogue instance, or restored
session with a token from another. The query result must return the complete
tuple used to build it:

```text
world/session epoch
inventory revision
character-state revision
recipe-catalogue revision
station identity and station revision, when station-bound
```

`inventory_revision` versions container contents and accounting only.
`character_state_revision` separately versions equipment and any other
character-owned query input. `recipe_catalogue_revision` changes whenever
knowledge or recipe data affecting availability/deficits changes; immutable
content must also be bound to a versioned catalogue/manifest identity so a
restart or content update cannot produce an ABA match. The station revision
comes from the authoritative station/world provider and is not supplied as
truth by the requester. Reserve zero as invalid and fail closed rather than
wrapping any revision; a new session/world epoch is required when a revision
space is exhausted or replaced.

The preferred implementation serializes the entire query on GameSession's
simulation owner and evaluates one captured inventory value snapshot plus one
stable recipe-book/catalogue view. If a future caller executes the read across
threads, it must capture immutable views and verify their epochs/revisions are
still the ones reported before publishing the result. If any participating
revision changes or station authorization becomes stale, discard all staged
rows and return a stale-state/permission result with caller outputs unchanged;
do not return a partially mixed list or silently retry against newer state.
The caller then issues a fresh query. Mutations always revalidate every token
and authorization at commit time, because a coherent query can become stale
immediately after it returns.

Default unlocks must include planks, sticks, workbench and all necessary basic gathering/processing recipes to avoid progression softlocks. Proposed discoverable unlocks: rare card recipes, advanced gear, special inks and stronger processing tiers. Discovery sources are loot blueprints, exploration achievements or research; **which source unlocks each stronger item is a balance decision still open**. The implementation must support all of them through a data-driven source grant, not hard-code a final progression choice.

Persist recipe book with character/world binding policy. Portable characters carry allowed unlocks, but world-disabled recipes remain unavailable. Migration maps stable IDs; unknown newer recipes are retained or quarantined, not silently converted into another item. Research synthesis is a distinct uncertain action with bounded deterministic pool and recorded RNG; exact known recipes never randomly yield another card.

For every persisted recipe unlock, store `{recipe_id, catalogue_version,
scope, source_event_id}` or an equivalent versioned record. The recipe ID alone
cannot distinguish a world discovery from an account-portable research unlock.
Use a migration rule for older recipe-book snapshots that lack scope, and keep
that rule consistent with the character admission policy; do not default old
records to globally portable. Joining a world filters the character-owned
unlock projection against destination policy without deleting source-world
knowledge. A denied recipe may remain visible as unavailable only if the
destination's information policy permits revealing it.

#### Source-audited recipe provenance gap

The current Libft `game_recipe_book` format is v1 and each unlock record stores
only `{recipe_id, source_event_id}`. It has no catalogue version, character or
world owner, or portability discriminator. Unknown recipe IDs are retained in
quarantine, but that does not supply the missing provenance. Minecraft's
`MinecraftRecipeCatalog` persists the exported Libft book and its discovery
source registry maps `(source_type_id, source_id)` to a recipe; neither records
whether the grant belongs to a character, a particular world, or both. The
configured grant authorizer and `RecipeDiscoveryTransaction` protect the
current grant path and roll back the in-memory book if the session snapshot
write fails, but they do not make an unlock portable/world-bound or provide a
durable cross-store commit with the event that caused the discovery.

The standalone discovery path has useful guarantees that the design must
preserve: `GameSession::grant_recipe_discovery` authorizes the supplied source
event, `RecipeDiscoveryTransaction` snapshots the old book, appends the unlock,
and invokes the GameSession snapshot writer when a new event receipt is
recorded. The prior omission for a distinct event that grants an already-known
recipe is fixed in the current worktree: the result distinguishes
`event_newly_recorded` from `recipe_newly_known`, and the transaction saves when
the former is true. An exact duplicate receipt is a no-op and does not trigger
another save; save failure restores the prior book and clears both output
flags.

This does not yet retain the original result payload for the event. On retry,
the current two-boolean API reports both flags false, even if the first
execution made the recipe newly known. Therefore it is an idempotent state
transition, not a durable replay of the original semantic response. Any caller
that must reproduce the original response after a timeout or restart must
persist it in the enclosing action's result journal and return it from there.
This is especially important when discovery is one part of another action. If
opening a chest,
claiming loot, consuming a research item, or completing a card effect both
changes another store and grants a recipe, both mutations and their replay
result must share one prepared/committed journal record and one recovery
boundary. Otherwise a crash or retry can retain the unlock without its cost,
duplicate the reward, or lose the unlock after the source event was consumed.
The stable idempotency key must be namespaced across source types and owners;
Libft currently deduplicates `source_event_id` alone inside a recipe book, so
Minecraft must issue collision-free IDs or encode the full source namespace
into the event identity before calling the generic API.

The source-event receipt also needs an explicit retention/compaction contract.
It cannot be evicted merely because the recipe is known: old requests may be
replayed after save restore or delayed multiplayer delivery. Either retain
receipts for the lifetime of the owning character/world, or use a monotonic
per-owner sequence with a durable high-water mark plus a bounded exact-result
window for requests that may still be retried. Any bounded policy must define
the maximum retry horizon and reject expired requests without reapplying their
effects; it must not silently treat an expired ID as a fresh discovery.

Keep ownership semantics in Minecraft rather than making the reusable Libft
book depend on Minecraft identities. Add a versioned Minecraft provenance
section adjacent to the recipe-book snapshot. Each canonical grant must carry
at least `{recipe_id, catalogue_version, scope, owner_id, source_type_id,
source_id, source_event_id}`; `scope` must distinguish character-portable from
world-bound grants, and `owner_id` must identify the character or world
accordingly. If one discovery intentionally grants both scopes, persist two
explicit grants rather than infer one from the other. Commit the generic book
unlock and the provenance record through the same idempotent discovery
transaction/checkpoint. Import, replay, or direct book mutation must not expose
a recipe while its required provenance is absent. Recipe queries should derive
an effective known/unavailable projection from canonical grants plus the
destination world's admission policy; filtering a grant for one destination
must never delete its source-owned record.

Migration must fail closed. New-world bootstrap recipes are granted from the
destination's current defaults, not treated as migrated portable discoveries.
For a legacy v1 book, bind each non-default unlock to the source world only
when the containing world's identity is known. If it is unavailable, quarantine
the grant and keep it unusable until an explicit migration/resolution step;
never infer global portability from recipe ID, source event ID, or the fact
that the recipe is currently enabled. Preserve the original v1 bytes or a
recoverable migration record until the new provenance snapshot is durably
committed, and make migration idempotent across retries and backup restore.
The exact treatment of pre-existing bootstrap/default grants must be explicit
in the migration version so a legacy save does not accidentally turn defaults
into portable progress.

Acceptance tests must cover character-portable and world-bound unlocks,
source/destination IDs, destination policy filtering without source-state
mutation, identical and conflicting retries, crash/recovery between book and
provenance publication, catalogue-version changes, unknown-ID quarantine,
legacy v1 migration with and without a known source-world identity, repeated
migration, and restore from a pre-migration backup. Libft tests should remain
limited to the generic stable-ID, source-event idempotency, checksum and
unknown-ID quarantine contract, including a distinct event for an already-known
recipe being durably recorded (`event_newly_recorded=true`,
`recipe_newly_known=false`) and an exact retry being a no-op; scope, identity
and migration policy belong in Minecraft validators. Minecraft validators
must separately cover the
standalone-authorized grant/save/retry path and at least one loot/research
action that grants a second discovery event for an already-known recipe and
verifies that the new receipt survives save/restart, as well as one action that
changes a second durable store. Inject failures before prepare,
after prepare, between in-memory mutations, before snapshot replacement, and
after replacement but before the caller observes success. Every recovered or
retried case must match exactly one committed source event, one unlock record,
one cost/reward outcome, and the same reported result.

## 10. Complete item and acquisition coverage

The roadmap's item catalogue is the required content scope. At implementation, enumerate the canonical Voxel registry: every built-in/runtime block must have exactly one explicit acquisition/drop policy. Do not freeze today's registry count into the validator.

- All non-air/non-fluid built-ins get placeable self-drop block items by default, including stone, ores, decorative plants, logs/leaves, snow/ice, sandstone and crystal/mineral blocks.
- Grass drops dirt; air drops nothing; water/lava use containers. Bedrock has an admin/creative item mapping but is not ordinarily breakable. New technical blocks may explicitly have no item.
- Add blocks/items for planks where applicable, workbench, furnace, Paper Mill, Card Press, World Loom, chest/storage, farmland, wheat and paper plants, including configured crop-state/drop mappings.
- Processed resources: sticks, coal, iron/gold/copper bars, diamond/emerald products if their extraction recipes are enabled, plant fiber/binder, paper reed fiber, paper sheets, card stock, pigments, dark/colored/advanced ink and magical dust.
- Hostile-mob loot: rotten flesh, bones, gunpowder, string and spider eyes, each tied to a named deterministic loot table and entity/death event. These are world-loot acquisitions, not recipe outputs; define chance/count ranges, eligibility, and replay-safe delivery in Minecraft.
- Containers/vehicle: empty/water/lava buckets and boat; filled buckets stack to one and have validated fluid state.
- Food/farm: wheat, seeds, bread, apple, pear, raw/cooked beef/pork/chicken/mutton; animal materials wool, leather, feathers and eggs.
- Gear: wooden hoe; configured wood/stone/iron/gold/copper/diamond pickaxes, axes and swords; bow, arrows, shield, staff; cloth/leather/iron armor for head/chest/legs/feet. Material variants are explicit enabled definitions, not assumed combat balance. The hoe is also the required tool for the revision-checked farmland tilling action.
- Cards: all seven starter definitions and twelve current non-starter, discovery-locked definitions (IDs 11–22, including Fortune's Draw); Biome Guarantee remains future and disabled. Each crafted output is a unique physical card.

Every item must declare exactly one primary acquisition policy: exact recipe, world/block acquisition, mob/harvest acquisition, loot/discovery, or restricted admin/technical. Not every obtainable block needs an artificial crafting recipe. No enabled craftable item may lack a recipe. This exhaustive policy is what makes “all items and recipes” testable without pretending bedrock or apples must be crafted.

Source audit note (2026-10-22): the checked-out Minecraft item catalogue already
registers the five hostile-loot items above and a recipe-acquired wooden hoe;
the prior prose list omitted them. `MinecraftLootCatalog` owns their loot
tables, while `MinecraftRecipeCatalog` registers the hoe recipe as ID 3131.
Keep this inventory list synchronized with those registries, but treat the
registry-driven coverage validator—not this snapshot's item count—as the
completeness check.

## 11. Placeholder ordinary recipes

These are proposed versioned Minecraft **content data**, not Libft constants. `A × n` denotes n units. Every enabled wood variant expands the log/plank/station/boat family consistently; mixed-wood eligibility is a declared tag rule. Yields are per action. Fuel and processing durations are configured separately.

| Output | Inputs | Station / initial unlock |
|---|---|---|
| Planks × 4 | Log × 1 | Hand / default |
| Sticks × 4 | Planks × 2 | Hand / default |
| Workbench × 1 | Planks × 4 | Hand / default |
| Furnace × 1 | Stone × 8 | Workbench / default |
| Chest × 1 | Planks × 8 | Workbench / default |
| Ladder × 3 | Sticks × 7 | Workbench / default |
| Trapdoor × 2 | Planks × 6 | Workbench / default |
| Sandstone × 1 | Sand × 4 | Hand / default |
| Boat × 1 | Planks × 5 | Workbench / default |
| Wooden hoe × 1 | Planks × 2 + sticks × 2 | Workbench / default; required for farmland tilling |
| Empty bucket × 1 | Iron bars × 3 | Workbench / default once material available |
| Water/lava bucket × 1 | Empty bucket × 1 + valid source fluid | World transfer, not a crafting recipe; returns empty bucket when emptied |
| Paper Mill × 1 | Planks × 6 + stone × 2 | Workbench / default |
| Binder × 2 | Paper reed fiber × 2 | Paper Mill / default |
| Paper sheets × 3 | Paper reed fiber × 3 + water unit × 1 | Paper Mill / default; return container atomically |
| Card stock × 3 | Paper sheets × 3 + binder × 1 | Paper Mill / default |
| Dark ink × 2 | Brown mushroom × 1 + binder × 1 + water unit × 1 | Paper Mill / default |
| Colored ink × 2 | Red/yellow flower pigment × 1 + binder × 1 + water unit × 1 | Paper Mill / default |
| Pigment × 2 | Appropriate flower × 1 | Hand / default |
| Advanced ink × 2 | Quartz/amethyst/amber sample × 1 + binder × 1 + water unit × 1 | Paper Mill / unlock |
| Card Press × 1 | Planks × 4 + stone × 4 + binder × 2 | Workbench / default; no metal-only bootstrap lock |
| World Loom × 1 | Planks × 6 + card stock × 2 + stone × 2 | Workbench / default |
| Iron/gold/copper bar × 1 | Corresponding ore-block item × 1 + configured fuel | Furnace / default |
| Diamond/emerald material × 1 | Corresponding ore-block item × 1 | Workbench extraction / unlock; preserve the roadmap's ore-block drop rule |
| Cooked beef/pork/chicken/mutton × 1 | Matching raw meat × 1 + configured fuel | Furnace / default |
| Bread × 1 | Wheat × 3 | Hand / default |
| Magical dust × 2 | Configured crystal sample × 1 | Workbench / unlock/configured regeneration introduction |
| Pickaxe × 1, each enabled tier | Tier material × 3 + sticks × 2 | Workbench / basic default, stronger unlock |
| Axe × 1, each enabled tier | Tier material × 3 + sticks × 2 | Workbench / basic default, stronger unlock |
| Sword × 1, each enabled tier | Tier material × 2 + stick × 1 | Workbench / basic default, stronger unlock |
| Bow × 1 | Sticks × 3 + plant fiber × 3 | Workbench / default |
| Arrows × 4 | Gravel × 1 + stick × 1 + feather × 1 | Workbench / default; gravel is a placeholder until flint exists |
| Shield × 1 | Planks × 6 + iron bar × 1 | Workbench / unlock |
| Staff × 1 | Sticks × 3 + amethyst sample × 1 | Workbench / unlock |
| Armor head/chest/legs/feet × 1 | Selected cloth fiber/leather/iron material × 5/8/7/4 respectively | Workbench / cloth default, stronger unlock |

Current-catalog comparison (Minecraft `agent/analytics-performance`,
`c422dcc43bd758394637555d300e67cd17380f9b`; source inspection only): the table
above remains the proposed content target, not a verbatim dump of current
recipe definitions. The existing `MinecraftRecipeCatalog` already registers
the output families and stable IDs listed in Section 12.1, with these details
important for reconciliation:

| Area | Current source behavior | Handoff requirement |
|---|---|---|
| Wood recipes | Oak, pine, and birch planks have separate recipes; boat variants use recipe IDs 1011, 1024, and 1025, all producing the boat item. | Keep variant input eligibility explicit; do not assume one recipe accepts mixed wood unless the registered tag policy does. |
| Water-container processing | Paper sheets, dark ink, colored ink, and advanced ink consume a water bucket and return an empty bucket as a byproduct. | Treat input consumption, byproduct capacity, and output placement as one craft transaction. Preserve this contract in any recipe rebalance. |
| Smelting/cooking | IDs 1029–1035 are furnace recipes. Per-recipe fuel item, fuel quantity, and duration are host-configurable; defaults are coal × 1 and 200 server ticks. | Keep fuel/duration versioned policy, reserve input and fuel once, and test save/load plus full-output retry. |
| Equipment progression | Wood and stone tool tiers are initially unlocked; iron, gold, copper, and diamond tool tiers are locked. Cloth and leather armor are initially unlocked; iron armor is locked. | Treat these as current defaults only. Final unlock sources and balance remain open and must be data-driven. |
| Locked material recipes | Advanced ink, magical dust, diamond extraction, emerald extraction, and the stronger equipment families are registered locked by default. | Knowledge grants must be explicit, provenance-bearing, and idempotent; do not unlock these merely because ingredients are present. |

The source catalogue's exact current ingredients remain authoritative for the
current executable; the proposals above are the design target for a reviewed,
versioned content update. Before changing either side, update the stable-ID
mapping, unlock migration, and content validators together. The focused design
does not claim that current recipe costs have been balance-tested.

Logs/leaves/terrain, fruits, seeds, raw meat, wool, leather, feathers and eggs use harvesting/drops rather than fictional manufacturing recipes. Crystal samples can initially be their registered block items; do not duplicate IDs merely to invent a catalyst. Farmland is a validated world/tool transformation, not an item duplication recipe. Future health/focus consumables, repair, glass, bricks and storage blocks require explicit definitions/recipes when enabled, and are not silently claimed complete here.

## 12. Card recipes and discovery

All exact card recipes use Card Press, output one unique physical card item,
and use stable recipe IDs linked to (but distinct from) card definition IDs.
They never directly create a second deck instance. The current catalog uses a
common substrate of card stock × 1, either dark or colored ink × 1, and binder
× 1 for starter cards. Its rare substrate is card stock × 2, advanced ink × 1,
and quartz × 2. The row catalysts below are additional inputs. Recipe
knowledge and ingredient acquisition are distinct gates.

The table records the current source-audited Minecraft catalog, not a frozen
balance decision. Recipe inputs, station requirements, and unlock policy are
versioned content and may be rebalanced only through an explicit catalog
migration; IDs must not be repurposed. The current costs are placeholders for
playtesting. Proposed alternatives must be recorded as a future catalog
version rather than described as if already implemented.

| Recipe ID | Card | Current additional catalyst(s) | Current knowledge |
|---|---|---|---|
| 2001 | Biome Exclusion | One sand, dirt, or snow block item | Basic/default known |
| 2002 | Deep Seam | One coal, iron, gold, diamond, emerald, or copper ore sample | Basic/default known; ore target at play is a separate typed choice |
| 2003 | Village Charter | Wheat × 1 + oak planks × 2 | Basic/default known |
| 2004 | Surface Magma | One volcanic rock or stone block item | Basic/default known |
| 2005 | Double Draft | Paper sheets × 2 | Basic/default known |
| 2006 | Biome Compass | One red or yellow flower item | Basic/default known |
| 2007 | Surveyor's Reach | Stone block item × 2 | Basic/default known |
| 2008 | Volcano Mark | Volcanic rock block item × 4 + obsidian block item × 2 | Rare; locked by default |
| 2009 | Ultimate Greed | Gold bars × 2 + amber block item × 1 | Rare; locked by default |
| 2010 | Biome Rupture | Sand block item × 1 + snow block item × 1 | Rare; locked by default; effect choices are validated separately at play |
| 2011 | Ore Overgrowth | Any two ore-sample items + emerald material × 1 | Rare; locked by default; ore target at play is a separate typed choice |
| 2012 | Shipwreck Chart | Oak planks × 2 + copper bar × 1 + paper reed fiber × 1 | Rare; locked by default |
| 2013 | Ruined Village Relic | Cracked stone block item × 2 + iron bar × 1 | Rare; locked by default |
| 2014 | Unstable Catalyst | Shimmer stone block item × 2 + amethyst sample × 1 | Rare; locked by default; hidden effect selection is resolved only on accepted play |
| 2015 | Desert Pyramid Seal | Sandstone × 4 + gold bar × 1 | Rare; locked by default |
| 2016 | Ancient Canopy Seal | Oak log × 4 + ladder × 1 | Rare; locked by default |
| 2017 | Spider Nest Seal | Plant fiber × 4 + brown mushroom × 1 | Rare; locked by default |
| 2018 | Volatile Stabilizer | Amber block item × 1 | Rare; locked by default; overload count belongs to its specified regeneration scope |
| 2019 | Fortune's Draw | Paper sheet × 1 | Rare; locked by default; one-card draw follows configured policy |
| Unassigned | Biome Guarantee (later) | Selected eligible biome sample × 2 | Later unlock, not enabled until effect/recipe exists; allocate only when versioned and shipped |

The current catalog registers these recipes at the Card Press; its seven
starter recipes are initially known and later card recipes are registered
locked. Keep exact unlock-source policy separate from the recipe ingredient
list. Crafted cards cannot be consumed as their own unlock prerequisite.
Optional research has a separate API/action, exact ingredient reservation,
server RNG, finite candidate pool and guaranteed persisted result. Retrying or
saving/reloading does not reroll. Magical dust is not a card-recipe ingredient
in the current catalog; it remains a regeneration resource unless a reviewed
content version explicitly changes that.

### 12.1 Stable recipe-ID map and migration rule

The following map is source-audited against the current Minecraft catalog. IDs
are persisted/request-visible content identity, not array positions. Once an
ID has appeared in a save, unlock event, network request, or replay, never
reassign it to a different recipe; retire it as a tombstone and allocate a new
ID for changed semantics. Catalogue migrations preserve recipe IDs when the
meaning is unchanged and explicitly migrate unlocks when the meaning or output
changes.

| ID(s) | Current recipe output / family |
|---|---|
| 1001–1003 | Oak, pine, birch planks respectively |
| 1004–1010 | Sticks, workbench, furnace, chest, ladder, trapdoor, sandstone respectively |
| 1011, 1024, 1025 | Oak, pine, birch boats respectively |
| 1012–1018 | Empty bucket, Paper Mill, binder, paper sheets, card stock, dark ink, colored ink respectively |
| 1021–1023 | Card Press, World Loom, magical dust respectively |
| 1026–1028 | Advanced ink, diamond material extraction, emerald material extraction respectively |
| 1029–1035 | Iron/gold/copper smelting, then beef/pork/chicken/mutton cooking respectively |
| 2001–2019 | Card recipes in the exact order listed in the card table above |
| 3001–3005 | Bread, bow, arrows, shield, staff respectively |
| 3101–3106 | Wood/stone/iron/gold/copper/diamond pickaxes |
| 3111–3116 | Wood/stone/iron/gold/copper/diamond axes |
| 3121–3126 | Wood/stone/iron/gold/copper/diamond swords |
| 3131 | Wooden hoe |
| 3201–3204 | Cloth head/chest/legs/feet armor |
| 3211–3214 | Leather head/chest/legs/feet armor |
| 3221–3224 | Iron head/chest/legs/feet armor |

Unlisted numeric gaps are not implicit aliases for new recipes; check the
versioned registry and migration ledger before allocating them. A card
definition ID and its recipe ID are distinct namespaces and must never be
conflated.

## 13. Required tests: Libft

Add focused files under `Test/Test`, following `AGENTS.md` naming; extend existing CardGame engine/simulation/snapshot tests, Game inventory/crafting/catalogue tests and Scripting tests. All Minecraft names/content in library tests are replaced with synthetic domains.

| Suite | Required assertions |
|---|---|
| Action sequencing | Invalid actor/phase, stale revision, duplicate request, different payload same ID, overflow IDs, unavailable action; replay yields identical state/output/RNG |
| Card identity/zones | Instance exists in exactly one zone; transactional transfer; failed move retains source; four-copy policy across all deck zones; boundary sizes and invalid policies |
| Draw policy | Full-hand burn, partial multi-draw, empty pile, legal reshuffle, no eligible cards, empty+overflow costs exactly once, no infinite refill loop |
| Obligations/choices | Draw-two-discard-one, multi-option validation, duplicate discard, foreign instance, zero hand, timeout default, save/load pending resolution, action blocked until complete |
| Effect transactions | Callback/script fail after emitted output, allocation failure at every preparation point, host rejection, rollback including RNG/outputs; producer-pending/outbox snapshot restore, ordered consumer deduplication, prepare/commit/ack retries, payload-conflict rejection and fake-host crash recovery at every protocol boundary |
| Generic schemas | Unknown intent/version, bad quantities, arithmetic overflow, output/node/byte caps, callback reentrancy, no raw-pointer wire payload |
| Script bridge | Native and scripted equivalent effects; stable bindings, wrong manifest, invalid bytecode, instruction/native/output caps, diagnostics, no world/I/O access, concurrent isolated owners |
| Persistence | Snapshot/delta round trips, unknown versions/IDs, corrupted lengths/checksum, rules mismatch, pending choices, burns, use counters and idempotency restored |
| Inventory | Stack compatibility, split/merge, weight/capacity, durable/unique items, partial pickup plan, concurrent revision conflicts, multi-container rollback, forged instance rejects |
| Crafting | Missing/unlocked recipes, station availability and mismatch, legacy station bypass rejection, duplicate ingredient overlap, tagged alternatives, output full, returned bucket capacity, invalid output catalogue, batch overflow, ingredient/result allocation failures |
| Recipe book | Default grants, idempotent discovery, sparse stable-ID persistence/migration, unknown IDs, query does not mutate, no recipe knowledge spoof |
| Timed processing | Fuel/input once, paused/full output, save/load/unload, capped tick catch-up, completion retry no duplicate result |

Use the existing failure-injection conventions: allocation limit zero disables limits, not forces failure. Test all lifecycle initialization/destroy/move failures and Linux/Windows/macOS serialization compatibility. Run production dependency/build checks so a test-only binding does not leak into archives.

## 14. Required tests: Minecraft

Add deterministic card, inventory, recipe and regeneration validators alongside existing validators; wire accepted validators into `make validate-all` and the platform-equivalent runner.

- Catalogue exhaustiveness: every registered block has one drop policy; every enabled craftable item has a valid recipe; every enabled card has an effect, item, recipe, knowledge policy and preview description. Starter counts total 20 and no definition exceeds four.
- Card intent compatibility: assert the policy/empty-draw/overflow type IDs, version 2 field counts and scalar types, definition-to-effect-kind mapping, source-instance/request/sequence identity, and rejection of unknown versions or a mismatched effect kind. Round-trip the payload through adapter snapshots and replay. Verify Volatile Stabilizer and Unstable Catalyst private results are hidden from every client-facing query, preview, save export and diagnostic while remaining available to authoritative server recovery.
- Future inventory/recipe read APIs: immutable inventory view reports one consistent revision and value-only item data; combined recipe query reports knowledge, station availability and ingredient deficits against stated inventory/catalogue/station revisions. Verify unauthorized actor redaction, stale query rejection/requery, exact craft retry, changed-payload rejection, and that read-only queries never mutate inventory, recipe knowledge or action caches.
- One table-driven test per card verifies stable definition-ID mapping, legal targets, ordered effect, caps, no-op display, instance consumption, rare sourcing, instability output and clean base rules next session. Biome choice tests must prove config-local indices are rejected under a changed manifest and never silently remapped to another biome.
- Sequential composition: remove biome then compass/guarantee; Deep Seam then doubling; multiple villages; final instability applied consistently to feature loot/spawners. Test incompatible ocean/desert/forest/cave footprints and unsupported required feature failure before spending.
- Unstable Catalyst secrecy/permanence, pool coverage, reload/retry consistency; Volatile Stabilizer escalating curve/session counter, deterministic overload and irreversible outcome; kiss/curse reward opportunities and caps.
- Multiplayer occupied chunk plus eight-neighbor exclusion, simultaneous reservations, glowing-wall movement/teleport/boat paths, disconnect/restart, five-minute timeout, no voluntary cancellation, resource journal recovery.
- World Loom lifecycle state machine: cancel an owned draft before commit with no debit; commit exact retry spends/deals/installs barriers once; reject cancel, selection edits and changed request payload after commit even during the decision window; restart preserves the same deadline and committed session; only the authoritative clock expires choices; generation/retry retains the committed rules and barriers until safe publication. Verify the solo/multiplayer deadline policy explicitly.
- Durable CardGame-to-world intent transaction: persist the accepted action and complete outbox before host mutation; bind world/session/actor/request/action/intent identity to the canonical payload; inject restart/failure at producer-save, host-prepare, each participant mutation, host-commit, producer-ack and response boundaries. Verify at-least-once delivery produces one effective mutation, an exact replay returns the stored result, conflicting payload reuse fails closed, intent order is preserved, and unresolved prior effects block dependent card actions. Include both rule/instability/feature-plan changes and inventory/resource changes; test asynchronous chunk publication separately so retries never charge or consume the card twice.
- World Loom start APIs: select/unselect exact coordinates with revision/idempotency checks; cost query is non-mutating; opening counts 4–7 cost `B4, 2B4, 4B4, 8B4`; overflow/insufficient dust/stale selection/occupied chunk leaves dust, reservations, barriers and CardGame hand unchanged; retry never charges or deals twice; committed start freezes selection and cannot be cancelled.
- Opening-deal accounting: a fresh 20-card starter deck yields hand/draw-pile counts of 4/16 by default and `n/(20-n)` for every allowed opening size `n=4..7`; physical instance IDs remain unique and total owned cards remain 20. Imported-card cases add only their actual imported count and still enforce deck/copy caps.
- Per-chunk instability: persist/reload the full world policy plus WCI1 records; zero default and selected nonzero challenge default; pre-v9 migration; policy-hash mismatch fails closed; untouched chunks remain unchanged; reward unlock monotonicity versus active danger-tier reduction after stabilization; multi-chunk feature atomicity, capacity/stale-record rejection, chunk+record crash recovery, and stale generation rejection.
- Each ordinary/card recipe expands into a content-driven craft test with expected costs/yields/byproducts. Test inventory capacity failures preserve ingredients and unlock state. Book default workbench path, rare discovery and portable/world-bound character rules.
- Physical card transfer: crafting creates one unique physical item; import removes that exact inventory instance and adds one matching session instance; export restores the same physical ID and is rejected if it would reduce the deck below 20. Cover copy/deck caps, stale inventory revisions, changed-payload request reuse, exact retry, crash after prepared checkpoint, crash after mutation before terminal checkpoint, recovery, failed final save rollback, and duplicate-ID detection across inventory/equipment/ground/entity/session-card stores.
- World integration: station reach/claims/destruction race, grass-to-dirt drops, ore-block extraction, fluid-container transfers, crop/fruit/animal acquisition, boat item/entity transfer and save/reload exactly once.
- Character/world admission: portable versus world-bound physical card/deck instances, ordinary inventory/equipment, and recipe-unlock provenance; policy filtering without deleting source-world state; destination policy changes; missing-scope legacy migration; simultaneous-join leases; exact join retries; crashes between prepare/commit/save; backup/restore; and proof that no physical ID or active World Loom session is duplicated or imported.
- Performance: matched idle/play/draw/discard/research/crafting/timeout workloads, maximum legal deck and output limits, continuous block breaking during regeneration, inventory saves and chunk publication. Report frame mean/p95/p99, action latency, script operations, bytes/snapshot, generation/commit time, queue depth and allocations. No render-thread unbounded loop.

Libft tests prove mechanics; Minecraft tests prove named content and world rules. Do not duplicate a hard-coded Minecraft recipe/biome catalogue in Libft just to run library tests. Golden replay fixtures use versioned definitions and seed, not unspecified RNG outputs.

## 15. Implementation order and roadmap tracking

| Order | Deliverable | Depends on | Acceptance / roadmap update |
|---|---|---|---|
| 0 | Baseline API/limit audit, build/tests, dependency and save-version plan | Nothing | Record actual existing passing tests; no new feature marked done |
| 1 | Inventory prepare/commit and recipe-owned transactional outputs | Baseline | Failure/duplicate/concurrency tests pass; inventory/crafting foundations integrated status only |
| 2 | Generic action envelopes, non-combat resolution, outputs and durable protocol | Baseline | Libft outbox/ordering/rollback/snapshot tests and Minecraft shared-journal crash matrix pass; exact result replay, payload-conflict rejection, and safe retention/expiry are proven before the action is reported committed |
| 3 | Draw/burn/mandatory discard/choice policies and copy limits | 2 | Full boundary matrix; update deck/hand decision mechanics status |
| 4 | Script bridge and stable manifest | 2–3 | Native/script equivalence, bounded failures, platform replay tests |
| 5 | Recipe book/catalogue extensions and timed processing | 1 | Unlock, output/byproduct, snapshot-query consistency, save/load/fuel tests; verify all catalog rows bind to the same inventory/catalogue revisions |
| 6 | Minecraft item/drop/recipe catalogue and starter card host | 1–5 | Full registry coverage, all starter effects, bootstrap economy and content validators |
| 7 | Regeneration reservation/lifecycle and per-chunk persistence | 6 + existing generation | Multiplayer barriers/timeouts, base snapshot isolation and atomic publication |
| 8 | Rare cards, instability overlays, discoveries/research | 7 | Every current rare card tested, hidden/permanent outcomes and kiss/curse balance checks |
| 9 | Future-caller API/network/save integration, security and matched performance | All | Typed APIs are reachable through an authorized host boundary; normal/analytics builds and full validators pass on target platforms. UI design and implementation remain out of scope |

Independent work can run in parallel after contracts are decided: generic inventory transactions, command envelope tests and content-table documentation. Avoid concurrent workers changing the same snapshot/wire schema. Narrow worker tasks specify exact files, behavior, constraints and tests; lead reviews integrations before marking complete.

For each delivered task record: affected APIs/files, schema versions, Libft tests, Minecraft validators, platform results, performance observations, reviewed commit IDs and remaining limitations. Update both this ledger and roadmap headings in the same implementation change. A generic API is not a completed Minecraft feature; a passing unit test is not a verified multiplayer world-save pipeline.

### Open decisions before production content freeze

Instability amounts/caps and overload curve; starting draw count; recipe unlock sources and portability restrictions; fuel/duration balance; material gear tiers; guaranteed-feature infeasibility UX; catalogue migration policy; research costs/pool; exact irreversible-session boundary. Defaults above provide implementable test fixtures, not hidden permanent balance commitments. Resolve these explicitly and update both documents instead of silently changing gameplay in implementation.


Validation update (2026-10-10): active World Revision generation is now explicitly non-cancellable. The validator starts regeneration, confirms cancellation returns `FT_ERR_INVALID_OPERATION` while the generation phase is active, and then drains the pipeline to a successful committed result. The dead active-cancellation branch was removed. `make -j1 normal` succeeded on Windows MinGW and `ft_vox.exe --validate-world-revision` passed (`world-revision: ok regenerated=1 skipped=0`). This verifies the runtime boundary only; draft cancellation remains available, and resource/card/chunk multi-system commit and crash recovery remain unimplemented.


Validation update (2026-10-10): the World Revision network boundary now requires a trusted host authorization callback and rejects requests whose resolved actor or Loom does not own the active draft. The validator verifies server initialization fails closed without an authorizer, an authorized actor can open a draft, and another connection cannot mutate it. `make -j1 normal` and `ft_vox.exe --validate-world-revision` pass. This is a host integration boundary, not a complete authentication provider; request idempotency, per-session coordinators, occupancy checks and committed start remain open.


Validation update (2026-10-10): `WorldRevisionServer` now requires a nonzero decimal `request_id` on each authorized mutation, bounds messages to 4 KiB, and retains up to 4,096 actor/Loom-scoped request results in memory. Exact payload retries return the original status; a reused ID with different bytes is rejected; cache capacity and allocation are checked before action execution. The World Revision validator covers exact retry, changed-payload rejection, foreign-actor draft mutation, and fail-closed server initialization. Windows `make -j1 normal` and `ft_vox.exe --validate-world-revision` pass. The cache is process-local and is cleared on server destruction; durable retry recovery remains open.


Validation update (2026-10-10): the production `game_inventory` API no longer exposes mutable map access. The test-only accessor remains under `LIBFT_TEST_BUILD` for fixture compatibility. New `clear_items()` performs a locked clear, resets slot/weight accounting and next-slot state, and advances the revision before publishing the change; save loading and world reset now use this API. The isolated inventory test executable passed 13/13 tests, including the new revision assertion. The all-tests binary failed to link because the current full graph has unrelated unresolved GPU/render platform symbols (`ft_gpu_window::create` and `ft_render_platform_*`); inventory objects and tests compiled successfully. The standalone helper expects a removed Make target, so the inventory test was linked from its compiled object and run at the standard test path.


Validation update (2026-10-10): `game_inventory::transfer_to` now has a transaction-ID overload with a bounded 4,096-entry source-local replay cache. It binds each ID to the destination, slot, quantity and expected revisions; exact retries return the original success or failure result, while changed parameters are rejected. The legacy overload remains source-compatible and does not promise retry idempotency. The focused transaction file passed 11/11 tests after direct linking; `make -j4 run-tests` compiled the inventory implementation/tests but the repository-wide test link still fails on unresolved GPU/render platform symbols. The cache is volatile, cannot support crash recovery, and returns `FT_ERR_FULL` once exhausted; the regression suite verifies that the failure is reported consistently without mutation. Durable transaction journaling and a production retention/eviction policy remain required.


Validation update (2026-10-10): inventory transfer transaction IDs now support an explicit scope, so equal numeric IDs from different actors do not collide in one source inventory. `WorldItemStore::pickup` routes each exact transfer attempt through the scoped overload; its existing `(actor_id, request_id)` pickup cache remains the outer replay boundary, while a monotonic store-local ID distinguishes internal quantity attempts after `FT_ERR_FULL`. The focused inventory transaction suite passed 12/12. Libft `make -j4 normal` and Minecraft `make -j4 normal` completed successfully; `ft_vox.exe --validate-world-revision` and `--validate-item-catalog` both passed. This does not make world-item entities or pickup request results durable, and the existing pickup cache can evict old IDs; crash-safe cross-inventory/world persistence remains open.


Validation update (2026-10-10): `WorldItemStore::pickup` now records the terminal result code with its actor/request cache entry. Exact retries replay both successes and failures; retained IDs reject changed entity, revision or quantity inputs. Added the `--validate-world-items` validator for partial stack pickup, exact success replay without duplicate inventory credit, changed-payload rejection, failed-capacity replay after inventory state changes, and preservation of the world stack. Minecraft `make -j4 normal` completed and `ft_vox.exe --validate-world-items` passed (`world-items: ok partial=2 retry=stable failure=stable`). The cache remains volatile and FIFO-evicts after 4,096 entries; persistence and durable request-ID retention remain open. A version-1 value-only WorldItemStore snapshot codec now serializes entity/item state, ID counters and the retained pickup-result cache with a checksum. The world-item validator round-trips both success and failure retry results, verifies entities survive restore, and verifies checksum corruption leaves the existing store state intact. The save_file/load_file API uses atomic file replacement and bounded binary reads; the game session does not yet call it as part of a saved-world lifecycle or a cross-system journal.


Validation update (2026-10-10): WorldItemStore now exposes a bounded version-1 snapshot codec that preserves ground entity/item values, chunk indexing inputs, ID counters, and retained actor/request pickup results. Deserialization checks format/version/checksum, count and value bounds, item/chunk consistency, duplicate IDs, cache/order consistency, and success/failure result invariants before swapping staged state into an initialized store. --validate-world-items verifies restored entity state and exact successful and failed pickup retries, rejects changed-payload retries, and confirms checksum-corrupt input does not replace live state. Windows make -j4 normal and the validator pass. The validator now exercises atomic file save/load rather than only in-memory decode. Game-session persistence for chunks/card sessions, atomic coordination with inventory and block edits, durable request retention beyond the bounded cache, and cross-version catalogue migration remain open.


Session persistence update (2026-10-10): GameSession derives a SHA-256 seed-scoped path under saves/world_items and uses GameSessionPersistence to save the Libft character/inventory buffer together with the WorldItemStore snapshot in one checksummed file through atomic replacement. Startup stages and validates the entity snapshot before restoring character/inventory state; malformed or unsupported files fail startup, and legacy version-1 WISP entity-only files remain readable. The regression validator verifies character stats, inventory contents and ground entities round-trip together, then corrupts the combined file and verifies load rejection leaves the restored character unchanged. This does not persist chunks, card sessions, generation configuration, or a shared live transaction recovery journal. Save failures are reported through GameSession::error_code, while shutdown currently tears the session down after a failed save; retryable shutdown handling remains open.


Validation update (2026-10-10): `WorldItemStore::merge_stacks` now performs a revision-checked, nearby merge for ordinary stackable entities with matching item ID, max stack and owner-lock metadata. It rejects unique instances and incompatible metadata. Ground-item inventories are constrained to one slot both at spawn and restore, so inventory transfer cannot silently split one world entity into multiple internal stacks. The world-items validator covers a 6+5 merge into an 8-capacity destination, verifies the exact 2/3 quantities remain, and verifies stale entity revisions are rejected. Windows `make -j4 normal` and `ft_vox.exe --validate-world-items` pass. This is an explicit merge API; automatic merge selection from drop spawning or the session loop, durable merge recovery and rendering/network replication remain open.


Validation update (2026-10-10): Successful block mining now attempts to merge the newly created drop into a compatible nearby ground stack after the block edit commits. A failed block edit still compensates by despawning its new drop before any merge is attempted. Nearby queries now cap examined entities at 1,024 and returned snapshots at 256; overflow returns `FT_ERR_FULL` and leaves the caller output unchanged. `--validate-world-items` verifies both caps and unchanged output, alongside partial merge/revision cases. A query overflow or no compatible stack leaves the drop as its own entity. Windows `make -j4 normal`, `ft_vox.exe --validate-world-items`, `--validate-item-catalog`, `--validate-regeneration-cards`, and `--validate-world-revision` pass sequentially. The focused validator proves store merge and query bounds; there is not yet an end-to-end block-mining/drop merge test. This does not provide a durable block/drop transaction or a bound on bucket growth during item spawning.


Validation update (2026-10-10): Nearby target selection now lives in `WorldItemStore::merge_into_nearby`, the same method called after successful block mining. It verifies the source revision, searches through the bounded nearby query, filters item/stack/owner-lock compatibility, and leaves the source untouched when no target has capacity. The world-item validator now exercises this shared selection path, exact partial totals, a full-target no-op, stale revision rejection, and both query caps. `make -j4 normal` and `ft_vox.exe --validate-world-items` pass. The validator still does not execute a full world raycast/block edit, so end-to-end mining flow evidence remains open.


Validation update (2026-10-10): Ground item spawning now supports a pending state through `spawn_pending` and `publish_spawn`; publication retries with the same revision return success without duplicating the entity. Pending entities are hidden from nearby queries and pickup; snapshot serialization fails closed while a pending entity exists. Block mining stages its drop before editing the voxel, publishes it only after the edit succeeds or a post-error read confirms the block is gone, and removes the pending entity when the block remains. The world-item validator checks invisibility, pickup rejection, save refusal, idempotent publish retry without duplicate visibility, and cleanup. `--validate-world-items` also invokes the production downward-raycast mining path against a loaded chunk, verifies the block becomes air, and verifies the drop merges into the nearby stack. Windows `make -j4 normal` and `ft_vox.exe --validate-world-items` pass. The broader `--validate-block-edit` suite still exits later on its emissive-delete convergence check (timeout 59), after printing the successful mining-drop assertion. This closes the live pickup visibility window for mining drops, but it is not a durable cross-system transaction: process crashes still have no shared chunk/item journal.


Validation update (2026-10-10): Libft crafting now supports immutable recipe station assignments before unlock. Legacy query/prepare calls use station 0 and cannot execute a constrained recipe; station-aware queries report `station_available`, and station-aware prepare rejects mismatches before inventory mutation. The compatibility `craft_item` path also rejects station-constrained recipes. A focused regression was added for locked-policy immutability, wrong/right station query and prepare behavior, and rejection of the legacy `craft_item` bypass. `make -C Libft -j4 test-executable` compiled the changed crafting and test sources, then failed during the repository-wide link on pre-existing missing platform symbols (`ft_gpu_window::create`, `ft_render_platform_*`, `cmp_create_sound_device`); runtime execution of the new regression remains unverified. The production Libft archive and full Minecraft `make -j4 normal` build both succeed with the new API. Host station existence, reach, and station revision checks remain Minecraft responsibilities.


Validation update (2026-10-10): `game_crafting_ingredient` now accepts up to eight unique explicit alternative item IDs per ingredient group. Recipe queries allocate available stack units by inventory slot, reserving exact-rarity groups before wildcard groups; prepared crafts use the same two-pass policy and staged inventory, so overlapping groups cannot consume the same units twice. Missing-group query results expose the accepted alternative IDs. The prepared-craft regression compiles, and a standalone MinGW runtime smoke check passed for an overlapping alternative group, a one-unit deficit, and a successful batch prepare/commit. Symbolic tag definitions/resolution and timed processing remain unimplemented.


Validation update (2026-10-10): Minecraft's item catalogue now registers 97 ordinary content definitions covering processing materials, food, equipment, and the seven starter plus eleven current craft-only card items. Every definition declares a stack limit and one primary acquisition category (recipe, world, harvest, or discovery); built-in block items resolve to world acquisition, bedrock resolves to restricted acquisition, and filled buckets resolve to world acquisition. `validate_content_coverage` checks each content definition and policy against the registered catalogue. The edited catalog and validator compile with the repository's strict MinGW warning flags; a linked runtime smoke check passed catalog initialization, coverage validation, and representative recipe/harvest/world/restricted lookups. Full make validation was unavailable because the bundled `make.exe` crashed immediately, while `mingw32-make` could not start Git Bash under the current sandbox. The item catalogue is now present, but recipe registration, tag expansion, processing, card outputs, and the full acquisition pipeline remain open.

Validation update (2026-10-10): The content catalog now contains 99 definitions after adding Card Press and World Loom. `MinecraftRecipeCatalog` registers 25 stable recipe IDs (1001–1025), with explicit ingredient alternatives, station restrictions, initial recipe-book grants, a locked magical-dust recipe, and empty-bucket byproducts for paper and ink processing. Oak, pine, and birch boats all use the shared boat item definition. Strict MinGW compilation of the recipe catalog succeeds, and the linked `--validate-item-catalog` smoke harness passes, including a paper-mill craft that consumes water and returns an empty bucket. This remains a bootstrap subset: symbolic tags, the complete ordinary/equipment/card recipe set, timed furnace and paper-mill jobs, recipe-book persistence/session integration, unlock discovery, and the full acquisition transaction are still open. The full make target remains unavailable in this Windows runner because its make/Git Bash launch failures have not been resolved.

Validation update (2026-10-10): Superseding the previous count, `MinecraftRecipeCatalog` now registers 44 stable recipes: the 26-item-ID range 1001–1026 (with 1024/1025 as pine/birch boats), plus 18 Card Press outputs at 2001–2018. The seven common card recipes and the current 11 rare card recipes are registered with default-known and locked states respectively; advanced ink and magical dust are locked. The item-catalog validator checks those book states along with station gating and the water-bucket return transaction. Strict warning-as-error MinGW compilation and the linked validator pass (`content=99`). Card ingredients are still placeholder approximations for some chosen tags and multi-catalyst recipes, and recipe outputs do not yet mint globally unique physical card instance IDs. The ordinary gear/food/ore recipes, timed processing, recipe-book session persistence, data-driven progression unlock sources, symbolic tags, and full acquisition pipeline remain incomplete; the full make runner is still unable to start in this Windows environment.

Validation update (2026-10-10): The bootstrap catalog has expanded to 79 registered recipes. It now includes bread, bow, arrows, shield, staff, all six configured pickaxe/axe/sword tiers, and cloth/leather/iron armor pieces. Wood and stone tool recipes and cloth/leather armor are initially known; stronger metal/diamond gear, shield and staff remain locked. The catalog compiles with strict MinGW warnings as errors, and the linked item-catalog validator passes after loading the expanded recipe book. This closes those immediate recipe registrations only: bar smelting and cooked meats still require timed fuel processing; ore extraction and remaining material/gear policies need coverage; card catalyst fidelity and unique physical output IDs remain open, as do session persistence, data-driven unlock sources, symbolic tags and end-to-end acquisition transactions.

Session integration update (2026-10-10): `GameSession` now initializes and owns a `MinecraftRecipeCatalog` bound to its `MinecraftItemCatalog`, exposes it through `recipe_catalog()`, and tears it down on startup failures and normal stop. `GameSession.cpp` compiles with the strict MinGW warning set. This makes the catalog available at the world-session boundary, but the session does not yet expose crafting actions or persist/import the recipe book; processing jobs, globally unique physical output IDs, transactional inventory/host coordination, and the other gaps listed above remain open.

Recipe-book persistence update (2026-10-10): The combined `GameSessionPersistence` format now writes version 2 when a recipe book is supplied, storing its independently checksummed export alongside character/inventory and world-item state. Loading still accepts version-1 combined saves and legacy WISP entity-only saves; version-2 loads import the recipe book through `MinecraftRecipeCatalog`, which synchronizes known IDs into crafting. `GameSession` now passes its catalog to the persistence load/save path. The world-item/session validator saves an unlocked rare card recipe, restores it, confirms both book knowledge and crafting query visibility, then verifies outer-checksum and inner-book-checksum failures preserve character, ground-item and recipe state. Strict MinGW compilation and the linked validator pass (`recipe-book=restored`). Session snapshots still omit chunks/card matches/generation state and a shared recovery journal. Recipe action APIs, timed processing, unique physical output IDs and the remaining catalog/acquisition contract are still open.

Persistence compatibility follow-up (2026-10-10): The same runtime validator now also writes a version-1 combined save through the legacy overload, loads it through the catalog-aware overload, and confirms default-known common recipes remain known while the version-2 rare unlock is not introduced. During this extension it caught and fixed an uninitialized-buffer read in the version-1 save path. The loader stages and validates character/inventory and world-item snapshots before importing recipe-book bytes. The validator also corrupts the recipe book's inner checksum, recomputes a valid outer checksum, and verifies the loader rejects the save without changing live character, ground-item or recipe state. The strict persistence translation-unit compile and linked world-item/session validator pass after these checks. Full cross-system failure injection remains open.

Card ingredient fidelity update (2026-10-10): The 18 Card Press recipes now include the declared common/rare substrates and their full row catalysts, including Village Charter's planks and wheat, Volcano Mark's volcanic rock and obsidian, Shipwreck Chart's planks/copper/seagrass, and the multi-material rare recipes. Explicit alternatives now cover the common biome sample set, ore samples, surface-magma fallback stone, and flower samples. The item-catalog validator unlocks and crafts Shipwreck Chart from the full six-group ingredient set and verifies every ingredient is consumed exactly once; strict MinGW compilation and the linked runtime validator pass. Biome samples are still represented with ordinary block-item IDs rather than typed biome-tag/sample items, ore target binding and distinct-biome validation are not implemented, and card outputs still lack host-minted unique physical IDs. Remaining progression, timed processing, full item/recipe coverage, and atomic acquisition integration remain open.

Acquisition/recipe update (2026-10-10): Coal ore now has an explicit block acquisition mapping to the coal material, and the coal item policy is world acquisition rather than an unimplemented recipe. Diamond and emerald materials now have locked stable-ID workbench extraction recipes from their corresponding ore-block items. The item-catalog validator asserts the coal drop policy and world acquisition category and confirms the new extraction recipe IDs are registered locked. The block-edit validator now covers actual dirt and coal-ore mining/drop publication/merge paths and compiles with strict MinGW warnings; runtime execution of that validator remains unverified. The bootstrap catalog now contains 81 recipes; strict MinGW compilation and the linked item-catalog validator pass. Metal bar smelting and cooked meats still lack the required timed fuel processing, and the exhaustive recipe-policy/output coverage invariant has not yet been added. The full Make runner again failed to start Git Bash (`NtCreateDirectoryObject ... 0xC0000022`), so no full application rebuild was available.

Unique output commit update (2026-10-10): `game_crafting::commit_craft` now has an overload accepting a host-minted primary output instance ID. It assigns the ID only to a one-unit primary output, rejects a duplicate already present in the source or staged result, and leaves inventory and prepared state unchanged on duplicate rejection. The focused Libft test compiles with warnings as errors; a linked MinGW smoke run verifies ID 5001 is present on the output and a repeated ID fails without consuming the next input. Minecraft does not yet route card crafting through this overload, and global allocator persistence/deduplication remain open. Timed processing, recipe coverage validation, and the broader session/action/inventory integration are still unfinished.

Unique output allocation and persistence update (2026-10-10): `MinecraftRecipeCatalog::commit_unique_craft` now supplies monotonically increasing host IDs to the Libft commit overload and retries IDs already present in the inventory. The Shipwreck Chart integration validator crafts through this path and checks its nonzero instance ID. Session snapshot version 3 saves/restores the allocator's next ID alongside the recipe book; versions 1 and 2 remain supported. Strict MinGW compilation succeeds for the catalog, persistence layer, session, and affected validators. Relinked focused validators both pass: world-item/session persistence reports `inventory=paired recipe-book=restored`, and item catalog reports `builtin=64 runtime=0 content=99`. The persisted allocator counter is restored after load. This allocator is session-owned and does not yet guarantee uniqueness across separate sessions or all world/container stores. Timed processing, item acquisition transactions, and broader recipe coverage remain open.

Recipe-acquisition coverage update (2026-10-10): `MinecraftRecipeCatalog` records all primary and returned-container outputs and validates them against the item catalogue. `--validate-item-catalog` now fails when a content item labeled recipe-acquired has no registered recipe. This found missing furnace recipes for iron, gold and copper bars and cooked beef, pork, chicken and mutton; stable recipe IDs 1029–1035 now cover those seven outputs. Strict MinGW compilation and the linked item-catalog validator pass (`builtin=64 runtime=0 content=99`). The coverage check includes recipe-book-locked outputs and returned empty buckets. These furnace recipes currently use instant crafting transactions; reserved inputs/fuel, elapsed server ticks, pending outputs, and persisted timed jobs are still required by Section 9. Remaining acquisition contracts include harvest/drop execution and atomic coordination with block edits, inventory, ground items, and host intents.

Physical-ID load reconciliation update (2026-10-10): Combined session version-3 loading now scans staged character inventory and staged ground-item contents for the highest persisted physical item ID. If the saved recipe-catalog counter is nonzero but not above that ID, the loader advances it to the next ID (or leaves it exhausted after `UINT64_MAX`). `WorldItemStore::get_max_item_instance_id` performs the ground-item scan under the store lock. The persistence validator saves a deliberately stale counter (5002) alongside character ID 7000 and ground ID 9000, then verifies the restored allocator starts at 9001; attempts to lower it are rejected. Outer-checksum and inner-recipe corruption preserve the live counter and character state. Strict MinGW compilation and the linked world-item/session validator pass. This prevents new recipe outputs from reusing IDs present in these persisted stores; it does not yet reject duplicate IDs already present across inventories, or cover other world/container saves and concurrent sessions.

Persisted identity uniqueness update (2026-10-10): `WorldItemStore::validate_instance_id_uniqueness` compares nonzero physical IDs across a character inventory and every ground-item inventory, rejecting repeats within either store or between them. Versioned session loading runs this check on staged state before publishing it. The world-item/session validator exercises character-to-ground and ground-to-ground duplicate rejection as well as successful load reconciliation. Strict MinGW compilation and the linked validator pass. The check is scoped to a single persisted character/ground-store pair and assumes the simulation owner prevents concurrent changes during validation; global uniqueness across multiple characters, portable inventories, other containers, and concurrent sessions still needs an authoritative world-level registry/journal.

Ground snapshot identity validation update (2026-10-10): `WorldItemStore::deserialize` now rejects repeated nonzero item instance IDs within one snapshot before swapping staged entries into the live store. Its validator serializes two ground entities carrying the same physical ID, confirms deserialization fails, and confirms the initialized destination store remains empty. This closes duplicate-ID acceptance for standalone ground-store snapshots as well as the combined session path; durable global reservation across separate stores and concurrent sessions remains open.

Live ground identity update (2026-10-10): `WorldItemStore::spawn_pending` now rejects a nonzero physical item ID already present in any published or pending ground entity before reserving an entity ID or adding the chunk-index entry. The world-item validator confirms duplicate spawn rejection does not consume an entity ID, then admits a different unique item; it also forges a checksum-valid duplicate-ID snapshot and verifies decoder rollback. Strict MinGW compilation and the linked world-item/session validator pass. Cross-store reservation still relies on the session-level validation point and does not serialize concurrent ID allocation across players or containers.

Runtime block acquisition validation update (2026-10-10): `--validate-item-catalog` now registers a temporary runtime voxel block, confirms runtime coverage fails while its acquisition rule is missing, registers a self-item rule, confirms coverage succeeds with one runtime rule, and unregisters the block on cleanup. This validates the runtime registry/catalog integration path and dynamic block-item definition. Strict MinGW warning-as-error compilation of `ItemCatalogValidator.cpp` succeeds. The full application rebuild could not start because Chocolatey `make.exe` crashed and MinGW make's configured Git Bash failed to create its named object (`NtCreateDirectoryObject ... 0xC0000022`); the changed validator has not yet been relinked or run. Production runtime blocks still need content-specific policy registration at load time, and global physical-ID reservation, timed processing and durable cross-system transactions remain open.

Runtime block validator follow-up (2026-10-10): The changed `ItemCatalogValidator.cpp` was linked directly with the current catalog/crafting validator objects and `Libft/Full_Libft.a`, bypassing the broken Make/Git Bash launcher. The resulting smoke executable passed (`item-catalog: ok builtin=64 runtime=0 content=99`), so the synthetic register/missing-rule/self-item/unregister path now has runtime evidence. This is a focused validator link, not a full application rebuild.

Hand crafting session API update (2026-10-10): `MinecraftRecipeCatalog` now exposes hand-only recipe query and craft actions, and `GameSession` routes them to its character inventory. Craft calls require a nonzero monotonically increasing request ID and the expected inventory revision. Exact retained retries return the original status/revision; changed payloads are rejected, stale revisions are cached as failures, and evicted IDs cannot execute again because the high-water mark is retained. The cache is bounded to 4,096 entries and is volatile per session. The item-catalog integration validator now confirms successful planks crafting, no duplicate output on retry, changed-payload rejection, and exact stale-revision failure replay. Strict MinGW compilation and a directly linked focused validator pass (`item-catalog: ok builtin=64 runtime=0 content=99`). The API always uses station 0, so it cannot claim workbench, Paper Mill, Card Press or Furnace access. Real station blocks/reach/permission checks, workstation craft actions, UI/network transport, durable cache persistence, and timed processing remain open.

Craft action persistence update (2026-10-10): Combined GameSession snapshots now use version 4 to store the bounded hand-craft request cache beside character inventory, ground items, recipe book and physical-ID allocator. Cache records retain the request high-water mark, recipe/batch payload, expected and resulting inventory revisions, and terminal status; load validates cache checksum, record bounds/order and IDs before changing catalog state. Versions 1–3 and legacy WISP saves remain readable, and older formats start with an empty craft-action cache. The WorldItemStore/session validator now crafts planks, records a stale-revision failure, saves/loads, verifies exact successful and failed retries do not mutate inventory, rejects changed payloads, and rejects a cache-checksum-corrupt file with a valid outer checksum while preserving live state. Strict MinGW compilation and the directly linked validator pass (`world-items: ok partial=2 merge=partial query=bounded retry=stable snapshot=file-roundtrip inventory=paired recipe-book=restored craft-cache=restored`). CardGame play/draw/discard state and caches, station craft jobs/fuel/ticks, and a shared crash-recovery journal remain open.

Craft snapshot compatibility follow-up (2026-10-10): The world-item/session validator now derives a version-3 file from a valid version-4 snapshot by removing the v4 cache-length field and cache payload, recomputing the outer checksum, and loading it through the catalog-aware path. The v3 file restores the recipe book and allocator while clearing request-cache state; a subsequent v1 load uses a fresh catalog and confirms common defaults remain while the v3 rare unlock is not introduced. Strict MinGW compilation and the directly linked validator pass (`world-items: ok partial=2 merge=partial query=bounded retry=stable snapshot=file-roundtrip inventory=paired recipe-book=restored craft-cache=restored`). This verifies the v1/v3 compatibility paths after the v4 format change; version 2 remains covered by its legacy recipe-book overload behavior, while chunk/card-match persistence and crash recovery remain open.
Physical card transfer prerequisite update (2026-10-10): `game_inventory::remove_item_instance(slot, expected_revision, instance_id)` now provides the checked removal primitive required before a physical card item can enter a CardGame deck. It only removes a single-unit, nonzero-ID item when both the slot identity and captured inventory revision still match, and advances inventory revision after updating weight and slot usage. Added a focused regression covering wrong ID, stale revision, successful removal and repeated missing-slot behavior; its test translation unit compiles under strict warning-as-error flags, and a directly linked smoke executable passes (`inventory-instance-smoke: ok`). CardGame import and durable shared inventory/deck transactions are still unimplemented; this primitive alone does not close crash recovery or persist the cross-system identity mapping.
Physical card import update (2026-10-10): `RegenerationCardSession::import_card_item_to_deck` now accepts the seven registered starter card item definitions from a revisioned inventory slot, requires a single nonzero physical item ID, enforces the 100-card session cap and four-copy definition limit across deck/hand/resolution/discard/burn zones, and appends a matching CardGame instance to the draw pile. The session keeps an in-memory bijection from physical item ID to CardGame instance ID, plus a bounded 4,096-request replay cache; exact retries return the same instance, stale revisions and changed request reuse fail without consuming the card, and unsupported rare card items are rejected. `--validate-regeneration-cards` directly covers transfer, identity lookup, retry, stale revision, copy cap and rare rejection; strict MinGW compilation and a directly linked validator pass (`physical-import=ok`). This is an in-process slice: deck return to inventory, persistent bijection/request cache, shared crash journal, rare card definitions/effects and committed World Loom session integration remain open.
Physical card return and revisioned insertion update (2026-10-10): Libft `game_inventory::add_item_revisioned` now adds a unique one-unit physical item only when the caller's inventory revision matches, and returns the committed slot under the same transaction lock. Its regression covers stale-revision no-mutation, exact slot return, revision advance and duplicate physical-ID rejection; strict compilation and a direct MinGW smoke executable pass (`revisioned-inventory-add: ok`). `RegenerationCardSession::export_card_to_inventory` now returns imported starter instances with their stored item metadata, rejects a return that would reduce the session-owned deck below 20, and caches transfer results for exact retries. The regeneration validator additionally covers full-inventory failure without deck mutation, successful return, identity-map removal and exact retry; strict builds and the linked validator pass (`physical-transfer=ok`). The transfer pair is still process-local and assumes the documented simulation owner; the physical identity map/action cache are not serialized, shared crash journaling and concurrency barriers are not present, and rare cards remain unavailable.
Timed-processing transaction foundation update (2026-10-10): Libft `game_crafting::prepare_processing_at_station` now validates recipe ownership, unlock, station, ingredients and inventory revision while preparing an inputs-only inventory result. It returns a fixed-width, value-only snapshot for the primary product and every byproduct, scaled to the batch, without requiring product capacity in the source inventory. `commit_processing_inputs` commits only input consumption through the same stale-revision and staged-inventory checks; a host job can retain the product snapshot and retry delivery later if inventory is full. A regression covers normal immediate crafting failing for full output capacity, processing preparation succeeding, input consumption once, pending output remaining outside inventory while full, and later output insertion. Strict MinGW compilation and a directly linked smoke executable pass (`craft-processing-inputs: ok`). Fuel reservation policy, authoritative tick scheduler, pending-job cache, save/load, unload/reload and output-delivery idempotency are not yet implemented.

Minecraft furnace queue update (2026-10-10): `MinecraftRecipeCatalog` now registers one coal per smelt input and exposes an authoritative-owner-thread processing queue for recipe IDs 1029–1035. Starts are revision checked and cached for exact retries; the batch and queue are bounded. The tick API advances jobs within a caller-supplied budget, keeps finished output pending when inventory insertion fails for capacity or allocation, and removes the job only after successful insertion. This implementation is still volatile: queue state and retry results are not included in `GameSessionPersistence`, station entity/access checks are not integrated, and recovery across a crash between inventory commit and save is not guaranteed. Strict MinGW compile of `MinecraftRecipeCatalog.cpp` passed with `-Wall -Wextra -Werror`, `LIBFT_INTERNAL_HEADERS`, and the voxel backend define. Focused runtime validation and save/load coverage remain required.

Furnace persistence follow-up (2026-10-10): Processing jobs and retained start results now serialize as bounded `MCPJ` version 1 state. Validation checks checksum, version, high-water mark, action/job correspondence, recipe/batch limits, output values and complete input consumption before restoring staged maps. Combined `GameSessionPersistence` is now version 5 and includes the processing payload; versions 1–4 remain readable and older loads clear processing state. `MinecraftRecipeCatalog.cpp` and `GameSessionPersistence.cpp` compile with strict MinGW warnings as errors. This is save/load plumbing without runtime roundtrip verification. It does not make inventory input consumption and save writes one atomic durable transaction, nor does it persist the furnace block/station owner or handle chunk unload/reload.

Processing persistence validation follow-up (2026-10-10): `WorldItemStoreValidator` now drives a furnace job through the combined version-5 save/load path, checking consumed raw beef/coal, retained exact start retry, partial tick progress and single cooked-beef delivery after the resumed deadline. It also adapts the existing version-3 compatibility fixture from a version-5 file by removing both the cache fields and job payload, while retaining the separate version-1 load path. The changed validator compiles with strict MinGW warnings as errors; the runtime validator has not yet been linked or executed against the modified catalog and persistence objects.

Live furnace scheduler update (2026-10-10): `GameSession::tick_world` now accumulates bounded elapsed time at 20 processing ticks per second and advances up to the catalog's 64-job limit. Each furnace input consumes one coal and runs for 200 ticks. The processing accumulator resets at session start/stop; jobs themselves are serialized in version 5. `GameSession.cpp` compiles with strict MinGW warnings as errors. Furnace block placement, reach/permission authorization, station persistence, unload policy and runtime execution remain open.
Processing persistence runtime follow-up (2026-10-10): The full application Make build remains blocked by Git Bash's `NtCreateDirectoryObject ... 0xC0000022` failure. A temporary direct MinGW smoke executable linked the changed recipe catalog, item catalog, session persistence, WorldItemStore, current `game_crafting.cpp`, and `Full_Libft.a` with `-lws2_32`. It passed the v5 furnace path end to end: start consumes raw beef/coal, 75 ticks elapse, save/load restores the queued job, exact start retry is replayed, 124 more ticks do not deliver early, the last tick delivers one cooked beef. Temporary source and executable were removed. The broader design remains incomplete; this validates this single timed-processing persistence path, not the required CardGame, World Loom, transaction-journal or content-effect contracts.

World Loom quote protocol update (2026-10-11): `world_revision` requests with `action: "quote"` now accept decimal `selection_revision`, `policy_revision`, and `opening_card_count` alongside the idempotent `request_id`. The server authorizes the connection to an actor/Loom pair, checks the active draft owner through `query_start_cost`, and returns a direct `world_revision_quote` WebSocket message containing the result code and, on success, paid and bonus chunk counts, opening count, currency item, chunk/opening/total costs, and revisions. World Revision entries now retain bonus status through selection edits; trusted bonus selection uses the same eligibility, protection, cap, and compare-and-set revision validation. Quotes charge only paid chunks. A host-fed occupied-chunk set now protects each occupied chunk and its eight neighbors; updates remove newly ineligible selections and advance the selection revision so prior quotes become stale. Quote retries require identical request bytes and resend the retained result; changed payload reuse is rejected. `game_server::send_to_client` supplies the response path without changing existing world broadcasts. The World Revision validator now captures the protocol response, checks quote amounts/revisions including a paid-plus-bonus quote, verifies exact retry resends the response, and rejects altered bytes under a reused ID. An occupancy exclusion case was added to the validator; live player-location feed and runtime validation remain outstanding. The design's atomic committed start (resource debit, chunk/adjacency reservation, barriers, CardGame session/deal, deadline recovery, and durable journal) remains open.

Single-player occupancy feed update (2026-10-11): each authoritative `GameSession`
simulation tick now updates World Revision occupancy from the floored player
block position, using floor division for negative chunk coordinates. The same
chunk is cached to avoid per-frame vector allocation; crossing a chunk boundary
updates occupancy and invalidates stale selection quotes. Multiplayer player
aggregation, barriers, and atomic commit-time refresh remain open. Strict
MinGW syntax checks pass; the runtime validator cannot currently be rebuilt
because the Makefile's Git Bash subprocess fails to create its MSYS named
object (`0xC0000022`).

World Revision action response update (2026-10-11): valid mutation requests now
receive a direct `world_revision_result` message with request ID, result,
revision identifier, selection/policy revisions, and selected count. The first
action outcome snapshot is retained in the request cache, so exact retries
resend the same state; changed payloads retain the existing rejection behavior.
The network protocol now supports draft `cancel`; success releases the draft's
actor/Loom ownership, and exact retries replay the successful terminal result.
The validator captures responses and checks the successful begin retry is
byte-identical plus cancellation replay. This supersedes the earlier note that
the single-player live position feed was outstanding; multiplayer aggregation
and runtime validation remain open.

Revisioned currency-consumption foundation (2026-10-10): `game_inventory::remove_item_quantity` now consumes an exact positive quantity across matching stackable items under one inventory lock, after confirming sufficient untracked quantity exists. It excludes physical-ID items, checks the expected inventory revision, validates weight/slot accounting before mutation, advances the inventory revision once, and retains successful and failed outcomes by `(scope, transaction ID)` so retries cannot debit twice or change payload. A transaction key cannot be reused across quantity removal and transfer operations. The bounded 4,096-entry cache is volatile. Added tests for split-stack debit, replay after later inventory mutation, changed-payload rejection, cross-operation ID reuse rejection, inconsistent-accounting rejection and insufficient-quantity rollback. The implementation and test translation unit compile with strict MinGW warnings as errors; a directly linked MinGW smoke executable passed split-stack removal, exact retry after inventory mutation, changed-payload rejection and inconsistent-weight rollback. The full inventory test suite remains unrun. This is a debit primitive only: it is not yet wrapped in the design's prepared multi-container/journalled transaction, and the World Loom does not call it.

Shared transaction journal foundation (2026-10-10): Added `GameTransactionJournal`, a bounded version-1 checksummed record store keyed by `(scope, request ID)`. Prepared records retain the transaction type and exact bounded payload; changed reuse is rejected, terminal transitions are idempotent, committed records cannot be aborted, and prepared records can be enumerated for recovery. Deserialize stages all records, validates ordering, uniqueness, state/payload bounds and checksum, then swaps state only after full validation. `save_file` uses the existing atomic replacement helper. `--validate-transaction-journal` is wired into the application validator and `--validate-all`; its direct MinGW runtime smoke passed exact replay, payload conflict, transition guards, pending-record recovery, checksum rollback and file round-trip. Production transaction callers do not yet write or reconcile this journal, so inventory, CardGame, world-item, crafting and World Loom operations remain non-durable and cross-system atomicity is not achieved.

Session journal snapshot update (2026-10-10): Combined game-session persistence now writes version 6 when given a `GameTransactionJournal`, while existing overloads continue writing version 5. Version 6 load stages and validates the embedded journal before swapping it into the caller; legacy snapshots restore an empty journal. `GameSession` now owns and initializes the journal, uses the v6 overload to load and save world sessions, exposes the journal to session transaction callers, and destroys it with the other session state. Strict MinGW compilation passed for `GameSession.cpp`. A standalone runtime round-trip with committed and prepared records passed (`save=0`, `load=0`, committed record state and payload restored). The focused world-item/session validator passes end to end. It exposed and fixed two validator defects: craft retries must use the inventory revision bound to the original cached action, and the synthetic v3 snapshot must omit the v4 craft-cache payload as well as its size field. The test compares craft-cache bytes before/after v5 restore, validates v6 prepared/committed record restoration, then corrupts the embedded journal while recomputing the outer checksum and verifies rejection leaves the live character and prepared journal record unchanged. The new runtime validator completed with `world-items: ok ... transaction-journal=restored`. This persistence path does not yet make participant mutations and journal writes atomic, nor provide production recovery handlers.

Ground pickup recovery update (2026-10-10): Added `WorldItemPickupTransaction` and routed the single-player pickup loop through it. A pickup stores a bounded value payload (entity/revision, actor/request identity, expected inventory revision, quantity, position and tick) in a prepared journal entry and persists it with the pre-mutation session snapshot. The handler then runs the existing revision-checked, deduplicated inventory transfer, records committed/aborted status and saves the resulting character, ground-item store and journal together. Startup replays prepared pickups before entering play and resumes the scope's request ID high-water mark. Debugging exposed and fixed a payload decoder offset bug. Recovery also exposed that Libft inventory persistence omitted the revision: `game_save.cpp` now writes it as a decimal `uint64` field, `game_load.cpp` restores it after contents load, and old snapshots without the optional field remain readable. The Libft save/load regression now checks inventory contents and revision, and the Minecraft validator checks the prepared snapshot restores the expected inventory revision before recovery. These sources compile with strict MinGW warnings as errors. A direct standalone runtime smoke linked against the current changed Libft objects passed prepare/save/load/recover/retry (`loaded-revision=2`, `count=3`, committed journal state, no duplicate credit). The broader `--validate-world-items` executable did not complete with this manually assembled archive, and `make -j1 normal` crashes in the bundled make executable (`0xc0000008`), so full validator execution remains pending. Block destruction/drop, crafting, CardGame and World Loom mutations remain outside this handler; this is not a general cross-system atomic transaction layer.

Durable hand-craft recovery update (2026-10-10): Added `WorldItemCraftTransaction` and routed `GameSession::craft_hand_recipe` through a second journal scope. Its bounded payload binds request ID, recipe, batch and expected inventory revision. The prepared request is checkpointed with the recipe cache and pre-craft inventory before calling the existing recipe-owned transaction; terminal status, resulting inventory and craft retry cache are then saved together. Startup recovery filters by transaction scope and reapplies prepared hand crafts; pickup recovery likewise skips unrelated pending scopes. The world-item validator now covers prepared craft restore, input consumption/output creation, committed state and exact retry without duplicate output. A directly linked runtime smoke passed the full path (`recover=0 planks=4 logs=0`, `retry=0 replayed=1 planks=4`). `GameSession.cpp`, the transaction, and validator compile with strict MinGW warnings as errors. The normal Make runner still crashes and the manually linked broad validator still does not complete; the focused craft smoke is the runtime evidence for this slice. Timed furnace job starts now use a prepared journal transaction; CardGame actions and World Loom resource/chunk commits still do not use the journal.

CardGame byte snapshot codec update (2026-10-10): Added a versioned `GCGS` little-endian engine snapshot format in `card_game_snapshot.cpp`. It binds serialized state to the initialized rules hash, includes a checksum, and encodes player state, events, modifiers, host intents, custom zones, resources, allowances, choices, and usage limits. Restore stages dynamic data and delegates publication to the existing validated snapshot application path. A focused test covers state-hash equality after round-trip, rules-hash mismatch, and rejection of corruption/truncation without mutating live state. Review also found that vector allocation inside the `noexcept` writer path could terminate the process; writer allocation now propagates to the serializer's `FT_ERR_NO_MEMORY` handler. Codec and test translation units compile under strict MinGW warnings as errors, and a directly linked runtime smoke passed an engine snapshot round-trip with matching state hashes.

CardGame session persistence update (2026-10-11): Added versioned `RCS1` serialization for `RegenerationCardSession`. It includes the engine snapshot, play/draw/discard/import/export replay caches, pending Double Draft obligation, transfer and command counters, and physical-card-to-item snapshots. Restore validates checksums, cache identities and physical-card bijection before publishing the reconstructed state. `GameSession` now initializes a seed-bound starter session for new/legacy worlds, restores adapter bytes from the v7 snapshot, and writes them at shutdown; v7 places this bounded payload after the v6 journal while versions 1–6 retain their existing layouts. The RegenerationCard runtime validator passes with round-trip checks for imported physical identity, import/export request replay and Double Draft play/draw/discard replays. Strict MinGW compilation passes for the adapter, validator, GameSession and persistence sources. The session lifecycle test exposed that destroy/reinitialize retained stale engine subobjects; `RegenerationCardSession::destroy` now reconstructs its engine object, and the runtime validator covers reinitialization. The world-item v7 payload validator compiles but was not runnable: bundled Make crashes with `0xc0000008`, MinGW Make's Git Bash fails `NtCreateDirectoryObject` with `0xC0000022`, and a manually linked broad validator exits before reporting. Other CardGame effects still lack world consumers; host-intent economic journaling, action expiry/default policy, and World Loom atomic regeneration commits remain open.

Deterministic choice expiry update (2026-10-11): Exposed the choice ledger's existing transactional default resolver through `card_game_engine::resolve_expired_choices(authoritative_epoch, resolved_count)`. The engine advances its state sequence only when one or more expired choices resolve, reports the count, and keeps the operation idempotent on subsequent calls. The choice ledger prevalidates every expired default before resolving any, so a missing/invalid default leaves all pending choices unchanged. Strict MinGW compilation and a directly linked runtime smoke pass for pre-deadline no-op, deadline default selection, state-hash update, and exact repeated-call no-op. This provides the engine primitive only; the authoritative multiplayer clock, durable server action record, session deadline/restart recovery, and World Loom expiry workflow remain open.

GameSession v7 payload recovery fix (2026-10-11): Runtime round-trip exposed that the v7 loader computed the v6 journal offset from the end of the complete payload, causing it to read overlapping card-session bytes. The journal offset now subtracts the trailing card payload before locating journal bytes. `WorldItemStoreValidator` additionally checks the prepared journal record after v7 restore. Strict MinGW compilation passes for persistence and validator sources; a directly linked v7 runtime smoke passes exact opaque card-payload bytes and a non-empty prepared journal record (`save=0`, `load=0`, 7 card bytes restored).

Minecraft item-tag update (2026-10-11): Added stable named tag groups for wood logs, wood planks, crystal samples, ore samples, and flower pigments to `MinecraftItemCatalog`. Tag resolution stages its result so unknown tags or allocation failure leave the caller's output unchanged. `MinecraftRecipeCatalog` now resolves its wood, crystal, ore-sample, and pigment ingredient groups from those tags, removing the previous repeated plank ID lists from recipe declarations. `ItemCatalogValidator` checks deterministic group membership and unknown-tag rollback. Strict MinGW compilation and a directly linked catalog/recipe runtime smoke pass (`minecraft-item-tags: ok wood-planks=3 recipes=registered`). The complete canonical item acquisition and recipe catalog remains open.

GameSession regeneration-rules snapshot update (2026-10-11): GameSession now owns RegenerationWorldRules, initializes it from immutable world-generation configuration and session policy, serializes its state alongside the CardGame snapshot and transaction journal, and exposes apply_pending_regeneration_card_intent plus begin_card_regeneration_revision. The intent consumer stages the CardGame and rules snapshots, consumes the host intent, and checkpoints both in version 8; failures restore the pre-consumption snapshots. Versions 1–7 remain readable, and existing save overloads keep their previous formats. A v8 runtime smoke initially exposed the card payload offset including trailing rules bytes; the loader now computes the card start after the journal and limits the copied range to the card payload. The rebuilt smoke passes exact card/rules payload and prepared-journal round-trip (save=0, load=0). WorldItemStoreValidator now covers v8 card/rules payload restoration and unchanged output vectors on checksum failure. Strict MinGW compilation passes for persistence, GameSession and validator translation units. This checkpoints rules and intent acknowledgement together but does not yet provide the full World Loom start transaction, durable expiry coordination, or atomic interaction with chunk reservations and currency debit.

Authoritative choice-expiry session bridge (2026-10-11): `RegenerationCardSession` now exposes the engine default resolver, and `GameSession::resolve_expired_regeneration_choices(authoritative_epoch, resolved_count)` checkpoints resolved defaults through the combined v8 save. The method accepts only an externally supplied authoritative epoch; it does not use client time. It snapshots the adapter first and restores it if persistence fails, while a no-op expiry does not rewrite the save. The adapter, GameSession and regeneration-card validator translation units compile with strict MinGW warnings as errors. A runtime clock/restart coordinator still must call this API from authoritative server time and recover the decision window lifecycle.

Timed-processing transaction integration (2026-10-11): Added `WorldItemProcessingTransaction` with a versioned request payload, prepared journal checkpoint, idempotent `start_processing` application, committed/aborted checkpoint, exact retry and startup recovery. `GameSession::start_recipe_processing` now routes requests through that transaction, and pending processing starts are recovered with pickup and hand-craft operations. Transaction persistence uses a `GameSessionSnapshotWriter` callback in production, so prepare and commit saves include the live v8 CardGame and regeneration-rules payloads; direct low-level transaction callers retain the legacy fallback. Added the transaction source to `mk/sources.mk`. Strict MinGW compilation passes for all three transaction units and `GameSession.cpp`. The focused processing-recovery runtime validator has not yet been added or run. Furnace job advancement is still tick-driven and only checkpointed by subsequent session saves; server station reach/claim validation and a journal around each processing-time advance remain open.

Processing-start recovery validator update (2026-10-11): `WorldItemStoreValidator` now prepares a furnace start under a v8 snapshot writer, reloads the prepared journal plus opaque CardGame/rules payloads, recovers the start, verifies one input and fuel consumption, checks the committed journal state, and retries the same request without consuming again. All involved sources compile under strict MinGW warnings as errors. Runtime execution remains unverified: `mingw32-make -j1 normal` failed before compiling because bundled Git Bash could not create its named object (`NtCreateDirectoryObject`, `0xC0000022`).

Processing-start recovery runtime verification (2026-10-11): The directly linked `WorldItemStoreValidator` passes end to end with the new processing transaction scenario (`processing-recovery=exact-retry`). The direct link compiled the current Libft Game sources to avoid stale archive object layouts; the normal Make runner still fails launching Git Bash. The first validator run caught an assertion expecting a non-empty payload on an intentionally empty prepared journal record; corrected the expected payload size to zero. The v8 payload restore, prepared-to-committed processing recovery, one-time ingredient/fuel consumption and exact retry now all pass together.

Pending card-effect validator expansion (2026-10-11): The World Revision
validator now applies a second Deep Seam after draft creation and checks the
manager-owned ore frequency changes. It then applies Surveyor's Reach to a
loaded target, refreshes pending rules, moves that coordinate from paid to
bonus selection under the one-chunk cap, and checks the quote reports one
bonus chunk with zero chunk cost. Strict MinGW syntax compilation passes for
the changed world, session, and validator units; `git diff --check` passes.
The runtime validator remains unexecuted: `mingw32-make -j1 normal` again
failed while bundled Git Bash tried to create an MSYS named object
(`NtCreateDirectoryObject`, `0xC0000022`). The GameSession checkpoint rollback
path still lacks a fault-injection runtime check, and the authorized server
action still does not route through this session consumer.

Card-effect start-boundary guard (2026-10-11): `GameSession` now rejects
pending card intents after revision generation has become active, so a card
cannot be checkpointed as accepted after workers have frozen their rules.
`WorldRevisionManager::update_pending_rules` already rejects active generation;
the World Revision validator now checks that boundary as well. Strict MinGW
syntax compilation and `git diff --check` pass. Runtime validation remains
unavailable because the Make runner still fails in bundled Git Bash before
compilation.

Checkpointed GameSession card actions (2026-10-11): Added session-owned play,
draw, and discard entry points. Each captures the adapter snapshot, applies
the action, and writes the combined version-8 session snapshot (including
CardGame, regeneration rules, and journal); a failed save restores the adapter
and clears any success-shaped output. Adapter/rules accessors are now
read-only. Strict MinGW compilation passes for `GameSession.cpp` and
`WorldItemStoreValidator.cpp`; compiling `RegenerationCardValidator.cpp`
under the same strict set still hits an existing useless-cast warning at
line 61. `git diff --check` passes. This does not yet journal the external
inventory transfer and does not connect these methods to the World Loom
network server; card item import/export still needs session wrappers.

Pending revision card-effect integration (2026-10-11): Accepted card intents
consumed while a World Revision draft is pending now refresh the manager-owned
generation config from the immutable base config and the latest rules overlay.
Surveyor's Reach requires a pending draft and selects its typed chunk target
through the trusted bonus-selection path. That path applies the configured
selection cap. If the combined session checkpoint fails, the card/rules
snapshots and new bonus selection are rolled back, and the prior rules are
reapplied to the manager config. `WorldRevisionValidator` now checks that a
Deep Seam applied after draft creation changes the pending manager config.
Affected sources pass strict MinGW syntax compilation and `git diff --check`.
Runtime execution was not performed; the known Make/Git Bash launch failure
remains. Card effects still lack connection to the authorized World Loom
network action and full village, magma, and extra-scope generation consumers.

Checkpointed physical card transfers (2026-10-11): `game_inventory::restore_snapshot`
stages and replaces contents in place while preserving inventory identity,
revision, next slot and existing process-local request caches. Card transfer
wrappers now use `WorldItemCardTransferTransaction`, which writes a versioned
prepared record into the shared journal before mutation, checkpoints the
combined session, applies the adapter's idempotent import/export, then saves
the resulting inventory, CardGame replay cache and terminal journal state.
Startup recovery reapplies matching prepared records; retries validate the
original operation, item/card ID and expected inventory revision before
replaying the cached result. Strict MinGW syntax compilation passes for the
transaction, `GameSession.cpp`, and the world-item validator. The validator
now prepares an import, reloads the pre-action snapshot into fresh objects,
recovers the transfer, and checks the committed record, deck count, physical
identity and exact retry. The Make runner still fails before build with Git
Bash `NtCreateDirectoryObject` access denied, so runtime execution of the save-failure injection coverage remains unverified. The current file replacement is
atomic but does not promise fsync-level crash durability; host-intent delivery
and committed World Loom start are still separate unfinished contracts.

Inventory restore validator coverage (2026-10-11): `WorldItemStoreValidator`
now captures an inventory after a successful scoped transfer, removes its
source item, restores the value snapshot, checks contents/slot/revision/next
slot, then retries the original transfer request and checks the cached result
and destination quantity. The validator source and inventory/session sources
pass strict MinGW syntax compilation and `git diff --check`. The runtime
validator has not been executed; the Make runner continues to fail before
compilation in bundled Git Bash.


Script policy-context slice (2026-10-11): `card_game_script_bridge` now registers `context_value(key_id)` as a fixed-order native. Consumers register up to 64 increasing, nonzero signed-integer context keys before binding scripts and set/clear their values between actions. The sorted key schema contributes to the versioned bridge manifest; live values remain host-owned and excluded from content identity. Added focused coverage for key schema hashing, value exclusion from the hash, unknown key rejection, schema lock after binding, and value delivery as a typed host intent. Strict MinGW syntax compilation passes for the bridge and focused test translation units. A directly linked smoke using the changed bridge source and `Full_Libft.a` passes (`script-context-smoke=0`), verifying key-schema hashing, value exclusion from the hash, and typed intent delivery. The registered test harness was not run because Git Bash cannot create its MSYS named object (`NtCreateDirectoryObject`, `0xC0000022`). Wiring restored context values from Minecraft policy and engine/session lifecycle ownership remain open. The bridge now serializes the registered key/value state as bounded `CCV1` data; restore requires an exact key schema, validates checksum and flags before publishing, and rejects calls during a callback. A focused test covers value restore and corrupt-payload rejection. Strict MinGW syntax compilation passes for production and test translation units, and a directly linked runtime smoke confirms a restored signed value reaches a scripted host intent (`error=0 intents=1 value=-23`). `RegenerationCardSession` now persists the bounded `CCV1` context payload in its versioned `RCS1 version 3` snapshot; restore recreates the ordered stored key schema and validates the context checksum. Script catalogue loading and comparison against a trusted content manifest before restore remain open. Script-side draw and mandatory-discard requests are exposed as bounded `draw_cards(count)` and `require_discard(count)` operations. They append to the callback operation buffer and apply inside the event transaction; draws reuse the configured policy, and discard requests create the existing physical-card obligation in the configured zone. The native names enter the bridge manifest through ordered registration. Strict MinGW syntax compilation passes for the production bridge, engine operation units, and focused test translation unit. Directly linked runtime smokes confirm `draw_cards(1)` moves one card from the deck to the active player hand and `require_discard(1)` creates a pending obligation in the configured discard zone (`pending_discard=1`, `zone=41`). The focused test repeats both checks. The Make test runner remains unavailable because bundled Git Bash fails `NtCreateDirectoryObject` with `0xC0000022`, so the registered harness test itself is unrun.


Script callback teardown slice (2026-10-11): CardGame now exposes owner-checked callback unregistration that clears callback/user-data slots without reusing or compacting stable effect IDs. card_game_script_bridge::destroy detaches all registered effects before freeing compiled programs, and leaves the bridge intact if detachment fails. Destroyed engines accept teardown as a no-op because their registry is already cleared. Added validator coverage proving events emitted after bridge destruction do not deliver script host intents. Strict MinGW syntax compilation passes for the CardGame registration unit, script bridge, and focused tests. A directly linked smoke using the changed units and `Full_Libft.a` passes (`script-teardown-smoke=0 pending=0`); Make-driven harness remains unavailable due Git Bash named-object access failure. The engine object must outlive bridge destruction and cannot be reinitialized while a bridge still owns callback registrations.
Card session transfer wrappers verification (2026-10-11): The current
`GameSession` exposes `import_regeneration_card_item_to_deck` and
`export_regeneration_card_to_inventory`, blocks both during pending or active
world revisions, and routes them through `WorldItemCardTransferTransaction`
with the combined-session snapshot writer. Startup recovery dispatches the
same transaction scope. This supersedes the earlier checkpoint that said the
session wrappers were still missing. The transaction recovery validator
covers prepared import recovery and exact retry; direct runtime coverage of
the public `GameSession` wrappers and their save-failure rollback remains
open. Network authorization/action transport remains unimplemented.
Special-item acquisition coverage (2026-10-11): Empty bucket, water bucket,
lava bucket and magical dust now appear in the explicit content acquisition
rule table instead of being synthesized only by special-case lookup. This lets
`validate_content_coverage` check their definitions and makes recipe coverage
require outputs for empty buckets and magical dust while keeping filled buckets
world-acquired. The focused item catalog validator checks the four policies.
Strict MinGW syntax compilation passes for the item catalog, recipe catalog and
validator. A directly linked runtime smoke passes both coverage validators
(`error=0 content=103 outputs=92 missing=0`, magical-dust policy=recipe).
Catalog tag-reference validation (2026-10-11): `MinecraftItemCatalog` now
validates content acquisition rules and every item-tag reference during
initialization. Tag item definitions must exist, duplicate `(tag, item)` pairs
are rejected, and each registered built-in tag group must have at least one
member. `ItemCatalogValidator` continues checking deterministic tag membership
and unknown-tag rollback. Strict MinGW syntax compilation passes for the item
catalog and validator; a directly linked runtime smoke passes initialization
and the full content/tag coverage check (`error=0 content=103`).

Regeneration manifest identity update (2026-10-11): `RegenerationWorldRules` now derives its manifest identity from the voxel generation signature, CardGame content version, every regeneration policy limit, and the per-biome Compass adjustments. Initialization rejects a supplied hash that does not match the current rules inputs. `GameSession` and both rules validators compute the canonical hash rather than constructing ad hoc values. Strict MinGW syntax compilation passes for the rules, session, and validator translation units. A focused directly linked runtime smoke passes manifest initialization, serialization and rejection of old snapshot bytes under a changed policy (`manifest policy mismatch rejected`). The full `RegenerationCardValidator` executable remains unrun because the standalone validator build has unresolved pre-existing CardGame session/inventory symbols, and the MSYS Make runner remains blocked by `NtCreateDirectoryObject` access.
Regeneration manifest collision resistance update (2026-10-11): The V2 canonical rules manifest now hashes the complete deterministic serialized voxel configuration in addition to Voxel's existing 32-bit configuration signature (which captures callback presence), then folds in CardGame content version and all regeneration policy values. The validator checks both changed policy and changed generation configuration against previously serialized rules bytes. Strict MinGW syntax compilation passes for rules, session, and both validators. A directly linked runtime smoke confirms changed sea level and changed policy each produce a distinct manifest, and old rules bytes are rejected under a changed policy (`manifest config and policy mismatch detected`). The existing full application validator remains unrun.
Card script-context session persistence (2026-10-11): `RegenerationCardSession` now owns the CardGame script bridge, exposes host methods to register ordered context keys and set/clear signed values, and stores the bridge's bounded `CCV1` payload in adapter snapshot version 2. Version-1 `RCS1` snapshots remain readable. Restore reconstructs the stored ordered key schema, validates the bridge payload/checksum, and then publishes the engine and adapter state. The focused validator checks byte-identical snapshot roundtrip after setting key 17 to -23. Strict MinGW syntax compilation passes for adapter, persistence, validator and GameSession sources; a directly linked smoke passes (`script-context-session-roundtrip=ok`). Content-catalogue script registration and validation against an independently trusted manifest remain open; Make is blocked by Git Bash `NtCreateDirectoryObject` access denial.
Card-session snapshot compatibility check (2026-10-11): The focused regeneration-card validator now builds a version-1 adapter fixture from a version-2 snapshot by removing the `CCV1` extension and recomputing the snapshot checksum. It verifies that the legacy state and physical-card mapping restore while script context keys remain absent. Strict MinGW syntax compilation passes for the validator and adapter sources; a directly linked compatibility smoke passes both version-2 context roundtrip and version-1 restore (`script-context-v2-and-legacy-v1=ok`).

Card transfer terminal-save rollback (2026-10-11): `apply_prepared` now
snapshots inventory, card-session and journal state before applying a prepared
transfer. If marking or checkpointing the terminal state fails, it restores
the in-memory state and returns the persistence error, leaving the transaction
retryable. The world-item validator injects failure on the terminal save, checks
that the physical card and deck state are restored with a PREPARED journal record,
then retries and checks that it commits exactly once. Production and validator
translation units pass strict MinGW syntax compilation. Runtime execution remains
unavailable through the current Make runner (NtCreateDirectoryObject,
0xC0000022); fsync durability and World Loom cross-system coordination remain
open.


Host-intent consumer rollback reporting (2026-10-11): GameSession::apply_pending_regeneration_card_intent now checks each compensating operation after a failed rules/selection/session checkpoint: removing an added bonus chunk, restoring card and rules snapshots, and refreshing pending-revision rules. It returns the first rollback error when compensation fails, instead of hiding it behind the original operation error. Strict Windows MinGW syntax compilation passes for GameSession.cpp; the full validator remains unavailable through the Make runner's MSYS named-object error. Persistent host-intent delivery and journal coordination remain open.


World Revision host commit boundary (2026-10-11): WorldRevisionServer no longer calls World::start_revision_regeneration() directly on confirm. Hosts must install a WorldRevisionServerStartCommitter before initialization; confirm supplies the resolved actor/Loom IDs, request ID, expected selection and pricing-policy revisions, and opening-card count (4–7). The server caches the callback result by actor/Loom/request, so exact retry does not invoke it twice and changed payload reuse is rejected. WorldRevisionValidator covers parameter delivery, exact retry, changed-payload rejection, and a server without a configured committer refusing confirm while leaving the draft pending. Strict MinGW syntax compilation passes for server and validator. No application currently wires the callback to GameSession; the durable resource debit, card deal, chunk reservation/barriers and generation commit remain open.


Opening-deal adapter slice (2026-10-11): RegenerationCardSession::deal_starting_hand can extend the pristine four-card bootstrap hand to a quoted 4–7 cards. It rejects use after card actions/transfers, stores request ID and target size in RCS1 snapshot version 3, returns the same result on exact retry after restore, and rejects changed request reuse. Versions 1 and 2 remain readable; the compatibility validator builds a v1 fixture by stripping the v3 extensions. A validator now checks hand/deck counts, in-process retry, snapshot restore, and restored retry. Strict MinGW compilation passes for adapter, persistence, and validator sources; runtime remains unverified because the full Make runner cannot launch Git Bash. Resource debit and invoking this operation through the World Loom commit transaction remain open.


GameSession World Loom commit slice (2026-10-11): GameSession::bind_world_revision_server installs a host commit callback, and commit_regeneration_start validates actor/Loom/request IDs plus the quoted selection and cost-policy revisions, calculates the authoritative quote, rejects currency amounts that cannot fit the inventory API, prepares a WLS1 shared-journal record and saves before mutation, consumes the quoted currency, completes the persisted 4–7 opening deal, then starts selected-chunk generation and records COMMITTED. Failures before generation restore value-snapshot inventory and card session, mark the request ABORTED and checkpoint. Startup recovery aborts a remaining PREPARED record because World Revision draft/selection state is not persisted. Strict MinGW syntax compilation passes for GameSession and WorldRevisionServer. No production server currently binds the callback; durable draft/reservation state, barrier placement, and recovery of the generation commit remain open. A terminal checkpoint failure after generation begins is surfaced, with the live session retaining the committed journal state; crash-consistent world/chunk persistence is not yet available.


World Loom decision-window lifecycle correction (2026-10-11): The host commit now stops after the quoted debit and persisted opening deal, leaving the pending revision available during the card decision window. GameSession::finalize_regeneration_start(authoritative_epoch) resolves expired choices using the supplied host epoch, consumes pending card intents through the durable session bridge, then starts generation. WorldRevisionServer freezes draft mutations after a successful commit while preserving exact request replay. Strict MinGW syntax compilation passes for the changed GameSession, server, adapter, persistence, and validator units. The host still needs to schedule finalization at the authoritative deadline and persist/recover the draft lifecycle; pending choice state is not itself a deadline gate in this method.


World Loom finalization guard (2026-10-11): GameSession now permits finalize_regeneration_start only after a successful paid/deal commit and terminal snapshot save. The guard resets when a new revision draft begins or the session stops, preventing an uncommitted selection draft from bypassing cost and opening-hand processing. A retry of a committed start re-saves the session checkpoint before restoring the in-memory gate. Strict MinGW syntax compilation and diff whitespace checks pass. The gate is currently in-memory; because draft state is not recovered on restart, durable decision-window recovery remains open.


Draw host-notice delivery slice (2026-10-11): RegenerationCardSession now validates the registered empty-draw and hand-overflow intent schemas, including player ownership and payload bounds. GameSession::consume_pending_regeneration_draw_notice returns the front notice to the host and checkpoints its acknowledgement together with the CardGame snapshot; on save failure it restores the pre-ack snapshot and leaves the output unchanged. This prevents typed draw notices from being mistaken for policy-card intents and silently blocking later processing. Strict MinGW syntax compilation and diff whitespace checks pass. The server/UI subscriber still needs to call this API and deliver the returned notice to the user-facing action stream.
RegenerationCardValidator now checks accepted empty-draw and overflow notice schemas and rejects a notice carrying the wrong actor ID. The validator translation unit passes strict syntax compilation; the full executable validator was not run because it would use the existing binary rather than a rebuild of these source changes.

Regeneration action-window gate (2026-10-11): GameSession now rejects card play, draw, discard, policy-intent application, draw-notice consumption, and choice expiry unless a paid/opening-deal start is checkpointed and the world revision is pending. This binds the public session actions to the committed decision window; once generation starts or a new uncommitted draft opens, these actions fail closed. Strict MinGW syntax compilation passes for GameSession, adapter, persistence, and validator units. The state gate is volatile and does not replace the still-required restart-safe deadline/draft record or host action authorization.

Mandatory-discard finalization barrier (2026-10-11): RegenerationCardSession exposes its persisted Double Draft discard obligation, and GameSession::finalize_regeneration_start now refuses to begin generation while any mandatory discard remains. The existing Double Draft validator checks obligation count transitions from one to zero around the required physical hand discard. Strict syntax compilation and diff checks pass; runtime execution remains unavailable because the Make launcher fails in its Git Bash/MSYS initialization before building.

Pending-choice finalization barrier (2026-10-11): RegenerationCardSession now counts unresolved choices from the authoritative engine snapshot. GameSession resolves defaults against the supplied server epoch, checkpoints changes, and refuses to start generation if any choices remain unresolved; this prevents an early or incomplete finalization call from skipping a live choice window. Combined with the mandatory-discard check, generation waits for both choice and draw obligations to clear. Strict syntax compilation and diff checks pass. Deadline ownership and restart recovery still require host integration.

World Loom finalization retry update (2026-10-11): If generation has already entered its active state but the terminal session checkpoint fails, a repeated finalize call now retries the checkpoint and returns its result instead of rejecting the already-started revision. This makes the live-process terminal save recoverable without re-debiting resources or re-dealing cards. Strict MinGW syntax compilation and diff checks pass. It does not make generation/chunk publication crash-durable across process restart.

Furnace tick checkpoint slice (2026-10-11): GameSession now snapshots inventory and processing-job state before advancing active jobs, saves the combined GameSession snapshot after the advance, and restores both state snapshots if advancement or persistence fails. Inactive processing queues avoid unnecessary writes. A new catalog query reports active job count. Strict MinGW syntax compilation passes for GameSession, recipe catalog, processing transaction, and world-item validator; diff checks pass. This provides bounded in-process rollback and per-advance checkpointing. A prepared journal record for each tick and authoritative station reach/claim validation are still open.

Prepared furnace-advance transaction (2026-10-11): Added WorldItemProcessingAdvanceTransaction. Each active server-tick batch stores its bounded tick/job-budget payload in the shared journal and checkpoints PREPARED before mutation. The advancement then updates processing and inventory, marks the journal COMMITTED, prunes older terminal records for this internal scope while retaining the newest monotonically increasing request ID, and checkpoints the combined session. If final persistence fails, inventory, processing state and in-memory journal restore to the prepared snapshot. Startup replays prepared advances before play. The WorldItemStoreValidator now covers injected terminal-save failure, no in-memory progress leak, prepared recovery and byte-identical exact retry; GameTransactionJournalValidator covers high-water preservation after pruning. Strict MinGW syntax compilation and diff checks pass. The Make launcher still fails before building in bundled Git Bash/MSYS, so runtime validators have not run. Station reach/claim authorization remains open.
The direct --validate-transaction-journal equivalent built from the edited validator source passed at runtime (	ransaction-journal: ok ...), including retained high-water ID behavior after pruning. The processing-advance integration validator compiles with strict warnings but could not yet be linked against the stale Oct-10 Libft archive: it lacks current recipe/catalog and CardGame symbols; bundled Make still fails during MSYS startup. The processing transaction therefore remains runtime-unverified.


Validation follow-up (2026-10-11): The direct processing-integration validator initially exited with a stack overflow on the default Windows stack. Relinking with a larger stack exposed a failure in the earlier card-transfer recovery fixture, before the furnace assertions. Inspection confirmed RegenerationCardSession::restore_snapshot requires an uninitialized destination and initializes it internally; an attempted explicit initialization was removed. The current direct run returns FT_ERR_NOT_FOUND during the card-transfer fixture, so it does not validate processing advancement. Strict syntax compilation and the direct journal validator pass. The production Make/MSYS launcher remains unavailable, and the temporary direct link uses the existing Libft archive.
Card-transfer fixture correction (2026-10-11): The second physical card in the terminal-save-failure case now uses a Village Charter instance. Deep Seam already has the four permitted starter copies, so importing another Deep Seam would test the copy cap instead of the intended persistence-failure recovery. The updated validator compiles strictly; its direct integration run still stops earlier in the card-transfer recovery fixture with FT_ERR_NOT_FOUND, so furnace advancement remains unverified.
Processing transaction validation update (2026-10-11): The direct MinGW integration executable now passes both GameTransactionJournalValidator and WorldItemStoreValidator, including the processing-advance terminal-save failure, prepared replay, and exact-retry assertions (processing-advance-recovery=prepared-replay-and-exact-retry). The run links current Game/CardGame objects and edited Minecraft sources against the available Libft archive, uses a 16 MiB Windows stack for the existing large world-item validator frame, and exits 0. The validator fixture now discovers the retry card's inventory slot by physical instance ID after the previous card removal; it no longer assumes slot 0. This closes runtime validation of the journaled furnace-advance slice only. Station reach/claim authorization, durable World Loom start, complete item/recipe coverage, script content loading, and remaining Section 13/14 coverage are still open.
Recipe coverage update (2026-10-11): MinecraftRecipeCatalog::validate_recipe_coverage now checks all 18 currently enabled card item definitions, including rare cards whose acquisition policy is discovery rather than ordinary recipe acquisition. Every card must have exactly one recipe output; the seven starter-card recipes must be default-unlocked, and the eleven rare-card recipes must remain locked pending discovery. Strict MinGW compilation passes, and the directly linked item-catalog validator passes at runtime (item-catalog: ok builtin=64 runtime=0 content=103). This validates recipe/output/knowledge coverage; rare-card effect execution and discovery sources remain open.
Recipe discovery update (2026-10-11): MinecraftRecipeCatalog now accepts trusted, content-defined (source_type_id, source_id) -> recipe_id bindings for recipes that are locked at registration. Discovery grants resolve only through a registered source and pass the stable source-event ID to game_recipe_book for idempotency. Conflicting bindings, unlocked targets, and unknown sources fail closed; identical registration and event retries are safe. ItemCatalogValidator covers binding conflict, successful rare-recipe grant, exact event retry, and unknown-source rejection. Strict MinGW compilation and direct runtime validation pass (item-catalog: ok builtin=64 runtime=0 content=103). Actual loot, exploration, and research content bindings and their event consumers remain to be configured and integrated.
Recipe discovery persistence slice (2026-10-11): GameSession::grant_recipe_discovery now snapshots the recipe-book state, applies a registered source grant, and checkpoints the session before acknowledging a new unlock. If the checkpoint fails, it restores the prior book and synchronizes the crafting unlock flags; exact duplicate grants remain successful without an unnecessary checkpoint. MinecraftRecipeCatalog now exports/restores recipe-book snapshots for this rollback, and game_crafting::lock_recipe supports restoring locked state. ItemCatalogValidator verifies book and runtime-crafting rollback, and passes in a direct strict MinGW runtime build. Actual loot/exploration/research source bindings, authoritative event consumers, and durable-save failure injection remain open.

Recipe discovery checkpoint failure injection (2026-10-11): The checkpointed grant/rollback logic is now isolated in RecipeDiscoveryTransaction and is used by GameSession. ItemCatalogValidator injects a disk-full save result, verifies recipe-book and runtime-crafting locks are restored, then retries the same event with a successful checkpoint and confirms later exact retry does not write again. Direct strict MinGW runtime validation passes (item-catalog: ok builtin=64 runtime=0 content=103). GameSessionPersistence uses filesystem_atomic_write, which provides atomic visibility but explicitly does not guarantee crash durability; fsync-level durability remains an open persistence requirement.

Libft crafting-lock regression update (2026-10-11): Extended the existing locked-recipe transaction test to lock a recipe after a successful unlock/craft, verify `is_recipe_unlocked` returns false, and verify the subsequent craft is rejected without consuming its input or changing its prior output. The updated Libft test translation unit and `game_crafting.cpp` pass strict MinGW syntax compilation. The Libft test runner was not rebuilt or executed in this slice.

Script catalogue loading and trusted manifest slice (2026-10-11): `card_game_script_bridge` now serializes and loads bounded `CGSC` v1 catalogues containing event bindings and canonical serialized programs. Loading validates the checksum, bounds, deserialization and program verifier before registering anything, then computes and compares the trusted manifest; a mismatch unregisters the staged callbacks. The manifest covers ordered scripting native IDs/names, canonical context-key IDs, sorted CardGame host-intent schemas and draw policy, script event bindings/program bytes, and an optional host policy-manifest hash supplied with `set_policy_manifest_hash`. The new script-environment schema hash is independent of host registration order and excludes callback pointers. The focused bridge runtime test passes corrupt-bundle rejection, policy/content hash mismatch rollback, successful load and execution (`exit 0`); strict MinGW compilation passes. The host must still authenticate the expected hash and enforce it during session import; Minecraft has not yet wired an authored script catalogue or generation-policy hash into this bridge.

Manifest limit correction (2026-10-11): The bridge manifest also includes the configured script operation limit, since it changes whether a program completes. The focused mismatch test checks operation-limit and policy-hash differences both roll back before a trusted catalogue loads.

Canonical schema hash validation (2026-10-11): A focused Libft runtime check now constructs two engines with identical host-intent definitions registered in opposite orders and confirms `get_script_environment_hash()` returns the same value. The script-catalog test also passes operation-limit mismatch and policy-hash mismatch rollback, checks a corrupted checksum, then loads and executes the matching catalogue.

Minecraft catalog lifecycle slice (2026-10-11): `RegenerationCardSession` RCS1 version 4 snapshots now store the bridge manifest hash. Restore registers the saved context-key schema, restores its values, loads an optional caller-supplied catalog against the independently trusted content hash and current generation-policy hash, verifies the resulting bridge hash against the snapshot, then restores engine state. Legacy RCS1 versions 1–3 remain supported for scriptless sessions. `GameSession::configure_regeneration_script_catalog` accepts the bounded catalog and authenticated expected hash before `start()`; startup binds it to the calculated generation-policy manifest for both new and saved sessions. Strict MinGW syntax compilation passes for GameSession and both adapter translation units. Authored catalog delivery and host authentication of the content hash remain caller responsibilities; no authored Minecraft card scripts are configured yet.

Starter effect preflight slice (2026-10-11): GameSession now checks a new card effect against the active `RegenerationWorldRules` before the adapter consumes its physical card. Biome-final-removal, ore caps, village/magma plan caps, bonus-target caps and compass weight limits are checked read-only; Double Draft has no world-policy precondition. Cached play retries bypass preflight and continue through the adapter's exact-payload replay checks. The World Revision validator now checks that a capped Village Charter is rejected before mutation and that its plan count remains unchanged. Strict MinGW syntax compilation and `git diff --check` pass. A focused direct MinGW runtime harness linked to current CardGame and adapter sources passes the capped-plan rejection and unchanged-count assertions; the full World Revision validator has not run.

Surveyor target preflight extension (2026-10-11): Added a read-only `WorldRevisionManager::validate_bonus_chunk` path and exposed it through `World`. It checks the same draft revision, selection cap, loaded-chunk, protection and occupancy conditions as bonus selection without changing selection state. GameSession uses it before consuming Surveyor's Reach, and the World Revision validator checks valid-target preflight plus duplicate-target rejection after selection. Strict syntax compilation and `git diff --check` pass; full validator runtime remains outstanding.

Ore Overgrowth policy slice (2026-10-11): Rare card definition 11 now resolves to a persisted `REGENERATION_CARD_EFFECT_ORE_OVERGROWTH` intent. Read-only preflight checks the selected enabled ore, checked 2x arithmetic and the configured scale cap; accepted intents compose in host order with Deep Seam. The World Revision validator covers the 2x then 1.5x result and cap rejection without mutation. Strict MinGW syntax compilation passes for rules, persistence and validator units; full validator runtime remains outstanding. External consumable authorization, instability adjustment and authored script catalog delivery remain open.

Ore Overgrowth consumable path (2026-10-11): The session now maps the crafted physical Ore Overgrowth item to card definition 11, registers that card and event callback, validates typed ore choices, and accepts its play intent. Import consumes the identified physical item through the journaled transfer wrapper before the card enters the deck. `RegenerationCardValidator` now checks the rare item import, physical-item mapping, deck insertion and snapshot roundtrip path. Strict MinGW syntax compilation passes for the session, persistence, rules and validator units; runtime validator execution remains outstanding. Rare-card instability and the remaining rare-card effects remain open.

Chunk-local instability record foundation (2026-10-11): Added a World-owned per-coordinate store with optional challenge defaults, integer policy caps, independently configured monotonic reward/danger thresholds, compare-and-set updates, permanent modifier/version and feature outcome/version records, and generation revision/rules hash provenance. Its `WCI1` v1 codec is bounded, checksummed, tied to the configured policy hash, and restores transactionally; safe file replacement is available through World APIs. World Revision validator source now checks defaults, band transitions, cap/stale-revision rejection, metadata roundtrip and corruption rollback. A directly linked MinGW smoke passes the record update/roundtrip/corruption path, and strict MinGW syntax compilation passes for the store, facade, World and validator. Runtime integration with card actions, generation result commit, generated structure outcomes, and client-redacted views remains open; this is persistent state groundwork, not a complete instability gameplay loop.

Combined instability checkpoint update (2026-10-11): Combined GameSession snapshots now write version 9, with a bounded optional chunk-instability payload after the version-8 card and regeneration-rules payloads. Versions 1–8 remain readable; older snapshots yield an empty instability payload, and version 9 permits an empty payload when no policy is configured. GameSession accepts an explicit instability policy before startup, configures the World store before restore, rejects stored records when no policy is supplied, and serializes configured records with every session checkpoint. WorldItemStoreValidator now covers version-9 opaque payload roundtrip and leaves its destination unchanged on checksum failure. Strict MinGW syntax compilation passes for GameSession, persistence, the World facade and validator; runtime execution remains unavailable through the current Make/MSYS launcher. Card action updates, generation commit provenance, and reward/danger gameplay remain open.

Ore Overgrowth instability slice (2026-10-11): GameSession now requires a host-configured positive per-use instability delta, bounded by the configured store maximum. The amount is included in the canonical regeneration manifest, so scripts and restored rules reject a changed value. Card preflight checks the cost against every selected chunk before the physical card is consumed. Pending intent application updates the selected chunks and checkpoints instability together with the card/rules acknowledgement; any update or save failure restores the prior store snapshot. When regeneration starts, the World materializes bounded instability records for selected chunks before queue submission; successful chunk publication records a per-chunk monotonic generation revision and effective generation configuration signature as provenance. The revision advances beyond the stored value after restart, while record allocation happens before workers are queued. World Revision validator source now checks manifest-hash sensitivity, record materialization, and provenance update. Strict MinGW syntax compilation passes for the session, rules, revision manager/facade, generation applier and validator. Runtime session action, generation publication, and injected-save-failure coverage remain open; other rare cards, generated feature outcomes, and client-redacted views remain unimplemented.

Public instability view slice (2026-10-11): Added a World/store read API that returns only chunk coordinates and reward/danger bands. It does not return current value, modifier IDs, feature outcome IDs, generation revision, or rules hash; for unmaterialized chunks it derives bands from the configured challenge default without allocating a record. World Revision validator source checks the public bands against a record containing hidden modifiers/outcomes and checks that querying an unmaterialized chunk leaves serialized store bytes unchanged. Strict MinGW syntax compilation passes for the store, facade and validator. A client/network caller still needs to use this view instead of the server-only full record API; network redaction and feature/reward content remain open.

World reuse isolation (2026-10-11): `World::destroy()` now clears per-chunk instability records while retaining the configured policy. Reusing a World for a new seed therefore cannot carry old chunk values, modifiers, feature outcomes, or provenance into the next session; the matching policy can be applied again and the new seed's snapshot can restore its own records. World Revision validator source checks the cleared record returns to the configured default after destruction. Strict MinGW syntax compilation passes for World, the store and validator. Runtime execution remains blocked by the Windows Make/MSYS launcher and WSL access denial.

Durable GameSession checkpoint slice (2026-10-11): `GameSessionPersistence::save_file` now writes the combined session snapshot through Libft's `file_replace_safe`, which synchronizes file contents and the parent directory on POSIX. Windows replacement now passes `MOVEFILE_WRITE_THROUGH` alongside replace/copy flags after flushing the temporary file. The standalone regeneration-rules overlay and World Revision metadata use the same path. Strict MinGW syntax compilation passes for the persistence, rules, metadata and compatibility units; direct Windows runtime write/read smoke checks pass for both text and binary payloads, including embedded zero bytes and high-bit values. Full validator execution remains blocked before compilation by Git Bash's MSYS object-creation access denial.

Durable policy-file follow-up (2026-10-11): Standalone `RegenerationWorldRules::save_file` and `WorldRevisionMetadataStore::save_file` now use the same safe replacement primitive instead of visibility-only atomic writes. Rules overlays still require coordinated session/draft recovery, but their file payload and replacement metadata are flushed consistently with the combined save. Strict syntax compilation and diff checks are recorded with the current persistence slice; runtime validators remain unavailable through Make.

This update supersedes the Script effect adapter row's earlier RCS1 version 3 and “catalogue loading remains open” status: catalogue validation and restore-time trusted-hash enforcement are implemented in the bridge, adapter and GameSession lifecycle. The row's still-open policy wiring is now narrowed to delivery of authored catalogues and authentication of the trusted hash at the host boundary.

Authorized instability-view protocol slice (2026-10-12): `WorldRevisionServer` now accepts authenticated `instability_view` queries with request IDs and chunk coordinates, and responds only with coordinates plus reward/danger bands from the World's public view API. Its request cache preserves the queried result for exact retries and rejects changed-payload reuse. `WorldRevisionValidator` now covers the response's redacted shape, exact retry, and altered-payload rejection. Strict MinGW syntax compilation passes for the server and validator; `git diff --check` passes. The network-level validator has not been executed, and no client UI consumes this response yet.

Instability-view visibility gate (2026-10-12): Added a distinct host callback for authorizing each `(actor, Loom, chunk_x, chunk_z)` view request. The protocol denies queries when the callback is absent or denies the requested chunk, so the general Loom identity/access authorizer cannot accidentally bypass map exploration/fog policy. Validator source now exercises a hidden-chunk denial alongside public-field redaction, exact retry and changed-payload rejection. Strict MinGW syntax compilation and `git diff --check` pass; runtime protocol validation and a production host callback remain open.
The visibility-gate validator also rejects signed-coordinate overflow before invoking the host visibility policy, preventing out-of-range wire coordinates from wrapping to a different chunk. The updated protocol and validator still pass strict MinGW syntax compilation and diff checks; runtime remains unverified.

World Loom safe-zone anchor (2026-10-12): Added `World::configure_loom_station` as a compare-and-set host API. It stores a dedicated station chunk anchor, protects that chunk and its eight neighbors regardless of manual protection edits, removes selected chunks in the newly protected square, and rejects anchor changes while a revision is pending or generating. World Revision metadata now writes v3 with the station configured flag and signed chunk coordinates; v1/v2 readers remain supported and v2 does not overwrite an existing runtime anchor. The World Revision validator source checks the protected footprint, metadata roundtrip and active-draft rejection. A focused MinGW runtime smoke passed v3 signed-coordinate roundtrip and v2 migration (`build=0 run=0`); strict syntax compilation and `git diff --check` pass. Station block identity/position discovery, permission and reach checks, claims, and active-session removal policy remain open.
World Loom station prerequisite (2026-10-12): Network `begin` now fails with `FT_ERR_CONFIGURATION` until a host-configured station anchor exists, so a revision draft cannot start with an undefined Loom safe zone. `World::has_loom_station` exposes the saved runtime state; the server validator source checks fail-closed begin and successful begin after host anchor configuration. Station block identity validation, actor reach/claim authorization, and runtime verification remain open.
Station-zone geometry was also exercised in a focused MinGW runtime smoke: the center and all tested edge/corner cells at radius one are protected, the next diagonal cell is outside, and extreme signed coordinates do not overflow. Result: `build=0 run=0`.

Ultimate Greed runtime slice (2026-10-12): Rare card definition 12 is now registered and maps to its physical inventory item. Its targetless play intent validates through `RegenerationWorldRules`; the host then requests exactly three cards with zero compulsory discards, using the existing draw policy for hand overflow and empty-deck notices. Draw authorization is bound to the originating card intent, caches exact retries, and rejects mismatched draw parameters. World Loom finalization pauses while a draw notice is queued, allowing the host to deliver and acknowledge overflow/empty-deck costs before generation. The selected chunks receive a separately configured Ultimate Greed instability delta, preflighted against the world maximum and applied with snapshot rollback if draw/checkpoint or adjustment fails. The rules manifest hashes this optional policy while retaining the prior manifest sequence when it is unset. Strict MinGW syntax compilation passes for the touched session, persistence, rules, GameSession and validator units. This slice is not runtime-tested; complete transaction runtime coverage and surfaced card text/UI remain open.

Biome Rupture rules slice (2026-10-12): Rare definition 13 is registered and importable. It requires two distinct in-range biome IDs; authoritative rules remove both selected biomes in one idempotent intent, allow already-excluded choices as no-ops without substitution, and reject any choice that would remove every remaining viable biome. The card has a separate configured instability delta included in the rules manifest and applied through the GameSession rollback path. GameSession play responses now include a two-bit no-effect choice mask (primary/secondary), cached and persisted in RCS1 v5; readers retain compatibility with RCS1 v1-v4. Rules reject response-only metadata on inbound authoritative intents. Validator source covers successful two-biome removal, no-op without substitution, extra-metadata rejection, and last-viable-biome rejection. Strict MinGW syntax compilation and `git diff --check` pass. Runtime validation and presentation of the mask in client UI remain open.

Protection-reason query (2026-10-12): Added a read-only `World::get_revision_protection_info` view that independently reports generation protection, the Loom safe zone, manual protection, and occupied-player exclusion. The World Revision validator source checks Loom/manual/occupied reasons and the persisted station reason after reload. Strict MinGW syntax compilation passes; the full World Revision runtime validator remains unexecuted.

Protection reasons in preview rendering (2026-10-12): `RevisionPreviewEntry` now embeds the protection-reason flags; GameSession forwards them to `RenderDebug`, whose 13x13 overlay uses distinct purple Loom and orange occupied-player colors while retaining red for manual/world protection. RenderDebug copy/clear paths include the reason plane. The World Revision validator source checks a manually protected reason survives into preview entries. Strict MinGW syntax compilation passes for the world/session/renderer/validator path, and `git diff --check` passes. This is debug/preview rendering only; a production World Loom map UI and runtime visual validation remain open.

Station access authorization boundary (2026-10-12): `World` and `WorldRevisionManager` now expose the configured Loom station chunk anchor. `WorldRevisionServer` requires both the trusted client authorizer and station-access authorizer at initialization; station access is checked for network draft begin, quotes, every draft action, and exact action retries. The callback receives actor and Loom IDs, the current station chunk, and the full request so the game host can verify current station identity, reach, claims, and permissions. Missing callbacks fail closed before the network server starts. The World Revision validator fixture supplies a strict actor/Loom/anchor authorizer. Strict MinGW syntax compilation passes. The application has not yet bound a production authorizer, and station block identity/position, claim lookup, active-session revocation, and network runtime coverage remain open.

Uncommitted draft shutdown cleanup (2026-10-12): `WorldRevisionServer::destroy` now cancels its owned pending draft before dropping ownership, preventing a stopped/disconnected server instance from leaving selection state pending indefinitely. Shutdown returns network teardown or draft-cancellation errors and retains draft ownership/cache state if cancellation fails, allowing the caller to observe and retry cleanup. A start already accepted by the host commit callback is retained for the GameSession decision/finalization window. Strict MinGW syntax compilation passes. Runtime disconnect/restart coverage, crash recovery of draft state, and production disconnect policy remain open.

Exact World Loom anchor slice (2026-10-12): Added a host API to configure the station by signed block coordinates and expected block ID; configuration checks the loaded live block before storing the anchor. Chunk coordinates are derived with floor division (including negative world coordinates), the safe zone follows that chunk, and the exact block anchor is persisted in World Revision metadata v4; v1-v3 remain readable with the station block identity unset. The station access callback now receives exact anchor state, and the server checks that the live block at the saved position still matches the expected ID before allowing quotes or draft actions. Local block breaks and authoritative block edits invalidate the anchor when they remove or replace that exact block; an uncommitted draft is cancelled as part of invalidation. Strict MinGW syntax compilation passes for manager, facade, metadata, server, block-edit and validator units. The game still needs to place/register the Loom block and configure this anchor from the authoritative placement path; claim/reach provider wiring and runtime anchor migration checks remain open.

World Revision metadata session wiring (2026-10-12): GameSession initially used a seed-specific `.wrv` sidecar. The follow-up below supersedes that persistence format for new saves.

World Revision metadata embedded in GameSession snapshots (2026-10-12): Added byte serialization/deserialization for World Revision metadata while retaining the existing `.wrv` codec and v1-v4 readers. GameSession snapshot version 10 appends the bounded metadata payload under the snapshot checksum and atomic replacement; startup restores the embedded state and falls back to `.wrv` only when loading an older snapshot without embedded metadata. New saves no longer update the sidecar, so World Revision state and the other GameSession payloads share one atomic file replacement. Strict MinGW warning-as-error syntax compilation passes for the metadata store, manager, facade, persistence and GameSession translation units. Runtime save/load, compatibility fixture and injected crash/failure validation remain open; atomic replacement does not by itself promise durable media flush or recover a generation already published outside this file.

Fortune's Draw and unique physical card crafting (2026-10-12): Added card definition 14 with a no-choice, one-card draw effect routed through the existing draw policy, host-intent sequence binding, retry cache, and persisted adapter state. Its Card Press recipe is registered as recipe 2019 and remains discovery-locked; item/recipe coverage now includes all 19 card definitions. Card recipes now commit through the host physical-ID allocator, giving each crafted card a unique instance ID. The draw adapter now sets pending discard count from the requested obligation, so draws with no discard requirement do not create one implicitly. Six touched C++ units pass strict MinGW warning-as-error syntax compilation. Runtime card, recipe, retry, and snapshot validation remains outstanding; other rare world-feature effects remain unimplemented.

Snapshot trailing-payload offset correction (2026-10-12): Review of snapshot v10 found that journal, card, rules, and instability readers derived their ranges from the end of the payload without accounting for all newer trailing sections. The loader now subtracts each trailing section and copies exact declared lengths, covering v8-v10 layouts and avoiding metadata bytes leaking into earlier opaque payloads. `GameSessionPersistence.cpp` passes strict MinGW warning-as-error syntax compilation. Runtime round-trip and compatibility coverage remain unverified.

World rules-hash provenance correction (2026-10-12): Successful regenerated chunks previously stored the effective Voxel configuration signature in the instability record's `rules_hash` field. That lost accepted card-intent provenance whenever an overlay changed a rule without changing the generated configuration. `WorldRevisionManager` now computes a domain-separated FNV-64 hash over the serialized `RegenerationWorldRules` payload at revision start and on pending-rule updates, and chunk publication records that value. Revisions using the config-only overload hash its configuration signature under a separate domain. Strict MinGW warning-as-error syntax compilation passes for the manager and result-applier units; `git diff --check` passes. Runtime generation-publication validation remains outstanding; this records provenance but does not make feature plans active or make multi-chunk publication atomic.

Surveyor's Reach scope integration (2026-10-12): `RegenerationWorldRules` now exposes the ordered target chunk coordinates from its accepted typed-intent journal and rejects duplicate target coordinates during preflight and replay. Updating a pending World Revision stages and validates the complete target set against loaded chunks, manual/Loom/occupancy protection, and the configured selection cap before publishing the updated rules config and bonus-chunk selection. GameSession no longer selects the same target a second time; its existing card/rules checkpoint rollback removes the bonus selection when a save fails. Rules-owned targets cannot be manually unselected. Occupancy/protection changes may remove a target from the visible selection, and regeneration revalidates/re-adds it only when eligible; start fails while a required target remains unsafe or unloaded. Strict MinGW warning-as-error syntax compilation passes for the rules, manager, regenerator, and GameSession units. Runtime validator execution remains outstanding; cost debit, chunk reservations/barriers, and durable multi-system commit remain open.

Surface Magma generation slice (2026-10-12): Added built-in voxel block ID 64 for lava, with liquid, non-replaceable, light-emitting metadata. Minecraft's built-in item policy maps the block to a lava bucket, and the renderer maps it to the volcanic-rock atlas tile with a lava debug tint. The item-catalog validator now explicitly checks the lava-block-to-lava-bucket policy and the lava bucket's world-acquisition/max-stack policy; strict MinGW warning-as-error syntax compilation passes. A direct runtime smoke built with the current `game_data_catalog.cpp` passes catalogue initialization, all 65 built-in coverage entries, the lava-block mapping and lava-bucket stack/acquisition checks. The full `--validate-item-catalog` command has not run. `RegenerationWorldRules::copy_feature_plans` exposes ordered Village Charter and per-pool Surface Magma intents as typed plans keyed by host-intent sequence and ordinal. Pending revisions freeze those plans and distribute them deterministically across selected chunks; the worker places each magma plan as a contained 3-by-3 lava pool with a 5-by-5 volcanic-rock rim before building lighting and the mesh, allowing a one-block surface-height variation by raising the lower footprint with volcanic rock. Successful chunk publication records deterministic per-plan outcome IDs and feature-kind versions atomically with its generation revision and rules hash in the instability record. The World Revision validator source checks the generated pool footprint, combined generation/outcome publication, and that invalid outcomes leave the record unchanged. Strict MinGW warning-as-error syntax compilation passes for the validator, pipeline, request builder, manager, regenerator, worker, instability store/facade and result applier, and `git diff --check` passes; the validator has not been run because the Make runner crashes during startup. The worker searches generated terrain before placement; there is no preflight before consuming the card or durable cross-chunk footprint reservation, and a chunk with no suitable clear 5-by-5 site fails generation. Regeneration start now rejects unsupported Village Charter plans before queueing chunks, though the card intent can already have been accepted; Village placement and catalog save-compatibility checks remain open.

Village Charter generation slice (2026-10-12): Village plans now reach the worker and produce two log-and-leaf cottages and a coarse-dirt path in a 9-by-9 block footprint wholly contained in their assigned chunk. Terrain is leveled with generated dirt, and each footprint is checked before placement. Village plans are assigned to distinct selected chunks; start rejects the revision before queueing work if there are too few chunks. GameSession reads indexed pending host intents to count queued Village Charter plans and rejects an additional play before card consumption when the applied plus pending plans would exceed the selected chunk count; both Village Charter and Surface Magma play also fail before consumption when no revision chunks are selected. Since each village fits within one chunk, its geometry is committed with that chunk and cannot be split across chunk publication. The World Revision validator source now generates a village plan and checks both cottage layouts, and the Regeneration Card validator checks pending-intent counts before and after acknowledgement. The Libft host-intent API/test, adapter, GameSession, worker, regenerator, and validators pass strict MinGW warning-as-error syntax compilation. Runtime validation did not run: `mingw32-make -j1 debug` still fails when Git Bash cannot create its MSYS named object (`NtCreateDirectoryObject ... 0xC0000022`), and a custom worker smoke link against the stale Libft archive has unresolved `ft_nullptr_instance`/duplicate `voxel_data` symbols. Village IDs, storage, NPC spawn metadata, explicit footprint reservations, terrain-dependent site feasibility before card play, persistence migration and runtime validator execution remain open.

Cross-container physical-ID save validation (2026-10-12): `GameSessionPersistence::save_file` now calls `WorldItemStore::validate_instance_id_uniqueness` before serializing, so an ID duplicated between the player inventory and ground-item store cannot be committed into a session snapshot. The WorldItemStore validator creates that duplicate, confirms save returns `FT_ERR_ALREADY_EXISTS`, and confirms the existing snapshot file remains byte-for-byte unchanged. Persistence and validator pass strict MinGW warning-as-error syntax compilation; runtime execution remains unverified while the project Make runner fails during Git Bash startup. A shared monotonic allocator across all runtime item creation paths and atomic synchronization against concurrent inventory/world-item mutations remain open.

Fluid duplication guard (2026-10-12): The generic selected-block interaction now rejects water and lava placement, and the old water spread helper that created up to 150 cells without consuming a bucket has been removed. CameraInteractionValidator source checks both fluid IDs return `FT_ERR_PERMISSION_DENIED` and leave the target cell unchanged. Strict MinGW warning-as-error syntax compilation passes for the interaction and validator units. Runtime validation is outstanding. Source pickup, filled-bucket pour, revision/reach validation, inventory exchange, and durable world/inventory coordination remain unimplemented.

Runtime craft-ID high-water synchronization (2026-10-12): Before applying a journaled hand craft or replaying a prepared craft, `WorldItemCraftTransaction` advances the recipe catalog's output-ID counter beyond every physical ID currently present in the character inventory and ground-item store. The WorldItemStore validator adds a new high-ID ground item after restoring a prepared craft, recovers the transaction, and checks the counter advances to 12001 without colliding with that item. Catalog, transaction, persistence, and validator pass strict MinGW warning-as-error syntax compilation; runtime validator execution remains blocked by the Make runner startup failure. The remaining host-wide minting API and synchronization with concurrent item mutations are open.

GameSession snapshot compatibility coverage (2026-10-12): `WorldItemStoreValidator` now explicitly writes/checks v1 and v2 through the base and recipe-book overloads, constructs v3/v4 fixtures from v5 data, and checks v5-v10 formats through the latest loader. It verifies the expected trailing sections are preserved when present and empty for older versions; v10 round-trips card/rules/instability payloads plus real serialized World Revision metadata. A corrupted v10 checksum and a version-11 header with a recomputed valid checksum must each leave all four output vectors and the live character unchanged. Strict MinGW warning-as-error syntax compilation passes. A directly linked Windows MinGW smoke against current persistence, item-store, catalog, journal, metadata codec, and crafting sources confirms valid-checksum v11 rejection and rollback (`session-future-version: valid-checksum rejection and rollback ok`). The full WorldItemStoreValidator remains unrun, and synthesized v3/v4 fixtures do not replace archived real-world compatibility saves.

Volcano Mark implementation update (2026-10-12): The 256-chunk World Revision hard ceiling and default generation queue permit the minimum footprint. Before card consumption, GameSession now requires enough distinct selected chunks, a contiguous selected 9-by-9 square made entirely of paid chunks, and capacity for volcano and Village Charter reservations. World exposes a paid-selection snapshot so free bonus chunks cannot reduce the volcano footprint's normal dust price; the existing read-only start quote reports the paid count/cost. Revision start deterministically chooses the first unreserved paid square and attaches the same anchor/9-chunk width to all 81 chunk requests. The worker derives terrain from global coordinates and builds a volcanic cone, crater lake with rock containment, an arena shelf, and a connecting passage, preserving the profile across chunk seams. Playing the card now requires the rare Volcano Mark recipe to be known; the catalog exposes a host source-registration boundary, and ItemCatalogValidator source checks the blueprint recipe's locked state and checkpointed discovery grant/rollback. `WorldRevisionValidator` source checks the lake boundary, passage, arena support and slope continuity across three generated chunks. Strict MinGW warning-as-error syntax compilation passes for the affected application, recipe, revision-selection, pipeline, worker, and validator units. Runtime validation could not run: `mingw32-make -j1 debug` fails when Git Bash cannot create its MSYS named object (`NtCreateDirectoryObject ... 0xC0000022`), and WSL launch is denied (`Wsl/E_ACCESSDENIED`). The exploration/research source binding, configured larger footprints and feature-highlight preview, full support/lighting/committed-manifest validation, authoritative boss gating and lifecycle, production instability configuration, and runtime validation remain open; the volcano is still incomplete.

GameSession v10 direct runtime smoke (2026-10-12): Linked the current `GameSessionPersistence.cpp`, WorldItemStore, item/recipe catalogs, transaction journal and metadata codec against `Full_Libft.a`. The harness passes v10 save/load with all four opaque subsystem payloads containing valid World Revision metadata, then corrupts the outer checksum and verifies all output vectors remain unchanged (`session-v10-smoke: roundtrip and corruption rollback ok`). This directly validates the snapshot codec path; the full WorldItemStoreValidator, compatibility versions 1-9, and production `GameSession` startup migration path remain unrun.

Fluid-container metadata slice (2026-10-12): `MinecraftItemCatalog` now exposes validated capacity/fluid-kind rules for the empty, water-filled, and lava-filled bucket definitions. Catalog initialization cross-checks these rules against registered content IDs and stack limits, and `ItemCatalogValidator` source checks all three mappings. The affected catalog and validator pass strict MinGW warning-as-error syntax compilation, and `git diff --check` passes. This is definition metadata only: bucket instance persistence, source/flow cell state, reach and revision checks, inventory exchange, durable world/inventory transaction recovery, and runtime validator execution remain open.

Fluid source/flowing block identities (2026-10-12): Preserved water source ID 8 and lava source ID 64, and appended flowing-water ID 65 and flowing-lava ID 66 with liquid metadata, registry names, and matching fluid textures. The Voxel generator version is now 8 so the changed registry manifest is explicit. New `voxel_block_is_fluid_source` and `voxel_block_is_flowing_fluid` helpers classify the IDs. Flowing cells have explicit no-drop rules; generic place and delete commands plus the authoritative block-change API reject all liquid blocks, and the camera interaction guard rejects direct placement of any liquid. Item-catalog, camera, and authoritative-edit validator source covers these mappings and rejections. Strict MinGW warning-as-error syntax compilation passes for the modified Voxel, catalog, renderer, edit, world, interaction, and validator units; `git diff --check` passes. No simulation currently emits flowing cells, and source pickup/pour, bucket-instance state, reach/revision checks, durable block edits and runtime validation remain open.

World Revision metadata v2/v3 migration fixtures (2026-10-12): `WorldRevisionValidator` now derives valid v2 and v3 checksummed records from a v4 serialization, removing only the version-specific station fields, and checks revision/policy/protection results plus the expected station migration defaults. Strict MinGW warning-as-error syntax compilation and `git diff --check` pass. A directly linked Windows MinGW smoke against the current `WorldRevisionMetadataStore.cpp` and `Full_Libft.a` passes both downgrade/restore cases (`world-revision-metadata-migration: v2-v3 ok`). The full validator remains unrun; v1 remains covered by the existing file fixture and v4 by the normal current-format roundtrip.

Revision-checked fluid source edit primitive (2026-10-12): `World::replace_fluid_source_at` now allows only source-to-air pickup and air-to-source placement, and checks both the expected current block and chunk voxel revision before mutation. It updates the block override/read state and schedules lighting/mesh work. `CameraInteractionValidator` source covers a valid source placement/removal, rejects flowing-water placement, and verifies a stale revision leaves the source unchanged. Generic delete/place and authoritative block-change paths reject liquids. Strict MinGW warning-as-error syntax compilation passes for the world editor/facade and affected validators, and `git diff --check` passes. This primitive is not yet connected to inventory, reach checks, durable request replay, or a cross-system save transaction; fluid edits also do not yet survive chunk unload/reload.

World fluid override persistence (2026-10-12): Superseding the opaque-payload limitation above, `World` now maintains bounded source-cell overrides, serializes them as checksummed `FLS1`, and applies them to newly generated chunk contents before the chunk read state is published, then schedules remesh work. Overrides stay ordered by chunk so chunk restore scans only that chunk's records, and corrupt duplicate coordinates are rejected after an `O(n log n)` sort. `GameSession` restores this world state from v11 before chunk streaming and serializes it on snapshot save. `CameraInteractionValidator` covers source mutation, state roundtrip, checksum rejection, and unchanged state after rejected restore; `WorldItemStoreValidator` covers v11 snapshot payload roundtrip/corruption rollback and rejects version 12. Strict MinGW warning-as-error syntax compilation passes for the changed world, publication, session, and validators. Runtime validator execution remains unverified. Inventory exchange, reach checks, durable request replay, atomic world/inventory journal coordination, and flowing-fluid simulation remain open.

Session bucket transfer API slice (2026-10-12): `GameSession::transfer_fluid_bucket` now raycasts from the authoritative session camera, checks the exact source/place cell, chunk voxel revision, inventory revision, a 10-block reach bound, and World Revision chunk protection, then stages bucket conversion in an immutable inventory snapshot. It updates source state through `World::replace_fluid_source_at`, records exact request parameters in the shared journal, prunes the transfer scope to its latest committed request, and atomically saves the resulting inventory/world state in snapshot v11. The latest exact retry returns success and reusing its ID with a different payload is rejected; older retries fail their stale inventory revision after their record is pruned. In-memory participants roll back if the mutation or snapshot save fails. Strict MinGW warning-as-error syntax compilation passes for the session and affected world/validator units. Runtime action tests remain unrun; the API is not wired to an input/UI slot selector, bucket instance IDs are not minted for split stacks, durable multiplayer claim authorization and runtime action tests remain open.

Bucket inventory snapshot planner (2026-10-12): Extracted the item exchange into `MinecraftItemCatalog::prepare_fluid_transfer_snapshot`, which validates empty/filled bucket kind and capacity, stages stack splitting or singleton conversion, enforces inventory capacity, increments the captured revision, preserves identified bucket instance IDs, and leaves caller output unchanged on failure. `ItemCatalogValidator` now covers a multi-bucket split, full-inventory rejection, unique-ID fill, and pour back to an empty bucket. Strict warning-as-error syntax compilation passes for the planner, GameSession caller and validators. Runtime validator execution remains unverified; physical ID minting was completed in the subsequent entry, while the full world/inventory retry matrix remains open.

Physical ID minting for split buckets (2026-10-12): Split bucket fills now allocate a nonzero physical item ID through `MinecraftRecipeCatalog` after advancing its high-water mark beyond IDs in the player inventory, ground-item store, and imported-card links. The planned filled bucket is single-instance and stack-size 1; fill/pour of an already identified bucket retains its ID. Hand crafting also advances the recipe allocator above imported-card physical IDs before it mints recipe results. Failed transfer actions restore the allocator counter, while successful saves persist the minted ID in the inventory snapshot; the next session derives its counter from persisted instances. ItemCatalogValidator checks a supplied nonzero ID on split output; strict warning-as-error syntax compilation passes for catalog, recipe allocator, card session, GameSession and validators. Runtime tests remain unverified. The later allocator-concurrency entry covers reservation locking within one catalog; a host-wide allocator shared with every item-creation/import path remains open.

Playable-card preview metadata (2026-10-12): `RegenerationCardSession::get_card_preview` now returns static title, concise effect description, required choice kind, and choice count for every currently supported card definition. The regeneration validator checks every supported definition has complete metadata, checks the two-target Biome Rupture count, and verifies unsupported definition 8 is rejected without changing caller output. Strict MinGW warning-as-error syntax compilation passes for the session and validator. Runtime validation remains unverified; seven craftable rare card items still lack supported card definitions/effects and therefore are not covered by this preview catalog.

Session physical-ID high-water synchronization (2026-10-12): `GameSession::synchronize_item_instance_id_allocator` now derives the recipe catalog's next ID above physical IDs in character inventory, ground items, and imported-card links. Both hand crafting and split-bucket pickup use this same synchronization path before minting; WorldItemCraftTransaction retains its inventory/ground conflict check. Strict MinGW warning-as-error syntax compilation passes for GameSession and the card/catalog units. Runtime validation remains blocked by the Git Bash MSYS object-creation failure.

Recipe allocator concurrency (2026-10-12): `MinecraftRecipeCatalog` now protects output-ID allocation, high-water synchronization, restoration, reads, and unique-craft commits with one mutex. Reservations and commits within one catalog instance cannot race to mint the same ID. `ItemCatalogValidator` source now reserves 512 IDs from eight concurrent threads and checks all are nonzero and unique. GameSession also serializes hand craft, fluid transfer, card import/export, and ground pickup across the container-scan/mutation window. Strict MinGW warning-as-error syntax compilation passes for the recipe catalog, GameSession, and affected validators. Runtime validation is still unavailable through Make; direct mutation through exposed WorldItemStore/catalog references and separate catalog instances/hosts remain outside this session boundary.

Volcano rules restore fix (2026-10-12): `RegenerationWorldRules::deserialize` now copies the replayed Volcano plan count from its validated staging object into the live object. The existing regeneration validator checks that both direct deserialize and file load restore the Volcano feature plan, alongside village/magma/target counts. Strict MinGW warning-as-error syntax compilation passes for the rules and validator, and `git diff --check` passes. Runtime execution remains unverified because Git Bash cannot create its MSYS named object in this environment (`NtCreateDirectoryObject ... 0xC0000022`); the Volcano lifecycle and other rare-card effects remain incomplete.

World Revision identifier exhaustion (2026-10-12): Both World Revision begin paths now reject `UINT32_MAX` before changing selection/configuration state, preventing a successful finish from wrapping the revision identifier. `WorldRevisionValidator` source checks both overloads reject the exhausted ID while leaving the manager non-pending and its selection revision unchanged. Strict MinGW warning-as-error syntax compilation and `git diff --check` pass; runtime validator execution remains unavailable because the Make wrapper's Git Bash process cannot create its MSYS named object (`NtCreateDirectoryObject ... 0xC0000022`).

Volcano Mark instability application (2026-10-12): `GameSession::apply_pending_regeneration_card_intent` now includes Volcano Mark in the committed instability adjustment and snapshot rollback path. Previously its configured delta was checked and snapshotted but omitted from the mutation branch. Strict MinGW warning-as-error syntax compilation passes for GameSession; full session-level runtime validation remains unavailable through Make, and other Volcano lifecycle gates remain open.

Recipe discovery authority boundary (2026-10-12): `GameSession` now requires a host-installed authorization callback before granting any recipe discovery event; the callback is configured before session start, and grant requests fail closed when it is absent or rejects the exact source/event tuple. Untrusted clients cannot register discovery mappings through GameSession; source mapping remains a trusted catalogue operation. `ItemCatalogValidator` source checks null, zero-ID, mismatched-event rejection and exact-event acceptance. Strict MinGW warning-as-error syntax compilation passes for GameSession and the validator; runtime validator execution remains unverified, and authenticated multiplayer transport must provide the callback.

Script catalogue host authorization (2026-10-12): `GameSession::configure_regeneration_script_catalog` now requires an independently installed host authorizer to approve the exact bounded catalogue bytes and expected manifest hash before storing them. The authorizer must be configured before start and fails closed when absent; the callback owns the responsibility to compare against the authenticated content manifest. `ItemCatalogValidator` source checks null authorizer, null bytes, zero hash, mismatch rejection and exact approval. Strict MinGW warning-as-error syntax compilation passes for GameSession and the validator. Runtime execution and production host provisioning remain open.

Timed processing station authority boundary (2026-10-12): `GameSession::start_recipe_processing` now requires a pre-start host authorization callback before it can start or replay a furnace job. The callback receives the furnace station ID, exact signed block coordinates, caller-captured station revision, and current session player position so the host can verify live block identity/revision, reach, claims, and permissions. Missing authorization, a zero station revision, or a rejected station returns permission denied before the transaction journal or inventory changes. The version-2 processing journal payload binds those station coordinates and revision to the request; recovery continues to read version-1 prepared records. The local application now configures a single-player host callback before world startup; it checks the exact Furnace voxel, current chunk revision, protected-zone status, and six-block reach. The camera-interaction validator source covers acceptance and rejection for wrong station, zero/stale revision, excessive distance, a protected chunk, and a removed Furnace. Multiplayer hosts still need their authenticated claim/permission provider, and no station UI or runtime processing/recovery validation exists yet. Strict MinGW warning-as-error syntax compilation passes for GameSession, ApplicationPhaseController and CameraInteractionValidator.

Recipe knowledge boundary (2026-10-12): Removed the mutable `GameSession::recipe_catalog()` accessor and replaced it with a const view. Callers can query recipes but cannot invoke `unlock_recipe()` through the session to bypass the authorized `grant_recipe_discovery()` transaction and its checkpoint/rollback behavior. Repository search found no mutable accessor call sites. Strict MinGW warning-as-error syntax compilation and `git diff --check` pass. Runtime API validation, actual discovery event consumers, and multiplayer authorization provisioning remain open.

Recipe discovery source provisioning (2026-10-12): Added bounded pre-start `GameSession::configure_recipe_discovery_source` for trusted hosts. Startup installs configured `(source type, source ID, recipe ID)` mappings after catalog initialization and before loading saved recipe-book state. `MinecraftRecipeCatalog` now permits a source mapping for an already-known recipe, preserving source bindings across saves; granting a new event for that recipe remains idempotent. `ItemCatalogValidator` source covers known-recipe mapping and grant behavior, zero-ID rejection, duplicate/conflicting mappings and the 4096-source cap. Strict MinGW warning-as-error syntax compilation passes for GameSession, recipe catalog and validator. Actual exploration/loot/research event consumers, runtime execution and authenticated multiplayer host wiring remain open.

Card cross-catalog validation (2026-10-12): `ItemCatalogValidator` now checks all 19 card recipe outputs against their CardGame item-to-definition mapping, single-stack acquisition policy, default recipe knowledge state, and supported effect/preview metadata. The seven rare items without implemented effects must remain unmapped and discovery-locked. Effect and item-to-definition lookups are exposed as read-only catalog queries. Strict MinGW warning-as-error syntax compilation and `git diff --check` pass. Runtime validator execution and coverage of newly enabled rare cards remain open.

Furnace block identity (2026-10-12): Added built-in voxel ID 67, `voxel:furnace`, with solid/breakable metadata, a stone atlas texture, and a block-acquisition rule that drops the existing recipe-acquired Furnace item. The voxel generator version advances to 9 for the registry change. `MinecraftItemCatalog::resolve_placed_block_item` now resolves self-item block definitions and the Furnace item to the exact placed block ID; the item validator checks this Furnace mapping, block rule and metadata. The existing dynamic builtin-coverage check includes the appended ID. Strict warning-as-error syntax compilation passes for voxel registry/data, item catalog, renderer texture registry, and validators. Revision-checked inventory consumption/world placement, a station UI, production authorization wiring, and runtime catalog validation remain open.

Persistent world-cell override foundation (2026-10-12): The checksummed session world-cell payload now writes format version 2, retaining its version-1 reader for fluid-only saves while accepting any known non-flowing block state in version 2. Added a revision-checked arbitrary block replacement path that stages its persistent override before mutating the loaded chunk, plus a state-only override setter for transaction recovery before the target chunk streams in. The same chunk publication hook reapplies these saved block states and remeshes affected chunks. `CameraInteractionValidator` source now checks a furnace block roundtrip through v2 and restoration to the prior air override. Strict MinGW warning-as-error syntax compilation passes for the validator, editor, world facade, generation committer and GameSession. Ordinary player block edits still do not all enter this saved override table, and runtime migration/recovery validation remains open.

Inventory-backed block placement (2026-10-12): `GameSession::place_inventory_block` now raycasts the requested target from the authoritative camera, checks usable distance, collision, target air state, loaded chunk revision and World Revision protection, maps only registered placeable items, and stages a revision-checked single-item inventory decrement. A versioned journal request binds slot/item/block IDs, inventory revision, world coordinates and voxel revision. The item debit, persistent block override and committed journal are saved together; failed mutations restore the inventory, world cell and prior journal. Startup recovery reapplies prepared placements to the persistent cell override before chunk streaming, then consumes the item and commits the journal. Secondary mouse placement now uses this transaction and the first matching inventory stack for the selected block; there is not yet a hotbar or inventory-slot selection UI. The local application configures the single-player processing-station authorizer before startup, as recorded in the station authority entry above. Multiplayer claim/permission integration, ordinary non-inventory block edit persistence, and runtime action, retry, injected-save-failure and crash-recovery validation remain open.

Processing station binding (2026-10-12): Active processing jobs now retain the exact signed furnace coordinates and captured station revision from their prepared request. Processing-state format v2 persists that context, and the v1 reader retains legacy jobs with an absent revision so they remain visible but fail closed during tick authorization. `GameSession::tick_world` checks each queued job with a distinct continuity validator before recovering or advancing a shared processing batch; this validator verifies station presence and ongoing access without imposing interaction range. Starting a job still uses the separate authorizer that checks reach and the captured chunk revision. The local continuity validator checks the exact Furnace voxel and protected-zone status without treating unrelated edits in the containing chunk as station removal; network hosts must install a validator that also checks current claims and permissions. If any station is rejected, the batch's elapsed processing time is discarded until authorization succeeds. This pauses all jobs together because the scheduler advances them in one transaction. Strict MinGW warning-as-error compilation passes for the catalog, transaction, session, and validator units. A focused station-context persistence assertion, runtime roundtrip, legacy-v1 migration behavior, independent job scheduling, and production multiplayer continuity policy remain open.

Station-scoped recipe query (2026-10-12): `MinecraftRecipeCatalog::query_recipe_at_station` now exposes the existing station-aware Libft query for any registered station ID. `GameSession::query_station_recipe` resolves against the live inventory only after the host authorizer approves the exact station block/revision and current player reach, so a caller cannot preview a workstation recipe by supplying a client-side station flag. This is query groundwork only: station craft transactions, station blocks other than Furnace, and a recipe UI remain open. Strict MinGW warning-as-error syntax compilation passes for the recipe catalog and session units.

Station craft transaction (2026-10-12): `GameSession::craft_station_recipe` now applies the same exact-station host authorization before submitting a craft. `WorldItemCraftTransaction` format v2 binds station ID, signed coordinates, and expected station revision into its prepared journal record; its recovery reader retains format-v1 hand crafts. `MinecraftRecipeCatalog` prepares/commits against the requested station and includes that context in craft action cache v2, retaining the v1 hand-only cache reader so exact retry cannot change station context. Strict MinGW warning-as-error syntax compilation passes for the catalog, transaction, session, and world-item validator units. Runtime journal recovery, cache migration, station authorization failures, workstation block placement, and crafting UI remain open.

Physical workstation blocks (2026-10-12): Appended built-in `voxel:workbench`, `voxel:paper_mill`, and `voxel:card_press` at IDs 68–70; the built-in count advances to 71 and generator version to 10. All three are solid/breakable, have atlas mappings and drop their existing recipe-acquired station item. Item-to-block placement resolution now maps those items to the corresponding block IDs. The local station authorizer resolves each station ID to its exact block, so the station query and craft APIs can be used with placed local workstations. Strict MinGW warning-as-error syntax compilation passes for the voxel registry/data, block texture registry, item catalog, GameSession, and ItemCatalogValidator. Runtime catalog validation, station craft/recovery validation, and the workstation UI remain open.

Configured Volcano Mark footprint (2026-10-12): Added a pre-start host setting
for a 9–16 chunk square footprint. The selected width is included in the
generation manifest when non-default, while width 9 preserves the earlier
manifest input. Card preflight, paid-chunk square search, revision reservation,
feature-plan transfer and worker geometry now use the configured width; the
worker scales its terrain profile across the footprint. Added a 12-by-12
worker-generation case to `WorldRevisionValidator`. Strict MinGW
warning-as-error syntax compilation passes for the rules, session, revision,
worker and validator units. Runtime validation is unverified because Git Bash
fails to create its MSYS named object (`NtCreateDirectoryObject ...
0xC0000022`). The configured-width setting must be supplied consistently by
the host when restoring a session; authoritative boss gating, feature preview,
full lighting/support checks and durable lifecycle validation remain open.

Volcano Mark progression authorization (2026-10-12): GameSession now requires a
host-installed authorizer for the exact card instance and active World Revision
before Volcano Mark can be consumed. The host binds its callback context to the
authenticated session owner and decides progression/boss eligibility; missing
or rejected authorization fails closed. The check follows the existing rare
blueprint-unlock gate and runs only on first execution, so accepted action
retries preserve their cached result. ItemCatalogValidator source covers missing
callbacks, zero IDs, denial, mismatched card/revision context and approval, as
well as callback configuration. Strict MinGW warning-as-error syntax
compilation passes for GameSession and ItemCatalogValidator. Runtime validator
execution remains blocked by the Git Bash MSYS named-object failure. The game
still has no authoritative boss-completion store or production provider wired
to this callback, so Volcano Mark remains unavailable unless the host supplies
that progression state.


Volcano configured-footprint preview (2026-10-12): RegenerationCardPreview now carries a reserved chunk-width field, and GameSession exposes a session-aware preview query that reports the configured 9-16 chunk width for Volcano Mark. Other cards report zero. The static card copy no longer claims that every world uses 9x9. Strict MinGW warning-as-error syntax compilation passes for the card session and GameSession translation units. Runtime/UI validation is still open; the feature highlight and actual renderer/client integration have not been implemented.
Volcano preview validator coverage (2026-10-12): ItemCatalogValidator now checks session-aware preview widths at 9, 12 and 16 chunks, rejects configurations of 8 and 17, and confirms non-footprint cards report zero. The three affected translation units pass strict MinGW warning-as-error syntax compilation; runtime validator execution remains unverified.
Build and preview validation update (2026-10-12): The card validator now also asserts the default static preview uses width 9 only for Volcano Mark and zero for all other supported cards. Strict MinGW syntax compilation passes for both validators, GameSession and RegenerationCardSession. Running mingw32-make through cmd.exe reproduced the Git Bash MSYS NtCreateDirectoryObject access-denied startup failure, so the runtime validators could not run.
Volcano light validation (2026-10-12): The 9x9 and configured 12x12 pipeline validators now require each generated volcano chunk to have a valid, current light buffer and be marked ready for rendering after feature placement. Strict MinGW warning-as-error syntax compilation passes for WorldRevisionValidator.cpp. Runtime execution remains unverified because Git Bash Make startup fails with NtCreateDirectoryObject access denied.
Volcano feature-map preview (2026-10-12): The revision debug map now highlights the exact paid chunk square reserved for Volcano Mark in cyan. Preview and generation call the same deterministic anchor helper; preview applies village reservations first, then uses paid selected chunks and the configured 9-16 width. WorldRevisionValidator source checks anchor ordering, village reservation and bonus-chunk rejection. Strict MinGW warning-as-error syntax compilation passes for all changed units, and a direct native MinGW smoke executable passed these anchor cases. Full application runtime/UI validation remains open because the Make runner fails at Git Bash startup.
Volcano preview framing (2026-10-12): The revision map now spans 17-by-17 chunks and recenters on the reserved volcano footprint while it is pending, so configured widths through 16 chunks fit entirely. The preflight square check now calls the same paid-square anchor selector used by preview and generation, returning allocation errors distinctly. Strict MinGW warning-as-error syntax compilation and diff checks pass for changed units.
Preview coordinate boundary validation (2026-10-12): `World::build_revision_preview` now performs center-plus-offset arithmetic in 64 bits and skips coordinates outside the valid chunk range. WorldRevisionValidator checks 17x17 previews at both int32 extremes to catch wrapped entries. GameSession maps preview coordinates using 64-bit offsets and validates array bounds. Strict MinGW warning-as-error syntax compilation and diff checks pass; runtime validator execution remains unavailable due the Make runner's Git Bash startup failure.
Volcano width-16 anchor verification (2026-10-12): WorldRevisionValidator now exercises 16x16 candidate squares, deterministic row/column ordering, village-reserved squares and bonus-chunk exclusion. The standalone native MinGW smoke executable passed the same width-16 anchor cases. The coordinate-boundary preview validator and 64-bit map index checks also compile under strict warnings; full validator runtime remains unverified.
Feature preview mask lifecycle (2026-10-12): WorldRevisionValidator now checks that RenderDebug copies preserve the 17x17 feature-highlight mask and that clear resets every cell. Strict MinGW warning-as-error compilation passes, and a standalone native MinGW smoke executable passed copy/clear checks. The complete application validator still cannot run through Make because Git Bash startup fails.

World Loom committed-request retry (2026-10-12): GameSession::commit_regeneration_start now checks the durable journal before requiring a pending, non-regenerating world revision. An exact retry of a committed request therefore remains successful after finalization begins, while mismatched actor, loom, revisions, opening-hand count, transaction type, or record format is rejected. Strict MinGW warning-as-error syntax compilation and git diff --check pass. Runtime retry validation remains open; World Revision selection/barrier restart persistence is still a separate lifecycle gap.

Non-cancellable World Loom boundary validator (2026-10-12): WorldRevisionValidator now exercises a successful host start commit followed by a cancellation request, asserting the request is rejected and the revision remains pending. The validator source compiles with strict MinGW warning-as-error flags. Runtime execution is still unavailable because the Make runner fails launching Git Bash with NtCreateDirectoryObject ... 0xC0000022. Restart persistence for pending selections/barriers remains open.

World Loom commit idempotency coverage (2026-10-12): The WorldRevisionValidator now replays an accepted confirm request, rejects the same request ID with a changed opening-hand count, and verifies that cancellation remains rejected afterward. Strict warning-as-error syntax compilation passes. Runtime execution remains unverified because Git Bash cannot create its MSYS named object (0xC0000022).

World Loom occupancy feed (2026-10-12): GameSession now accepts a pre-start host provider for all occupied player chunks, refreshes it during world ticks and immediately before a new start commit, and fails closed on provider errors. Binding the network WorldRevisionServer requires that provider; single-player sessions retain camera-derived occupancy. The item-catalog validator checks provider configuration rules. Strict warning-as-error syntax compilation passes. Runtime coverage of provider refresh/commit interaction and the full server validator remain unverified because the Make runner cannot launch Git Bash (NtCreateDirectoryObject, 0xC0000022). Pending selection/barrier restart persistence remains open.

World Loom movement barrier (2026-10-12): WorldRevisionManager now activates a reserved-chunk barrier after the GameSession start commit. PlayerCollision treats reserved chunks as solid for movement into them, permits escape for a player already overlapping the reserved area, and prevents cancellation while the barrier is active. Successful regeneration clears the barrier; generation failure retains the selected coordinates and barrier for safety. WorldRevisionValidator covers entry, escape, cancel rejection, and explicit clearing. All affected units pass strict MinGW warning-as-error syntax compilation. Runtime execution remains unavailable because Git Bash startup fails with NtCreateDirectoryObject 0xC0000022. Barrier visual rendering, network teleport/vehicle hooks, durable restart state, and repair workflow after generation failure remain open.

World Loom generation repair path (2026-10-12): Failed regeneration now retains its selected coordinates, feature plans, and active movement barrier. A trusted host can call World::retry_failed_revision_regeneration or GameSession::retry_failed_regeneration to reset the failed progress record and resubmit the same plan without another opening-hand deal or currency debit. Successful completion clears the barrier. Strict warning-as-error syntax compilation passes for the manager, regenerator, facade, GameSession, movement collision, and WorldRevisionValidator. Runtime recovery remains unverified; visible barrier rendering, durable barrier/plan restoration, and failure repair after process restart remain open.

Barrier status in revision map (2026-10-12): RenderDebug carries and copies the active barrier state, and DebugOverlayRenderer outlines selected cells in bright cyan and labels the map while the barrier is active. The validator checks that this state survives RenderDebug copying and clear resets it. Strict MinGW warning-as-error syntax compilation passes. This is a screen-space map indication only; a world-space glowing boundary mesh and renderer/runtime visual validation remain open.

Barrier map visibility (2026-10-12): GameSession now forces the revision map into the optional overlay while a committed barrier is active, even if the user had hidden the preview. It returns to the user's preview preference once the barrier clears. Strict syntax compilation and diff checks pass; this remains a screen-space cue, not the world-space glowing wall required by the design.

World-space barrier renderer (2026-10-12): The GPU renderer now draws additive cyan translucent wall panels along the external perimeter of selected chunks while a committed barrier is active. Shared edges between neighboring selected chunks are omitted; geometry is bounded by the 256-chunk cap and uses the existing camera MVP without changing camera state. Strict MinGW warning-as-error syntax compilation passes for GpuWorldRenderer.cpp. Runtime shader compilation, visual inspection, and the software-renderer equivalent remain unverified. Network teleport/vehicle hooks and durable restart recovery remain open.

World Loom committed-plan snapshot recovery (2026-10-12): The checksummed World Revision metadata payload now carries a committed barrier's selected chunk coordinates, paid/bonus flags, mode, stage mask, and exact selection revision inside the atomic GameSession snapshot. Startup restores the rules first, rebuilds the frozen revision plan from the active generation config, reattaches the saved selection, activates the barrier, and refreshes host occupancy. Occupancy that now overlaps the committed selection fails startup closed rather than silently deleting reserved chunks. WorldRevisionValidator adds an active-barrier metadata roundtrip case. Strict MinGW warning-as-error syntax compilation passes for manager, facade, GameSession, and both touched validators; runtime execution remains unverified. Decision-window deadline/server draft identity recovery, process-restart generation publication recovery, legacy standalone .wrv active-plan recovery, and validator runtime execution remain open.

Standalone World Revision lifecycle persistence (2026-10-12): save_revision_metadata and load_revision_metadata now use the same versioned checksummed lifecycle envelope as the atomic GameSession snapshot. The standalone file is replaced atomically; legacy metadata payloads still deserialize without an active plan. The WorldRevisionValidator restore scenario now saves and reloads the active barrier from a .wrv file before rebuilding it with the restored rules. Strict MinGW warning-as-error compilation passes for the affected manager, session, facade and validators. Runtime validation remains unverified. Authoritative deadline and network draft identity recovery, and crash-safe generation publication, remain open.


World Loom decision-window deadline persistence (2026-10-13): World Revision lifecycle metadata version 2 adds a host epoch-millisecond deadline while retaining version-1 reads. GameSession requires a pre-start restart-stable authoritative clock provider, sets a five-minute deadline at the committed start boundary, rechecks host time before finalization, and restores the deadline with the saved barrier. The WorldRevisionValidator restore case asserts exact deadline round-trip. Affected manager, facade, GameSession and validator units pass strict MinGW warning-as-error syntax compilation; runtime validator execution remains unverified. Legacy version-1 committed metadata has no deadline and currently follows immediate-finalization compatibility behavior. Network draft identity, atomic generation publication recovery, and other design sections remain open.

World Loom deadline scheduler clarification (2026-10-13): The active GameSession update loop now polls the configured host clock and starts finalization once the persisted deadline is reached, including after restart. Early attempts remain rejected; pending choices defer completion until deterministic expiry handling resolves them. The host must install a restart-stable monotonic clock provider before a committed session is accepted. Strict syntax checks and diff checks pass; runtime behavior is not yet verified.

World Loom deadline choice resolution (2026-10-13): At the five-minute session deadline, GameSession now resolves every still-pending CardGame choice to its validated deterministic default, regardless of a later per-choice deadline. The Libft choice ledger validates all defaults before mutating any choice, so a missing/invalid default leaves the whole pending set unchanged. Focused Libft tests cover early session-deadline defaults, idempotent repeat resolution, and all-or-nothing failure. Strict MinGW warning-as-error syntax compilation passes for the Libft engine/ledger, new test source, RegenerationCardSession, GameSession, WorldRevisionManager, and WorldRevisionValidator; tests were not run.

World Loom deadline mandatory-discard recovery (2026-10-13): Once pending CardGame effects and typed host notices have been handled, finalization now resolves any mandatory hand-discard obligations automatically. It chooses the lowest physical card instance ID, generates a deterministic collision-checked internal request ID in the deadline namespace, checkpoints the change, and rolls back the card snapshot if the checkpoint fails. The deadline path still waits for typed empty-draw/overflow notices to be handled by the host so their configured instability effects are not silently lost; integrating those notice costs into deadline recovery remains open. Strict MinGW syntax compilation passes for the changed GameSession and card-session units; runtime validator coverage is not yet added.

World Loom deadline discard validator coverage (2026-10-13): RegenerationCardValidator now restores a Double Draft snapshot with an outstanding mandatory discard and checks deadline resolution discards exactly the lowest physical instance, clears the obligation, and preserves expected hand/discard counts. Strict MinGW warning-as-error syntax compilation passes for the validator and affected card/session units. Runtime execution remains unverified.

Clock-provider startup gate (2026-10-13): Startup now fails configuration validation when it restores an active committed World Loom barrier without a host clock provider, avoiding a session that could never enforce its persisted deadline. The provider's restart-stability remains a host integration contract; no production multiplayer clock source is wired yet.

Draw notice instability integration (2026-10-13): RegenerationWorldRulesPolicy now carries optional empty-draw and hand-overflow instability deltas, and both nonzero values participate in the content manifest hash. GameSession exposes configure_draw_instability_deltas, bounds the amounts by the chunk-instability maximum, and includes them in restored rules validation. At the deadline, typed draw notices are now processed before acknowledgement: each configured event applies a saturating delta to every reserved chunk, then saves the updated card intent ledger and instability store together. Failure rolls both in-memory stores back; an unset/zero delta still acknowledges the event without changing instability. Strict MinGW warning-as-error syntax compilation passes for GameSession, rules and WorldRevisionValidator. End-to-end notice transaction runtime coverage remains outstanding.

Draw-instability policy validation (2026-10-13): ItemCatalogValidator now checks draw-delta configuration fails before the chunk policy exists, accepts bounded empty/overflow values, rejects negative and over-maximum values, and rejects lowering the instability maximum beneath configured deltas. WorldRevisionValidator confirms both draw deltas change the regeneration manifest hash. The validator sources pass strict syntax compilation; runtime execution has not been performed.

World Loom deadline immutability (2026-10-13): WorldRevisionManager now permits setting the committed deadline once and accepts only an exact idempotent repeat; attempts to extend or shorten it fail with FT_ERR_INVALID_STATE. WorldRevisionValidator source checks that changed deadlines are rejected before saving/restoring the active plan. Strict MinGW warning-as-error syntax compilation passes; runtime execution remains blocked by Git Bash MSYS named-object creation failure.

Item acquisition-policy exhaustiveness (2026-10-13): MinecraftItemCatalog::validate_content_coverage now counts explicit special-item policies plus self/admin block-item policies and requires that count to equal the complete registered item-definition count. It rejects content rules that collide with IDs reserved for built-in/runtime block items, preventing an item from acquiring overlapping policy sources. Catalog initialization already runs this validator, and ItemCatalogValidator invokes it again after runtime registration. Strict warning-as-error syntax compilation passes for the catalog and validator; runtime validation remains blocked by the Git Bash MSYS startup error.

Item acquisition-policy runtime smoke (2026-10-13): A temporary direct MinGW harness linked the current MinecraftItemCatalog.cpp against the existing matching full Libft archive, initialized the catalog, and ran validate_content_coverage successfully (item-catalog-init=0, content-rules=104). This verifies the exact-count exhaustiveness check against the current catalog at runtime; it does not replace the broader item/recipe or full application validators, which remain blocked by the Make runner.

Validator freshness recheck (2026-10-13): The workspace executable was invoked with --validate-item-catalog and reported item-catalog: ok builtin=64 runtime=0, but current source declares 71 built-in policies. This proves the checked-in binary is stale and provides no runtime evidence for current catalog coverage. A fresh mingw32-make -j1 normal attempt failed before compilation because Git Bash repeatedly could not create its MSYS named object (NtCreateDirectoryObject, 0xC0000022); the attempt was interrupted. Continue to treat current item/recipe and World Revision runtime validators as unverified until a fresh build is produced. The direct catalog harness recorded above remains valid only for the catalog object it compiled against its stated Libft archive.

Deck-wide copy-limit enforcement (2026-10-13): card_game_format::validate_deck now totals each card definition across every submitted deck zone before applying its configured copy limit. The main and side zones can no longer each independently consume the full allowance. Added test_card_game_format_enforces_copy_limit_across_deck_zones for 4 allowed / 5 rejected, and a direct MinGW runtime harness linked the edited source and current Libft archive; it passed (cross-zone-copy-cap=ok). Both implementation and test pass strict warning-as-error syntax compilation. The full Libft test runner is still unverified because the workspace Make runner fails during Git Bash startup.

Minecraft recipe-policy runtime coverage (2026-10-13): A focused direct MinGW executable compiled the current MinecraftItemCatalog.cpp and MinecraftRecipeCatalog.cpp plus current Libft game_crafting.cpp, game_inventory.cpp, and game_recipe_book.cpp, linked against the existing support archive, initialized both catalogs, and ran validate_recipe_coverage successfully (recipe-coverage=0 outputs=93 missing=0). This verifies that every current content item whose acquisition policy is recipe-backed has a catalogued recipe output, and every tracked card output satisfies its unique recipe/discovery policy checks. It does not prove all proposed recipes/features in Sections 10-12 exist or that the application-wide validators pass. The regression test enforcing deck-wide copy limits also passes a direct runtime harness (cross-zone-copy-cap=ok).

Spawn paths respect active World Loom barriers (2026-10-13): PlayerCollision::player_box_intersects_world now includes the selected-chunk barrier, so PlayerSpawner's normal candidate check rejects reserved chunks. If no legal spawn candidate exists during an active barrier, the fallback no longer drops the player into the reserved region; it leaves the existing position so an already-present player can escape through normal movement rules. WorldRevisionValidator now checks a vertical-free point inside the selected chunk is blocked and a point outside is not. Strict warning-as-error syntax compilation passes for PlayerCollision, PlayerSpawner and WorldRevisionValidator; the runtime validator has not been run. No authoritative teleport or vehicle movement API exists in this client/server code yet, so those hooks remain open for future movement entrypoints.

Barrier-aware spawn regression (2026-10-13): WorldRevisionValidator now exercises PlayerSpawner while the committed barrier is active. It accepts either a spawn outside all barrier geometry or an unchanged in-barrier position if no safe candidate is available; it rejects a relocated spawn still intersecting the barrier and rejects the unsafe in-place drop-to-ground fallback. Strict warning-as-error syntax compilation passes for the validator, PlayerCollision and PlayerSpawner. Runtime execution remains unverified because the application build cannot start Git Bash in this workspace.

Build-path recheck (2026-10-13): Running mingw32-make -j1 normal via cmd.exe reproduces Git Bash NtCreateDirectoryObject access denied (0xC0000022); invoking WSL for the same workspace is denied by the environment (Wsl/E_ACCESSDENIED). Neither alternate shell reaches compilation. Continue using direct MinGW compilation and focused harnesses for source-level slices, while treating application validators as unverified until the workspace can launch its supported build runner.


Validator sweep and stale expectations (2026-10-13): A direct MinGW link assembled a fresh application from current Minecraft objects, all current CardGame objects, current voxel registry/data, and targeted current Libft Game units. `--validate-item-catalog` passes (71 built-ins, 0 runtime, 104 content policies); `--validate-transaction-journal` passes. The catalog validator now runs its synthetic runtime-block mutation last, after production coverage checks, and its Shipwreck Chart craft fixture grants the rare recipe through discovery first. RegenerationCardValidator now locates imported rare items by physical inventory slot and searches the discard pile for the lowest-ID automatic discard. Its remaining snapshot check detects non-identical RCS bytes after restore/replay; the physical mapping and exact request replay succeed, but persistence round-trip still needs investigation. WorldRevisionValidator fixtures were corrected for variable biome counts and zero-based new-record revisions; the sweep now advances to a separate magma-feature generation failure. `--validate-world-items` still terminates with an access violation. Strict warning-as-error compilation passes for the three edited validator sources. The full Libft suite and application-wide `--validate-all` remain unverified.

Swept World Loom barrier collision (2026-10-13): PlayerCollision now clamps a movement step at the selected chunk edge instead of treating the first intersecting chunk block as a collision surface. The WorldRevisionValidator checks exact boundary clipping for entry from either X direction and entry along Z, while retaining the rule that a player already inside a reserved area can leave. Both changed sources pass strict MinGW warning-as-error syntax compilation. An isolated MinGW runtime harness linked the actual PlayerCollision implementation against a stub World and passed forward entry, reverse entry, Z-axis entry and exit from inside. The full World Revision validator remains unverified: a fresh direct-linked application currently fails with Windows heap/stack corruption in world-dependent validators.

Timed furnace enforcement (2026-10-13): Generic immediate crafting now rejects the seven furnace-processing recipe IDs; callers must use the persisted processing queue. The rejected outcome is retained for exact retries and changed-payload reuse is rejected. The WorldItemStoreValidator asserts the rejection leaves input, fuel and inventory revision unchanged. The catalog and validator pass strict MinGW warning-as-error syntax compilation. A focused MinGW runtime harness confirmed immediate rejection, exact retry, changed-payload rejection and a 200-tick processing job that consumes inputs once. The full world-item validator remains unverified because the direct-linked application has a separate heap/stack corruption failure.

RegenerationCard validator stack and snapshot verification (2026-10-13): `RegenerationCardSession` is 502,176 bytes on this build; the physical-import validator held five sessions locally, exceeding the Windows default thread stack. It now allocates those fixtures on the heap with explicit allocation-failure handling. Strict MinGW warning-as-error syntax compilation passes. A focused executable built from current session, CardGame, inventory and validator sources ran the exact physical-import validator successfully, including three physical-card imports, script context, restored mapping, exact retry, and byte-identical RCS snapshot replay. This resolves the reported RCS byte mismatch for that path. The full Libft suite, other application validators and `--validate-all` remain unverified.

Regeneration Card validator recovery and Volcano Mark preflight (2026-10-13): The physical-import validator now heap-allocates its five 502 KB session fixtures, preventing Windows stack overflow; its RCS serialize/restore/retry byte comparison passes. The World Rules validator’s second Surveyor’s Reach fixture now uses a distinct chunk coordinate so it exercises the configured cap, and `RegenerationWorldRules::preflight_card_effect` admits the special Volcano Mark definition (15) through its ID guard. The current RegenerationCardValidator executable passes end to end (`regeneration-cards: ok`, result 0); both changed sources pass strict MinGW warning-as-error compilation. Full Libft tests and application-wide validators remain unverified.

CardGame host-intent snapshot and replay coverage (2026-10-13): The byte snapshot test now restores into an engine with the same registered callback/rules manifest as the source; the previous fixture omitted a callback whose presence is intentionally included in the rules hash. Its direct MinGW runtime harness passes checksum rejection, snapshot restore, pending records, hash equality and no-mutation-on-invalid-snapshot checks. The multi-draw serialized replay test now compares each pending host-intent sequence, schema ID/version, field type and field value after replay deserialization; its direct MinGW runtime harness passes. Both test files pass strict warning-as-error syntax compilation. The full Libft suite remains unverified; direct linking its existing response file fails because its object/archive set has mismatched sections and missing renderer symbols.

Choice-command update (2026-10-13): `CARD_GAME_INTENT_CHOOSE_OPTION` now routes a choice ID, stable option ID, and a host-supplied authoritative server epoch through the sequenced command pipeline. Choice actions participate in request-idempotent retries; retries compare the stable choice payload and reuse the original committed epoch. Command-log v8 stores the epoch and result value and reads v1-v7 with absent fields defaulted to zero. CardGame replay v7 stores the epoch and result value and keeps v1-v6 readers. The host boundary is explicit: ordinary `submit_action` rejects choices and any client-supplied epoch; `submit_host_action` injects server time. Exact retries ignore a later host time and return the original result, while a changed option payload is rejected. The added choice-action test covers the host boundary, retries, command-log roundtrip, and deterministic replay. Changed sources and the test compile with strict MinGW warnings-as-errors. The repository runner could not launch because Git Bash failed creating its named object (`NtCreateDirectoryObject`, access denied); an independent direct MinGW runtime harness passed the choice action, command-log roundtrip, and replay path. Full suite remains unverified. The host-only expiry command now records its epoch and resolved-choice count, and supports idempotent retries and deterministic replay. Authenticated host wiring, session IDs and bounded action-result history remain open.

Host clock-boundary correction (2026-10-13): Choice epochs are not accepted from the client action payload. `card_game_engine::submit_action` rejects choice commands; `submit_host_action` adds the authoritative epoch before dispatch. Exact retries compare the stable client payload and return the original committed state even when the host supplies a later epoch. A changed choice option under the same request ID is rejected. Direct strict MinGW compilation and a runtime harness pass the boundary, retry, command-log and replay paths. This remains a host API boundary; authenticated server wiring into the World Loom scheduler remains open.

Logged choice-expiry update (2026-10-13): Added a host-only `RESOLVE_EXPIRED_CHOICES` command. It runs deterministic defaults at the injected host epoch and stores the number resolved in the command result. Ordinary client action submission rejects this command; the host API binds it to a server actor. Command-log v8 and replay v7 preserve epoch and result value, retaining prior readers. A focused test covers ordinary-action rejection, host execution, exact retry with a later host epoch, persistence, and replay. Strict MinGW compilation and a direct runtime harness pass; the standard test runner remains unable to launch because Git Bash cannot create its named object.

World Loom sequenced choice expiry (2026-10-13): `RegenerationCardSession::resolve_expired_choices` now submits the host-only command with a request ID and persisted sequence, then retains the original epoch, sequence, and resolved count in its bounded RCS1 v6 expiry cache. Retries return that cached result without resubmitting, because the engine's command log is not part of the engine state snapshot. Older RCS1 v1-v5 snapshots remain readable with an empty expiry cache. `GameSession::resolve_expired_regeneration_choices` reads time from the configured authoritative clock, rejects the reserved `UINT64_MAX` internal request ID, and checkpoints successful calls even when no choices expired so the retry record becomes durable. Deadline finalization uses the same checkpoint path with the epoch it just read from that clock. The focused runtime smoke test covers first execution, later-clock retry, snapshot restore, retry after restore, no additional snapshot mutation, and loading a v5 snapshot after removing the v6 cache section. Strict MinGW warning-as-error syntax compilation passes for affected session, persistence, GameSession, and validator units. Authenticated network scheduler wiring, session IDs, bounded result history, and broader transaction coordination remain open.

Choice-expiry validator integration (2026-10-13): The regeneration-card validator now covers the adapter's logged expiry call, later-clock retry, snapshot restore, retry after restore, and the v5 reader compatibility fixture. That fixture now strips the v6 empty-cache count before deriving its older format. A fresh direct MinGW link with the matching current CardGame command object passes `--validate-regeneration-cards` end to end. The standard Make runner still cannot launch Git Bash; `--validate-world-revision` in the direct-linked application terminates with Windows stack-overflow status and remains unverified.

World Revision lifecycle restore validation (2026-10-13): The direct validator's barrier-restore fixture put two `World` objects on the default Windows thread stack, creating a 2.5 MB frame and stack overflow. It now heap-allocates both objects (measured frame fell to 1.9 KB). That exposed a decoder bug: v2 lifecycle metadata length validation subtracted the deadline field after the reader had already consumed it, rejecting every v2 extension. The parser now accounts only for the unread checksum/trailer after selected coordinates. In the fresh direct-linked validator, barrier restore now passes; the broader `--validate-world-revision` run proceeds farther but still fails a later server-validation helper with `FT_ERR_INVALID_STATE`. Strict warning-as-error compilation passes for manager and validator. Full World Revision runtime validation remains open.

World Revision validation follow-up (2026-10-13): The server authorization fixture now accepts and records denial responses sent back to a second authenticated client, matching the server's request boundary. The bonus-pricing case independently verifies paid-only and bonus-only pricing without depending on an asynchronously streamed third chunk. Occupied-player exclusion checks use the pre-update selection revision, and selection-policy assertions now account for the live policy revision and two cumulative Deep Seam effects. The deterministic chunk wait allows up to five seconds for streaming. A fresh direct MinGW link with current manager, server, and validator objects passes `--validate-world-revision` (`world-revision: ok regenerated=1 skipped=0`). A current direct-linked application with updated card/session and CardGame command objects passes both `--validate-regeneration-cards` and `--validate-world-revision`. Strict warning-as-error compilation passes. This validates those focused mechanics only; the standard Make runner, full `--validate-all`, multiplayer lifecycle wiring, and durable cross-system recovery remain open.

World-item validator stack and compatibility follow-up (2026-10-13): Heap allocation of the three large card-session fixtures in `WorldItemStoreValidator::validate` and two nested fixtures in card-transfer recovery reduces measured frames from 1,532,096 + 1,010,720 bytes to 25,424 + 6,272 bytes. A fresh direct-linked `--validate-world-items` no longer overflows the Windows stack and now reaches persistence checks. It currently fails the legacy v2 recipe-book case: after saving/loading the v2 snapshot, recipe 1001 is reported locked (`FT_ERR_INVALID_STATE` assertion). This persistence contract remains open pending isolation of the saved/imported recipe-book state. The full `--validate-all` run independently fails early in the block-edit emissive-delete convergence check with timeout 59; that is outside this integration slice and leaves the global validator sweep unverified.

World-item snapshot validation and v11 fluid-state fix (2026-10-13): Corrected the session-snapshot validator to check the `MCSP` magic used by `GameSessionPersistence` (the `WISP` magic belongs to standalone world-item snapshots). The v2 recipe-book round-trip now passes; an isolated clean-session repro also confirmed recipe 1001 survives import. The same direct-linked `--validate-world-items` run exposed a v11 boundary bug: revision metadata extraction included the trailing fluid-state bytes. `GameSessionPersistence` now ends the metadata range at its declared size. The validator also updates the current metadata stored flags and reports the exact snapshot compatibility stage on failure. After that fix, the fresh direct-linked `--validate-world-items` passes, covering inventory and recipe restore, craft cache, journal, pickup/craft/processing/card-transfer recovery, and session snapshots through v11. Strict MinGW warning-as-error compilation passes for the changed persistence and validator sources. The separate `--validate-all` block-edit convergence timeout and full Libft test suite remain open.

World Loom committed-start retry guard (2026-10-13): A committed request retry now verifies that a still-pending, non-regenerating revision has an active barrier and a persisted decision deadline, then checkpoints the idempotent result before returning success. It no longer reports success immediately after enabling a missing barrier, which could bypass deadline validation or leave the retry uncheckpointed. The decision-window flag is restored only while the revision is pending and not regenerating, preventing a stale committed retry from reactivating it after generation begins or completes. Added a `WorldRevisionValidator` case that starts a headless `GameSession`, seeds a committed start record, retries it with no pending revision, and checks the decision flag and barriers remain inactive. `GameSession.cpp` and `WorldRevisionValidator.cpp` pass strict MinGW warning-as-error compilation. Runtime execution is still unverified: `mingw32-make -j1 normal` fails before compilation because Git Bash cannot create its MSYS named object (`NtCreateDirectoryObject ... 0xC0000022`). Prepared starts still abort safely from the pre-debit snapshot; server draft persistence and crash-durable generation publication remain open.

Entity lifecycle foundation (2026-10-13): Added `WorldEntityStore` for monotonic entity IDs, atomic group spawning, revision-checked updates/despawn, bounded snapshots, and checksummed WENS v1 serialize/restore. The `--validate-world-items` path checks entity snapshot roundtrip, successful batch IDs, invalid-batch no-partial-mutation, stale revision rejection, checksum rejection, and no mutation on corrupt restore. Strict MinGW compilation passes; the direct-linked runtime validator passes these cases. The store is now connected to GameSession snapshots in MCSP v12, with older snapshots restoring an empty entity store. World/chunk lifecycle, mob AI, spawners, and loot remain unimplemented and required for the rare-card effects.

GameSession entity snapshot integration (2026-10-13): Added MCSP v12 with a bounded entity-state section after fluid state, backward-compatible loading for versions 1-11, GameSession startup restore, and snapshot serialization. The world-item validator covers a v12 entity roundtrip and v11 loading through the extended API with an empty entity section. All changed translation units pass strict MinGW warning-as-error syntax compilation. A fresh direct-linked application using current Minecraft and Libft objects passes `--validate-world-items` end to end (`world-items: ok`), including the v12 test, and also passes `--validate-regeneration-cards`, `--validate-world-revision`, and `--validate-item-catalog` (71 built-in, 104 content policies). The standard Make runner still cannot launch because Git Bash fails to create its MSYS named object. World/chunk lifecycle, mob AI, spawners, and loot remain open.

Entity spatial query foundation (2026-10-13): `WorldEntityStore::query_radius`
returns a bounded 3D radius query in stable entity-ID order. Invalid/non-finite
query bounds are rejected; exceeding the caller's result cap returns `FULL`
without changing the output vector, avoiding silent truncation for future AI
and interaction callers. Strict MinGW syntax compilation passes with
warnings-as-errors. The world-item validator checks stable ID ordering,
bounded overflow without partial output, and non-finite radius rejection; a
fresh direct-linked `--validate-world-items` run passes end to end. This is
only a lookup primitive: mob AI and structure planners remain open. The
authoritative entity store, scheduled hostile spawners, bounded loot tables,
and checkpointed death-to-drop path have since been implemented; see the
entries below. Rare-card structure plans are still open.

Harvest acquisition integration (2026-10-13): The built-in acquisition table
maps tall grass, fern, and dead bush to plant fiber; seagrass to paper reed
fiber; and red/yellow flowers to their corresponding pigments. Mining uses the
existing pending-drop transaction, so each yield publishes only after the
block edit commits. Shipwreck Chart consumes harvested paper reed fiber instead
of an unavailable seagrass block item. Red/yellow pigment acquisition is
harvest-based and the duplicate pigment craft recipes are removed. The
item-catalog validator checks those mappings, and a fresh direct-linked
`--validate-world-items` run passes all six harvest/drop merge cases along with
existing session, recovery, and persistence checks. Crop, fruit, animal
acquisition, mob lifecycle, and rare-card effects remain open.

Volatile Stabilizer policy and replay foundation (2026-10-13): Registered card
definition 16 and its physical item mapping. GameSession now requires a
server-configured nondecreasing overload-risk curve and explicit positive
success reduction/overload surge; there is no built-in probability or
instability amount. The CardGame match RNG rolls inside the effect callback,
the pending host intent carries the outcome privately, and card play resolves
the effect immediately. Successful uses reduce selected-chunk instability
(bounded at zero); overloads apply the configured surge after capacity
preflight. The applied-use ordinal, outcome, and RNG state are snapshot-backed;
RegenerationWorldRules v3 stores private result fields and continues to read v2.
Focused `--validate-regeneration-cards` covers imported stabilizer item mapping,
configured policy manifests, use-order checks, both outcomes, and v3
serialize/restore. `--validate-item-catalog` covers valid and invalid risk
policy configuration. This is not the complete card contract: a persisted
danger/reward opportunity planner, crash-atomic host delivery, and end-to-end
GameSession roll/retry/restore coverage remain open, alongside the other rare
card effects and their world planners.

Direct-link validator diagnosis (2026-10-13): The `tmp/current_all` direct-link
set contained stale `RegenerationCardSession`, `WorldItemCardTransferTransaction`,
and `WorldItemStoreValidator` objects relative to the current headers. Rebuilding
those objects removed the heap-corruption warning from
`validate_card_transfer_journal_recovery`; under GDB the helper returned success,
and `WorldItemStoreValidator::validate` printed its `world-items: ok` result.
The executable then reported a Windows heap error during thread teardown, so the
process-level validator command is not a clean pass. This is build-artifact
diagnosis only and does not close the still-open integration requirements above.

Volatile Stabilizer GameSession recovery (2026-10-13): Committed revision restore
now rehydrates saved chunk reservations directly from validated metadata before
streaming loads those chunks, while enforcing the configured selection cap and
protection rules. The GameSession validator uses a selected chunk outside the
restored player's occupied protection ring and verifies a success, exact retry,
overload, save/restore, persisted use count, and exact post-restore retry. A
fresh direct-linked `--validate-world-revision` run passes. This closes the
end-to-end stabilizer roll/retry/restore item; the opportunity planner,
crash-atomic host delivery, and the other rare-card effects and planners remain
open.

Monotonic instability opportunity tiers (2026-10-13): Reward and danger bands
now represent the highest tiers unlocked for a chunk. Raising instability can
advance them; a later stabilizer reduction changes `current_value` but cannot
revoke an earned tier. The existing WCI1 v1 layout remains readable: decoded
bands are validated against policy caps and must be at least the bands implied
by the saved current value. The World Revision validator covers reduction,
public-view persistence, and serialize/restore of a retained reward tier. Strict
MinGW warning-as-error compilation and a freshly relinked direct
`--validate-world-revision` pass. Actual reward/danger opportunity planning and
content remain open.

Hostile spawner lifecycle foundation (2026-10-13): Added bounded persistent
spawner definitions with per-spawner live caps, server-tick spawn schedules,
stable entity ownership, and death events that remain pending until explicitly
acknowledged. WSP1 validates hostile types, finite positions, limits, ordered
IDs, and checksums. GameSession now advances spawners on its authoritative tick
and checkpoints entity plus spawner state together in MCSP v13/WEC1; older
MCSP v1-v12 and WENS entity payloads remain readable. The World Revision
validator restores a scheduled spawner and checks it stays idle before its
deadline and spawns at the saved deadline. Strict MinGW warning-as-error
compilation and direct-linked `--validate-world-revision` pass. This is still a
foundation: mob AI, loot-table resolution and atomic death-to-drop delivery,
spawner structure generation, and the rare-card feature planners remain open.

Spawner death-to-drop integration update (2026-10-13): GameSession now resolves persisted hostile-spawner death events through loot-table IDs 1, 2, and 3 (zombie maps to rotten flesh, skeleton maps to bone, creeper maps to gunpowder). Each event creates a ground item, removes the tracked corpse when present, and acknowledges the pending event before one combined MCSP v13 checkpoint. Failed processing/checkpoint restores the item and entity/spawner stores to their pre-processing snapshots, leaving the event retryable. These are deterministic one-item placeholder tables; weighted quantities, player attribution, mob AI, and feature planners remain open. Strict MinGW warning-as-error syntax compilation passed for the touched session, spawner, and item-catalog units. The standard Make runner still fails before compilation with an MSYS named-object access error, so runtime integration validation remains outstanding.


Mob loot catalog update (2026-10-13): Replaced GameSession's inline loot switch with the bounded MinecraftLootCatalog. Loot resolves deterministically from table ID and entity ID: zombies drop rotten flesh, skeletons drop bones and string, creepers drop gunpowder, with quantities capped at two per product. Unknown tables and zero entity IDs fail without changing output. The supported item catalog registers each drop with a world-acquisition policy. Strict warning-as-error syntax compilation passes for the session, spawner, item and loot catalog, and validator units. A focused executable smoke passes deterministic retry, unknown-table rejection, and invalid-ID checks. Full GameSession death-checkpoint integration runtime coverage remains outstanding.


Death-delivery validator update (2026-10-13): Factored the GameSession death commit into a directly exercised helper. WorldRevisionValidator now stages a skeleton death, checks both expected ground items, verifies corpse removal and event acknowledgement, stops/restarts the session, and checks the same two drops restore without a replayed death. Strict warning-as-error compilation passes for the expanded validator. The source runner cannot execute because Git Bash fails creating its MSYS named object (NtCreateDirectoryObject ... 0xC0000022); this integration case remains runtime-unverified.


Direct-link validation diagnosis (2026-10-13): Strict warning-as-error syntax compilation passes for the current session, loot catalog, spawner and validator sources. The standalone loot-catalog runtime smoke passes. A manually linked World Revision validator exits before the death-delivery check because GameSession startup reports MinecraftItemCatalog::initialize() error 7. A fresh catalog harness linked against the available full Libft archive reproduces error 7 at catalog initialization, while an older prebuilt catalog-count executable succeeds with its prior 104-policy source. The archive/build inputs therefore do not establish whether the current catalog source is at fault; the full GameSession checkpoint path remains unverified pending a current, internally consistent Libft link.


Death checkpoint rollback coverage update (2026-10-13): The GameSession regression fixture now redirects the snapshot path to a missing parent to force save failure during death delivery. It asserts the ground-item store remains empty, the corpse remains present, and the pending death is replayable; it restores the valid path and retries the same event before checking successful commit and restart persistence. Strict warning-as-error syntax compilation passes. This expanded runtime case is still unverified because the direct-linked application fails earlier at catalog initialization and the Make launcher cannot start Git Bash.

Loot chance and validator correction (2026-10-13): Skeleton loot uses an always-present bone entry and a deterministic 50% string entry, each with quantity one to two. WorldItemStore validation covers entity IDs that produce both outcomes; the World Revision death-delivery validator derives expected drops from the catalog and checks matching ground items before and after restart. Strict warning-as-error compilation passed for the loot catalog and both affected validators, and a focused executable verified the deterministic string and no-string outcomes. End-to-end GameSession runtime execution is still limited by the inconsistent available Libft link/build artifacts.

Loot failure-output contract validation (2026-10-13): WorldItemStoreValidator verifies an unknown table leaves the caller's existing drop vector unchanged and that zero entity IDs fail with invalid argument. Strict warning-as-error syntax compilation passes for the loot catalog and affected validators; the focused executable passes deterministic string/no-string, unknown-table output preservation, and zero-ID checks. Full GameSession death-checkpoint runtime validation remains unverified.

Spawner death runtime and catalog-ID correction (2026-10-13): A fresh direct-linked `--validate-world-revision` now passes the hostile death delivery scenario, including injected save failure, rollback, retry, commit and restart restore. The run revealed that mob-drop IDs 210070-210073 collided with existing shield, staff and armor definitions; they are now stable unused IDs 210130-210133. It also exposed a World Revision restore check that compared the saved selection revision with temporary reconstruction increments; restore now validates the selected chunk set and reinstates the saved authoritative revision. The same executable passes `--validate-item-catalog` (71 built-ins, 108 content policies) and `--validate-regeneration-cards`. Its `--validate-world-items` run still fails in card-transfer journal recovery with FT_ERR_NOT_FOUND against the available Libft archive, so that validator remains unverified in this build.

MCSP restore and runtime validation correction (2026-10-13): Fixed the loader carrying FT_ERR_NOT_FOUND from the successful legacy-payload probe into current MCSP parsing. Version-9 snapshots then failed before item restore because they bypassed the entity validation branch that had previously overwritten the stale error. Added an explicit prepared-snapshot existence assertion in the card-transfer recovery fixture. A fresh direct-linked executable now passes `--validate-world-items` end to end, including card-transfer exact retry, and passes `--validate-world-revision`, `--validate-item-catalog` (71 built-ins, 108 content policies), and `--validate-regeneration-cards`. The standard Make launcher remains unavailable because bundled Git Bash/MSYS fails before compilation.

Processing-station authorization correction (2026-10-13): Local station continuity now requires the supplied voxel revision to match the station chunk current revision, so stale station contexts fail closed. Regeneration-protected chunks remain usable for local crafting; this protection is a regeneration barrier rather than station ownership, and the separately configured host authorizer remains responsible for multiplayer claims and access. Updated the camera-interaction validator to assert this behavior. Strict MinGW warning-as-error compilation passes for the changed session and validator, and a fresh direct-linked executable passes both `--validate-camera-interaction` and `--validate-world-revision`. Rare-card world planners, durable multiplayer lifecycle, complete item and recipe content, and coordinated host transactions remain open.

World-item persistence validator verification (2026-10-13): Rebuilt the session snapshot, world-item store, transaction journal, pickup/craft/processing/card-transfer transactions, and WorldItemStoreValidator objects together against the current headers before direct linking. `--validate-world-items` completed with its `world-items: ok` result, covering MCSP v1-v13 compatibility, exact card-transfer retry, and pickup/craft/processing recovery; on this Windows workspace the file-backed suite took several minutes because its compatibility cases repeatedly replace the snapshot file. The validator and transaction sources were then rebuilt without temporary tracing. Strict warning-as-error compilation and `git diff --check` pass, and the clean direct-linked executable passes `--validate-camera-interaction`. This clears the earlier unverified world-items result; the rare-card planners and other open design requirements remain.

Village Charter guaranteed-placement update (2026-10-13): Village plans already reserve one distinct selected chunk per accepted Charter before the card is consumed. The terrain planner now preserves its natural-site search and uses a fixed, bounded 9-by-9 pad inside the assigned chunk when no level site exists, including an all-air chunk with only the bottom layer. This prevents the promised village from failing later with `FT_ERR_NOT_FOUND`; the chunk remains within the existing protected/occupied exclusion and selection-cap rules. The World Revision validator checks the synthetic all-air fallback and the normal generated village path. Strict MinGW warning-as-error compilation passes, and direct-linked `--validate-world-revision`, `--validate-camera-interaction`, `--validate-regeneration-cards`, and `--validate-item-catalog` pass. Ocean/desert/forest/cave rare-card planners, spawner structure integration, and multi-system durable publication remain open.

Surface Magma guaranteed-placement update (2026-10-13): The feature applier now preserves deterministic natural-site selection and uses a bounded center-of-chunk basin fallback if all natural candidates fail. On a chunk with no natural floor, the fallback lays volcanic rock at the minimum safe height, contains the lava pool within a volcanic-rock rim, and keeps all writes within the chunk. The World Revision validator checks this synthetic all-air case plus the normal generated terrain path; direct-linked --validate-world-revision passes (world-revision: ok regenerated=1 skipped=0). The fallback is implemented and verified; ocean/desert/forest/cave rare-card planners, hostile-spawner lifecycle, and durable multi-system publication remain open.

Boat entity identity validation hardening (2026-10-13): `WorldEntityStore::spawn_item` now accepts only boats; revisioned updates cannot turn an item-backed boat into another entity or turn a non-item entity into a boat. WENS deserialization rejects non-boat records carrying physical item IDs. The validator checks invalid spawn/update rejection, checksum-correct malformed snapshot rejection without mutating the destination store, and legacy compatibility. Strict MinGW warning-as-error compilation passes, and a fresh direct-linked `--validate-world-items` run passes. The cross-store boat placement/retrieval transaction, interaction path, and movement remain open.

Physical item allocator coverage for entities (2026-10-13): `WorldEntityStore::get_max_item_instance_id` reports the highest identity attached to a persisted entity, and `GameSession::synchronize_item_instance_id_allocator` now includes that value with inventory, ground-item and card identities before crafting or minting new items. The entity validator checks a placed boat contributes its ID. Strict MinGW warning-as-error compilation passes; direct-linked `--validate-world-items`, `--validate-world-revision`, `--validate-regeneration-cards`, and `--validate-item-catalog` all pass. Cross-store boat placement/retrieval and movement remain open.

Entity/inventory/ground identity uniqueness (2026-10-13): `WorldItemStore::validate_instance_id_uniqueness` can now include `WorldEntityStore` identities in the same check. Session startup rejects snapshots that duplicate an instance ID between inventory, dropped items and item-backed entities; every subsequent session checkpoint enforces the same invariant. The validator checks conflicting and distinct boat/ground IDs. Strict MinGW warning-as-error compile and direct-linked world-item, World Revision, regeneration-card, and item-catalog validators pass. Card-session mappings are included in the next-ID high-water mark but are not yet part of this duplicate scan; boat transfer and movement remain open.

Global physical-item identity check (2026-10-13): `RegenerationCardSession::copy_physical_item_instance_ids` exposes the session-owned physical card identities as a stable ordered list. GameSession now checks those IDs together with character inventory, ground items and item-backed entities after restore and before each checkpoint. The world-item validator checks duplicate and distinct additional identities; the CardGame validator checks that all imported physical card IDs are returned. Strict MinGW warning-as-error compilation passes, and direct-linked `--validate-world-items`, `--validate-world-revision`, `--validate-regeneration-cards`, and `--validate-item-catalog` all pass. Boat placement/retrieval transactions, interaction and movement remain open.

Regeneration publication rollback update (2026-10-13): WorldRegenerationResultApplier now retains the currently published chunk while installing a generated replacement and committing its generation/feature-outcome record. If chunk read-state publication or the instability-store update fails, it restores the prior chunk and reports the original error (or FT_ERR_INVALID_STATE if restoration itself fails). This closes the in-memory mismatch where regenerated voxels could remain visible after generation metadata rejected the commit; it does not by itself provide crash-atomic persistence across world, inventory, and host journals. Strict MinGW warning-as-error compilation passes, and the fresh direct-linked --validate-world-revision run passes (world-revision: ok regenerated=1 skipped=0).

Magma feature reservation correction (2026-10-13): Surface Magma pool plans now reserve one selected chunk each, after Village Charter reservations and before Volcano square selection. Volcano placement excludes both village and magma chunks; feature-preview planning uses the same reserved-chunk rule. GameSession preflight now includes already-applied plans, pending village/magma/volcano cards, and the card being played, rejecting insufficient capacity before consuming it. Strict MinGW warning-as-error compilation passes for GameSession and WorldRevisionRegenerator, and a fresh direct-linked --validate-world-revision plus --validate-regeneration-cards run passes. The app-wide validators and remaining rare-card effects remain open.
 The chunk reservation cursor is now a shared WorldRevisionChunkSet primitive; its regression fixture proves that a second feature class skips chunks already reserved by the first and returns FT_ERR_FULL when capacity is exhausted. The fresh direct-linked --validate-world-revision was rebuilt with this test and passes.

Physical item entity persistence foundation (2026-10-13): WorldEntityStore now stores an optional physical item instance ID alongside entity state; WENS v2 adds this field while retaining v1 deserialization with item ID 0. Boat entities require the item-bearing spawn path, item IDs cannot be duplicated in the store, and the world-item validator checks a boat item's ID through MCSP session roundtrip plus a synthesized WENS v1 compatibility load. Strict MinGW warning-as-error compilation passes and direct-linked --validate-world-items passes (world-items: ok). Boat placement/retrieval transactions, client interaction, and movement remain open; this is a persistence foundation only.

Boat inventory/entity transfer (2026-10-13): Added journaled GameSession actions to place a boat from an identified inventory slot onto a validated water block and retrieve the same physical item from a reachable boat entity. Both operations enforce expected inventory/entity/world revisions, cross-store physical-ID uniqueness, request idempotency, snapshot checkpointing, and rollback on failure; pending prepared transfers recover against the pre-action snapshot. The world-item validator covers placement/retrieval, exact retries, failed-checkpoint rollback, and prepared-record recovery. Strict MinGW warning-as-error compilation passes for the transaction, GameSession, and validator sources; a fresh direct-linked `--validate-world-items` passes, as do `--validate-world-revision`, `--validate-regeneration-cards`, and `--validate-item-catalog` (71 built-in, 108 content policies). Client interaction/rendering, boat steering and movement, multiplayer ownership/authorization, and crash-atomic coordination with external host state remain open.

Livestock death-drop acquisition foundation (2026-10-13): Added persistent cow, pig, chicken, and sheep entity kinds and enabled their bounded scheduled spawns in WorldSpawnerStore. MinecraftLootCatalog now resolves deterministic, bounded primary meat drops plus leather, feathers, or wool where applicable. World-item validation checks animal loot determinism and that each livestock type can spawn through the persisted schedule; World Revision validation drives a cow death through GameSession's ground-drop checkpoint path. Strict MinGW warning-as-error compilation passes, and direct-linked `--validate-world-items`, `--validate-world-revision`, `--validate-regeneration-cards`, and `--validate-item-catalog` pass. This provides the death-to-drop acquisition path only: animal movement/AI, natural habitat spawning, interaction and breeding, crop growth/harvest, and item acquisition rollback injection for livestock remain open.

Crop and fruit acquisition foundation (2026-10-14): Added append-only immature/mature wheat and farmland blocks with registry names, metadata and atlas mappings. `World::till_farmland_at` revision-checks and transforms dirt/grass without creating an inventory item. A craftable wood hoe now gives `GameSession` a secondary-click till action; it requires the hoe in inventory, checks the targeted soil, reach, protection, inventory/world revisions, and delegates to `WorldFarmlandTransaction` for prepared journaling, rollback and recovery. `--validate-wheat-crop` passes world tilling, stale-revision rejection, wheat/seed harvest, apple/pear leaf harvest, exact transaction retry, changed-payload rejection, hoe preservation, failed-checkpoint rollback, and prepared-record recovery. Direct-linked `--validate-item-catalog` passes with 74 built-ins and 109 content policies, including the hoe recipe; modified sources compile with strict warning-as-error flags and `git diff --check` passes. `--validate-world-items` now passes end-to-end, including mining and existing inventory, persistence and recovery cases. The focused validator exercises the transaction helper but not the live mouse/session authorization path. The broader block-edit validator still times out earlier in emissive-delete convergence. Moisture/light growth rules, paper plant acquisition, and gameplay/render validation remain open.

Wheat hydration eligibility (2026-10-14): The bounded near-player crop tick now
requires a water source within four horizontal blocks of the farmland, at the
soil level or one block above/below. Missing or unloaded water cells do not
hydrate the crop. `World::farmland_is_hydrated` performs the bounded lookup
under one world read lock. `--validate-wheat-crop` verifies dry soil, hydration
from a nearby water source and loss of hydration after source removal. Fresh
direct-linked crop validation passes; the changed world, session and validator
translation units pass strict MinGW warning-as-error compilation, and
`git diff --check` passes. This is a fixed initial hydration rule; configurable
hydration/growth parameters, authoritative per-block light thresholds,
persisted per-chunk growth deadlines/offline policy, and broader gameplay
coverage remain open.

Paper-reed renewable growth slice (2026-10-14): Added append-only builtin
`voxel:paper_reed` (ID 74) and `voxel:paper_reed_immature` (ID 75), metadata,
texture/color mapping, and explicit acquisition policies. Decoration
generation places immature reeds deterministically on suitable grass/dirt/mud/
sand shoreline ground within four horizontal blocks and one vertical block of
surface water. The generator version is now 12. The bounded server crop tick
only matures reeds on supported ground with nearby water. Mining mature reeds
publishes fiber and changes the plant back to immature; if drop publication
fails, the block transition is rolled back. `--validate-wheat-crop` checks
mature-harvest-to-immature, fiber merge, and runs the actual `GameSession`
growth routine to prove dry reeds stay immature and mature after nearby water
is added. `--validate-item-catalog` passes with 76 built-ins and 109 content
policies. The terrain determinism validator searches 16 deterministic wet-world
samples for natural reed placement; fresh direct-linked terrain, crop, and
item-catalog validators pass. Strict warning-as-error compilation passes for
the session, world, voxel generation, interaction, catalog, and validator
changes. Paper-reed planting, persisted crop growth deadlines/offline policy,
authoritative light thresholds, and gameplay/render validation remain open.

Livestock wandering slice (2026-10-14): Added a bounded server-side animal
simulation for cows, pigs, chickens and sheep. Every 10 ticks, nearby animals
choose deterministic wander directions from entity ID and tick epoch, step only
onto loaded supported space with clear body/head cells, and update persisted
position, velocity, yaw and action state through revision-checked entity-store
updates. Simulation is capped at 256 nearby entities and saves changed animal
state on the existing 600-tick session checkpoint. The world-item validator
checks the tick interval, repeatable movement across identical stores, and
state preservation after entity snapshot restore; the full direct-linked
`--validate-world-items` run passes. This is wandering only: follow/flee and
flocking behavior, natural habitat population, breeding/age, player interaction,
and animal rendering/replication remain open.

Chicken egg acquisition slice (2026-10-14): Chickens now produce one egg on a
deterministic per-entity interval from 6,000 to 11,999 server ticks (five to
ten minutes at 20 ticks per second). The persisted animal tick prevents a
second egg on retry; the egg and updated entity snapshot commit through the
same session save. Failed item creation or checkpoint restores the ground-item
store and rewinds the chicken tick so production can retry. The World Revision
session validator injects a checkpoint failure, verifies rollback and retry,
then restarts and confirms exactly one egg is present. Strict MinGW compilation
and direct-linked `--validate-world-revision` pass. Egg timing is fixed for now;
configuration and client presentation remain open.

Wheat seed planting integration (2026-10-14): The gameplay-selected seed path
now has runtime coverage in the World Revision session validator. It tills a
real supported dirt/grass cell, aims the live camera interaction at the cell,
places immature wheat through `try_place_selected_inventory_block`, and checks
that the seed is consumed and the block appears. This exposed a protection
interaction: tilling marks the edited chunk generation-protected, after which
the generic placement guard rejected planting there. Placement now permits
immature wheat on farmland in generation-protected chunks while retaining
Loom, manual and occupied-chunk denials. Strict MinGW compilation and the
direct-linked `--validate-world-revision` pass. Paper-reed planting, crop
deadline/offline persistence, light thresholds, and gameplay/render validation
remain open.

Paper-reed planting integration (2026-10-14): Paper-reed fiber now resolves to
the immature paper-reed block for the gameplay-selected placement path. Planting
requires grass, dirt, mud, or sand support and a loaded surface-water source
within the existing four-block shoreline radius. It uses the shared journaled
inventory/world placement transaction, permits the crop placement exception in
generation-protected chunks, and still rejects Loom, manual, and occupied-player
protection. Item-catalog validation checks both the immature block's fiber
acquisition rule and fiber-to-plant placement mapping; a fresh direct-linked
`--validate-item-catalog` passes (76 built-ins, 109 content policies), as does
`--validate-world-revision`. Its live selected-item fixture verifies dry-site
rejection, water-source acceptance, placement, and fiber consumption. Strict
MinGW warning-as-error compilation and `git diff --check` pass. Persisted
growth deadlines/offline policy, configurable growth values, and broader
gameplay/render validation remain open.

Authoritative crop light gating (2026-10-14): `World::light_level_at` now
returns the combined sky/block value only from a loaded chunk whose light
buffer matches its current content and light-input revisions. The bounded crop
tick requires level 9 before wheat or paper reed can mature; stale, invalid, or
unloaded light data fails closed. `--validate-wheat-crop` sets and checks stale,
dark, and bright light states, proving stale/dark crops remain immature and a
bright, hydrated reed matures. Strict MinGW warning-as-error compilation
passes for the world query, session, and crop validator, and the fresh crop
validator passes. This threshold is a fixed initial default; configurable light
and growth values plus persisted growth deadlines/offline behavior remain open.

Configurable crop growth policy (2026-10-14): Added a pre-start
`configure_crop_growth_policy` host API for the crop tick interval, deterministic
maturity-roll divisor, and separate wheat/paper-reed light thresholds. Defaults
preserve the verified behavior (200 ticks, one-in-three roll, light level 9).
Zero intervals/divisors and light values above 15 are rejected; upper bounds
prevent unbounded tick intervals and modulo policies. The crop validator checks
invalid-policy rejection and uses a custom reed threshold of 12 to prove level
11 remains immature while level 15 matures. Fresh direct-linked crop and World
Revision validators pass, and strict MinGW warning-as-error compilation passes.
The host must currently reapply the same policy on restore; policy identity in
the persisted session/config manifest remains open, alongside growth deadlines,
offline behavior, configurable hydration radius, and gameplay/render coverage.

Crop policy persistence (2026-10-14): MCSP v14 now stores the 16-byte validated crop growth policy and restores it as the session's authoritative policy. Existing snapshots through v13 remain readable and use constructor defaults; snapshot validation rejects malformed policy lengths and values. The World Item Store validator now retains a v13 spawner compatibility roundtrip and adds a v14 non-default policy roundtrip plus future-version rejection at v15. Production persistence/session translation units pass strict warning-as-error syntax compilation. A standalone direct-linked v14 save/load smoke test passes with a non-default policy, and --validate-wheat-crop passes. The full --validate-world-items run has not returned an exit result from the runner. The smoke test caught and fixed an entity-section boundary bug in v14 parsing. Per-chunk growth deadlines/offline catch-up and hydration-radius configuration remain open.

Configurable crop hydration radius (2026-10-14): CropGrowthPolicy now configures the shared horizontal water radius (0–8) used for farmland hydration and paper-reed growth. MCSP v15 persists the radius; v14 continues loading its 16-byte policy with the historical radius-4 default. The crop validator rejects out-of-range radii and proves a reed stays immature with water one block away at radius 0, then matures with radius 1 and sufficient light. Strict production syntax compilation, the focused crop validator, v15 standalone persistence smoke test, and git diff --check pass. Per-chunk growth deadlines and offline catch-up remain open.

Per-chunk crop deadlines and offline-time policy (2026-10-14): MCSP v16 now persists a checksummed crop-growth state containing the validated policy, world simulation tick, and bounded sorted per-chunk next-growth deadlines. Startup restores those values; growth checks only due nearby chunks and checkpoints updated deadlines with crop mutations. Simulation ticks pause while the session is offline, so the system performs no wall-clock catch-up; a stale deadline advances by one scheduled growth attempt and is rescheduled from the restored simulation tick. The World Item Store compatibility validator now round-trips a CGS1 v16 payload with a negative chunk coordinate and rejects v17. Strict validator syntax compilation, direct-linked --validate-wheat-crop, and the standalone v14/v15/v16 persistence smoke pass. The full --validate-world-items direct-linked invocation did not return or show a running process in this runner, so full suite execution is unverified. Gameplay/render validation and broader system design sections remain open.

Crop v16 integration recheck (2026-10-14): In addition to the focused crop and v16 persistence checks, a rebuilt direct-linked binary passes --validate-item-catalog and --validate-regeneration-cards. Its --validate-world-revision run currently fails in the Volatile Stabilizer restore case when GameSession::start returns FT_ERR_INVALID_STATE; this validator was previously passing before the v16 writer was introduced, so session restore integration remains an unresolved regression candidate. The broad --validate-world-items invocation does not return and leaves no executable process visible. Do not treat this crop slice as fully validated until restore failure is isolated and the broad validator returns.

Restore validator link correction (2026-10-14): Recompiling WorldRevisionValidator.cpp from the current source and relinking removed the Volatile Stabilizer restore failure; fresh --validate-world-revision passes (world-revision: ok regenerated=1 skipped=0). The earlier code-7 result came from a stale validator object and was not a crop regression. On this fresh binary, --validate-world-items exits with Windows heap-corruption status xC0000374 without diagnostics; the direct-linked validator remains unresolved and needs isolation.

World-item compatibility validator repair (2026-10-14): The v15-to-v14 fixture now closes its input stream before atomically rewriting the snapshot, avoiding Windows sharing violations. After rebuilding the current World Item Store and World Revision validator objects and linking a fresh executable, --validate-world-items passes end to end, including the new MCSP v16 CGS1 deadline roundtrip and v17 rejection, alongside inventory, recipe, transaction recovery, and block harvest checks. The earlier failures came from concurrent validator runs sharing fixed temporary files and from the fixture's open input handle; the stale-validator restore failure was also resolved by relinking current source. The standalone v14/v15/v16 persistence smoke, crop validator, item-catalog validator, regeneration-card validator, World Revision validator, strict validator compile, and root git diff --check pass. Overall design implementation and its broader multiplayer, rare-card, catalogue, UI, and performance requirements remain open.
Rare-card source audit (2026-10-14; supersedes the earlier gap audit): The
earlier statement that all six rare cards lacked executable identities is
outdated. Current source registers and previews the definitions, persists
feature intents, maps all five structure feature kinds through GameSession and
the worker generator, and includes hidden Catalyst/Overload modifier handling.
Ruined Village and Shipwreck have explicit loot/spawner payload paths; the
other structure paths also carry generation payloads in current source. These
are implementation slices, not proof of complete card economics, runtime
publication, restore behavior, or validator coverage. Their remaining work is
tracked in the later progress notes and the required Minecraft validation
matrix below.

Spawner batch registration and rollback foundation (2026-10-14): Added bounded WorldSpawnerStore add_batch and remove_batch operations for structure-authored spawner groups. Batch creation validates all definitions, stages sequential IDs, and rolls back on allocation failure. Removal validates the full ID set before mutation and only removes spawners with no live entities or pending deaths. Validator source covers invalid-input rollback, successful IDs, remove-set rejection without mutation, and successful removal. Strict MinGW warning-as-error syntax compilation passes for the spawner store and validator. A focused direct-linked runtime smoke against the current Libft archive passes invalid-input rollback, sequential ID assignment, invalid-remove no-mutation, and successful removal. The full world-items validator remains unrun, and rare-card planners do not yet call these APIs.

World regeneration store attachment (2026-10-14): GameSession continues to own and persist entity/spawner stores, while an active World now borrows those stores through an explicit attach/detach lifecycle. This establishes access for the regeneration result applier without moving session snapshot ownership or changing MCSP formats. Strict warning-as-error syntax compilation passes for World, GameSession, and WorldRegenerationResultApplier. Generated spawner payload publication and rollback are not yet connected; no rare-card planner is complete.

Generation-result spawner payload path (2026-10-14): GenerationFeaturePlan now carries bounded-value spawner definitions through queued requests and worker results. WorldRegenerationResultApplier filters definitions by owning chunk and commits them through WorldSpawnerStore::add_batch only after replacement chunk read-state publication; metadata failure removes the new spawner batch and restores the previous chunk. Each request allows at most 16 spawner definitions per feature plan and 4096 feature plans; the active World borrows the session-owned stores. Missing attachment, invalid coordinates, and deadline overflow fail before publication. A World Revision validator fixture checks the value payload survives request/worker result transport. Strict warning-as-error syntax compilation passes for the pipeline, manager, regenerator, applier, World, GameSession, and validator units. Runtime publication/rollback validation is still open, and no rare-card planner supplies these definitions yet.

Ruined Village Relic rules slice (2026-10-14): Added a manifest-bound instability delta and bounded per-session plan count. Rules now validate and persist definition 17 intents, reject capacity overflow before mutation, and reconstruct the Ruined Village feature plan after serialization restore. The World Revision validator source covers policy-hash sensitivity, accepted intent, cap rejection, and plan persistence; strict warning-as-error syntax compilation passes. A focused direct-linked rules smoke against the available Libft archive passes the same runtime cases. The card remains unavailable in RegenerationCardSession and has no generation kind reservation, ruined structure, or spawner payload yet; this rules slice is not a playable card.

Ruined Village Relic vertical-slice progress (2026-10-14): Definition 17 is now registered and previewable in RegenerationCardSession, maps from the physical item catalog, and is covered by the card-preview and item-catalog validator fixtures. GameSession preflight reserves one selected chunk, accounts for pending plans and applies the configured manifest-bound instability cost; the host can set that positive cost before start. Regeneration maps feature kind 4 to one distinct selected chunk, generates a damaged village footprint and gold cache, and carries zombie/skeleton spawner definitions through the worker result applier. The skeleton spawner uses a dedicated deterministic loot table with rare emerald and diamond drops. The gold cache also emits a bounded value-only emerald loot payload; WorldRegenerationResultApplier stages it invisibly in the session-owned WorldItemStore and publishes it with the replacement chunk/spawners, compensating the item if the chunk or instability commit fails. The request builder caps loot payloads, and World Revision validator fixtures now cover structure shape and loot payload transport/capacity. Strict warning-as-error syntax compilation passes for all touched production and validator units. Runtime generation/publication, rare-loot distribution and rollback remain unverified: both `make -j1 normal` and invoking make through cmd.exe terminate in the local make runtime before building. Other five rare cards and the broader design remain incomplete.

Shipwreck Chart vertical-slice progress (2026-10-14): Definition 18 now has a preview and maps from the physical card item. Rules persist its feature intent with a manifest-bound instability cost and bounded plan count; GameSession reserves selected ocean chunks and regeneration rechecks the water target before assignment. The worker generates a chunk-bounded shipwreck with a clear lower hull and open hatch, and the result path publishes hostile spawners and a value-only cache drop. Spawner maximum-alive/interval values follow the assigned chunk's final danger band; cache loot table and ground reward follow its final reward band. A World Revision validator fixture now checks the hull, navigable interior, mast, bounded payload shape and loot table resolution. Strict MinGW warning-as-error syntax compilation and `git diff --check` pass for the touched units. Runtime validator execution and publication rollback are not verified; the remaining rare cards and broader design requirements remain open.

Generic crafting-tag implementation pass (2026-10-14): Libft `game_crafting`
now supports up to 64 stable positive tag IDs with up to eight distinct item
members each. Tags are registered before recipes; a tagged ingredient is
expanded to an immutable alternatives group when its recipe registers, so
query, preparation, and crafting share identical matching behavior. The
Minecraft recipe catalog registers its five resolved content tags (logs,
planks, crystal samples, ore samples, flower pigments), and several wood
recipes now refer to the plank tag directly. This source change has not been
compiled or runtime-validated; those checks remain deferred until the current
implementation pass is complete.

Pending-choice query implementation pass (2026-10-14): Libft now exposes a
read-only per-player pending-choice query. Its result contains stable choice
and option IDs, kind, option values, and deadline, while hiding the timeout
default and resolved state. The output is staged and swapped only after a
successful allocation. The API and README are updated; source changes have not
been compiled or runtime-validated, per the deferred verification instruction.

Legal-action query implementation pass (2026-10-14): Libft now reports the
command kinds available to a player under the current phase and mandatory
discard/choice obligations, along with per-player and global pending-choice
counts and the owner's discard destination. The query is read-only and does
not execute effects or advance RNG. Action submission now pauses other
commands while a mandatory choice remains unresolved; the choice owner may
answer even when not the active-turn player, and trusted expiry remains
available. A pending discard has priority until fulfilled. Card-specific
target legality and user-facing unavailable/no-effect reasons remain open;
the current view is the engine command layer only. Source changes have not
been compiled or runtime-validated.

Physical-card action identity implementation pass (2026-10-14): CardGame
action schema v2 now requires the exact hand instance ID for PLAY_CARD while
retaining schema-v1 compatibility. Command-log v9 persists this source ID and
reads v1-v8; replay codec v8 carries it and reads versions 1-v7. Legacy
`submit_command` play commands without a source ID retain their prior
first-matching-definition behavior, while schema-v2 action retries compare
the requested physical ID. This implementation has not been compiled or
runtime-validated.

Shipwreck validator follow-up (2026-10-14): The structure fixture now also verifies that applying a Shipwreck plan to a dry chunk returns `FT_ERR_NOT_FOUND`. A fresh direct-link attempt was not possible: the saved response file references stale Minecraft and Libft objects and fails with unresolved symbols. Direct syntax compilation of the shipwreck, loot, and validator translation units passes; runtime behavior remains unverified.

Mob behavior implementation pass (2026-10-14): `WorldAnimalSimulation` now considers nearby hostile entities when selecting a safe movement step, makes livestock flee nearby threats, and uses same-species cohesion, separation, and heading alignment for flocking. Hostile mobs within 24 blocks choose safe steps toward the player; farther hostiles retain deterministic wandering. The behavior uses the existing bounded nearby-entity query and deterministic direction ordering. Implementation is not yet compiled or runtime-validated; attack/damage behavior and verification remain deferred until the requested implementation pass is complete.

Configurable furnace policy implementation pass (2026-10-14): `MinecraftRecipeCatalog::configure_processing_recipe_policy` and its `GameSession` pre-start wrapper configure a stable fuel item, units per output, and server-tick duration independently for each furnace recipe. Defaults remain coal x1 and 200 ticks. Processing-state format v4 records all seven policies and restore rejects mismatches; legacy v1/v2 use historical defaults, and the intermediate v3 shared policy is accepted only when it matches every recipe's configured policy. Code changes are not yet compiled or runtime-validated; tests and build checks remain deferred until the implementation pass is complete.

Durable mining/drop transaction implementation pass (2026-10-14): Added
`WorldBlockMiningTransaction` and routed GameSession's primary-button mining
through it. The journal record captures the target coordinates, previous and
replacement block IDs, voxel revision, tick, owner, and up to two exact drop
stacks. The prepared record is checkpointed before changing the block or
publishing drops; the committed record and resulting session state are
checkpointed afterward. Startup recovery applies prepared mining records,
including unloaded-block overrides, and ordinary mining now records block
replacements through the persistent override API. The legacy standalone
BlockInteractor entrypoint remains for isolated fixtures. This implementation
has not been compiled or runtime-validated; verification remains deferred until
the implementation pass is complete.

Hostile attack implementation pass (2026-10-14): `WorldAnimalSimulation::tick`
now emits bounded deterministic attack events for zombies, skeletons,
creepers, and spiders when they have line of sight to the player. Damage is
applied to the session character and checkpointed with the player state; new
characters start with 20 hit points, while restored characters keep saved
health. Attack cadence is derived from the persisted world tick and stable
entity ID, so restore does not reset a process-local cooldown. This supersedes
the earlier note that attack behavior was deferred. Combat balance,
death/respawn behavior, compilation, and runtime validation remain open.

Card effect status query implementation pass (2026-10-14): GameSession now
offers a read-only query for a physical hand-card instance and proposed choice.
It reports the definition and current rules preflight result without consuming
the card or mutating rules. It reports session-phase unavailability separately
from a successful query, and returns the no-effect choice mask for the two
biome-removal effects. This first host status surface initially lacked target
enumeration; that gap is addressed by the following query implementation
entry. Card-specific reason text and full authorization and recipe-knowledge
parity with the play path remain open. Source changes are deferred from
compilation and runtime validation until the implementation pass ends.

Legal card-choice query implementation pass (2026-10-14): Regeneration rules
can now enumerate all currently valid finite biome/ore choices and no-choice
plays. Surveyor's Reach choices are filtered from a host-provided candidate
set, then GameSession rechecks each candidate against current loaded,
protected, selected and capacity state before exposing it; the GameSession
query also applies Volcano Mark's recipe-discovery and authorization gates.
Outputs are staged before replacement. Card-specific display reasons remain
open. Source changes are uncompiled and untested per the implementation-first
instruction.

Draw-notice write-ahead transaction implementation pass (2026-10-14):
GameSession now journals each positive empty-draw/hand-overflow instability
adjustment by host-intent sequence, with exact target chunk values and
expected instability revisions. It checkpoints the prepared record before
mutating instability or acknowledging the notice, checkpoints participant
state, then marks the journal record committed. Startup recovery idempotently
completes prepared records and acknowledges the notice only if it remains
pending. This closes the local save/retry crash window for these penalties;
the transaction scope retains only its latest terminal high-water record to
stay bounded. The change is not compiled or runtime-validated yet.

Surveyor's Reach status parity (2026-10-14): The proposed-choice status query
now checks the target against the same read-only World Revision bonus-chunk
validator used before play. An unselected, unloaded, protected, over-capacity,
or stale-scope target therefore reports the current host preflight error
instead of appearing legal from rules-only validation. Full parity for
card-specific structure-site and instability feasibility remains open; this
source change is uncompiled and untested.

Generic host-output delivery surface (2026-10-14): GameSession now exposes a
host-only ordered copy of pending typed outputs and a checkpointed acknowledgement
for non-policy/non-draw outputs. The sequence remains in the persisted CardGame
queue until the host reports durable acceptance; exact consumer retry is keyed
by that stable sequence. Built-in policy intents and draw penalties remain on
their dedicated transactional paths. A globally unique session/batch ID and
prepare/commit/abort coordination with external consumers remain open. This
source change is uncompiled and untested.

Rare-card status feasibility pass (2026-10-14): The proposed-choice status
query now checks the configured positive instability budget against each
selected chunk for the relevant rare effects, checks both the maximum
Volatile Stabilizer surge and modifier capacity, and requires at least one
eligible Unstable Catalyst target. Shipwreck, Pyramid, Canopy and Spider Nest
also require a currently matching ocean/desert/forest/cave site. Remaining
parity gaps include compound feature reservations, Volcano footprint feasibility,
catalogue-specific authorization beyond Volcano Mark, and reason text. This
slice is uncompiled and untested.

Host output identity follow-up (2026-10-14): Added a host-configured durable
session namespace for pending typed outputs. The host configures a nonzero ID
before start; MCSP v17 persists it with the CardGame output queue. When a caller
does not preconfigure an ID, restore adopts the ID from the snapshot. A
preconfigured ID that disagrees with the snapshot fails closed. A new world
without a configured host ID receives a deterministic nonzero seed-scoped ID,
then persists it on its first snapshot. Queried outputs
carry `(session_id, sequence)`, and the public acknowledgement API rejects an
ID from another session before checkpointing. The sequence-only acknowledgement
is private, so external consumers must use the session-checked API. Snapshot
readers retain versions 1–16; this change is uncompiled and untested.

Card status reason text (2026-10-14): Added an overload of the read-only
proposed-choice status API that returns a stable reason string alongside the
status code and no-effect mask. It explains phase unavailability, authorization
gates, missing world policy, instability limits, modifier capacity, and
missing suitable structure sites; already-excluded biome choices explicitly
report no effect. This supplies UI-facing explanation without changing the
existing API. Compound reservation checks and full play-path parity remain
open. Source is uncompiled and untested.

Read-only compound feature capacity preflight (2026-10-14): Card status now
counts accepted feature plans and queued feature-card intents against the
current selected chunk set, including Surface Magma's configured pool count
and Volcano Mark's square footprint. It also checks that already planned and
new Shipwrecks fit the selected ocean-chunk count. The status path runs the
same bounded Volcano-square placement check used by card acceptance. This
closes the aggregate-capacity false-positive from the earlier status query;
the existing worker/assignment path remains authoritative for generation-time
site and footprint validation. Source remains uncompiled and untested.

MCSP output-session persistence integration (2026-10-14): Added snapshot v17
with a fixed-width session ID trailer after crop-growth state. The normal
GameSession save/restore path now writes and restores this value, adopts saved
identity when no host override is configured, and rejects a conflicting
nonzero host override. The persistence validator source now includes a v17
roundtrip assertion and uses v18 for the unsupported-future-version case.
GameSession exposes a getter so a host can bind its transport to a restored
session even when no output is currently pending. The session clears the
previous run's restored ID at the next start boundary and reapplies only the
configured override before loading the selected world; configuring zero clears
that override. Legacy snapshot versions remain accepted by the parser. These
source and validator changes are not compiled or executed yet.

Action-session identity follow-up (2026-10-14): CardGame action schema v3
requires a nonzero session ID, and command-log v10 stores it alongside each
request ID. Replay format v9 now carries the ID per event and restores it into
command records; older replay versions still deserialize with ID zero. The
Minecraft card adapter receives the GameSession's persisted ID before card
state initialize/restore and uses it for expiry actions. Test source now
covers session identity in serialized action replay. This source has not been
compiled or run. Other adapter actions still use established command
submission and cache paths; bounded action-result history, authenticated
session ID provisioning, and host transaction coordination remain open.

Adapter action routing follow-up (2026-10-14): With a GameSession ID
configured, regeneration draw and mandatory-discard actions now submit through
CardGame `submit_action`, recording request and session identity in the engine
command log. Their existing adapter cache still validates the higher-level
source-intent and physical-card retry payload. Standalone adapter use without
a host session binding retains the legacy command path. These changes are
uncompiled and untested. Card play was subsequently routed through the
replayable host-effect action command, which carries its typed choice and
private inputs. Generic host transaction coordination remains open.

Adapter identity persistence follow-up (2026-10-14): RCS1 v7 now persists the
adapter's bound action-session ID and includes it in cached draw/discard
results. Restore adopts the stored ID when no host value was supplied and
rejects a conflicting configured ID; v1-v6 snapshots remain readable, with
legacy cached results upgraded to the bound ID. The validator's synthetic v1
and v5 fixtures now remove the v7 header field when constructing old-format
records. This implementation and the fixture edits are uncompiled and
unexecuted.

Discovery recipe parity follow-up (2026-10-14): GameSession now maps each
craft-only rare card definition to its stable Card Press recipe ID. Play
submission and effect-status queries reject a rare card while its blueprint is
locked; legal-choice queries hide those options under the same condition.
Volcano Mark retains its additional authenticated host-use authorization.
Status reasons distinguish an undiscovered blueprint from a host denial.
This source change has not been compiled or runtime-validated.

Distinct structure-site preflight follow-up (2026-10-14): Read-only card status
now computes a distinct selected-chunk assignment for all queued Shipwreck,
Desert Pyramid, Ancient Canopy and Spider Nest plans. Ocean, desert, forest
and cave eligibility masks are matched so overlapping candidate chunks cannot
be counted for two required structures. The aggregate match includes accepted
plans, pending intents and the proposed card, alongside the existing total
chunk and Volcano-footprint checks. Source remains uncompiled and untested.

Generation-time distinct site assignment (2026-10-14): WorldRevisionRegenerator
now jointly matches Shipwreck, Desert Pyramid, Ancient Canopy and Spider Nest
to distinct eligible selected chunks before assigning Shipwreck payloads. This
prevents an ocean assignment from consuming the only site for another feature.
After village chunks are reserved, the worker rematches Pyramid, Canopy and
Spider Nest against the remaining sites and uses the resulting exact chunk
indices when building payloads. The worker and status query now use the same
site categories and distinct-assignment rule; villages remain a generation-time
reservation that can make an otherwise preflight-feasible plan fail closed.
Compilation and runtime verification remain deferred until the implementation
pass is complete.

Boat ownership persistence foundation (2026-10-14): WENS v3 adds a fixed-width
owner actor ID to entity snapshots, and deserialization continues to accept
WENS v1 and v2 with owner ID zero. WorldEntityStore exposes the owner value in
entity snapshots and offers an item-entity spawn overload that records it.
Boat placement and retrieval now have actor-aware GameSession and transaction
entry points. Placement records the actor ID in the entity and in WBT journal
payload v2; retrieval rejects an actor that does not match the stored owner.
Legacy WBT v1 records remain readable for owner zero, and local wrappers keep
their prior behavior. A GameSession boat-actor authorizer must be configured
before start; actor-aware placement/retrieval reject when it is absent or
denies the identity. The callback is the host authentication boundary, so its
user data must represent the currently authenticated session. WENS restore
rejects owner IDs on entities without a physical boat item. Boat
steering/movement and production transport wiring remain open. Compilation
and runtime checks remain deferred until the implementation pass is complete.

Replayable regeneration-card play implementation pass (2026-10-14): Added a
host-effect play command that resolves the registered card event, consumes the
exact physical hand instance into the configured discard zone, and rolls the
whole operation back if effect resolution or discard fails. Typed choice,
stabilizer, catalyst, and request inputs travel in a bounded command payload;
the engine effect context receives that payload, and command-log/replay formats
now preserve it with legacy readers retained. RegenerationCardSession routes
card plays through `submit_action` when session-bound and through the sequenced
command path otherwise. Play retries now compare cached private inputs, and
RCS1 v8 persists those inputs while retaining older snapshot readers. The
validator source checks action/session/request identity and persisted effect
payload. Command-log version 13 and replay version 11 include a dedicated
destination-zone field. These edits are implementation work only; compilation and runtime
validation remain deferred until the missing implementation pass is complete.

Card-play request identity consistency follow-up (2026-10-14): The
regeneration effect callback now rejects a payload request ID that differs
from the CardGame action request ID supplied by sequenced submission or
command replay. Raw legacy command submission still relies on the payload ID.
This closes the identity split between the engine retry cache and emitted
Minecraft host intent. This implementation-only edit has not been compiled or
runtime-validated; the broader listed integration work remains in progress.

Boat placement barrier integration (2026-10-14): GameSession now rejects
placing a boat into a chunk blocked by an active World Loom movement barrier,
in addition to its existing protected-chunk check. This prevents a vehicle
from being newly introduced across the barrier through the inventory transfer
entrypoint. Steering, movement, network transport, and existing-boat recovery
remain open. This implementation-only edit has not been compiled or run.

Boat actor boundary hardening (2026-10-14): Actor-aware GameSession placement
and retrieval now reject actor ID zero and require the configured host
authorizer to approve every supplied actor. The actor-less wrappers remain for
local use; network transport must call the actor-aware methods. This closes the
zero-ID bypass in those methods, but does not provide the production identity
provider or movement hooks. This edit has not been compiled or runtime-tested.

Authoritative boat movement transaction (2026-10-14): Added an actor-aware
GameSession step API for owned boats. A step is limited to one block, requires
the destination to be loaded water with air above, rejects protected and active
barrier chunks, checks reach and expected entity revision, and persists through
the shared transaction journal before publishing. Exact request retries use
the stored endpoint and revision; failed checkpoints restore entity and
journal state. Steering input/UI and transport wiring remain open. Source
changes are implementation-only and have not been compiled or run.

Entity mutation boundary follow-up (2026-10-14): `GameSession::world_entities`
now exposes a const store only. Callers can inspect entities but cannot bypass
actor ownership, transaction journaling, or World Loom movement checks through
the GameSession accessor. Server-side simulation still owns the private mutable
store. The client input/transport integration remains open; this change has not
been compiled or runtime-tested.

Instability reward/danger opportunity planning (2026-10-14): Regeneration now
adds up to three deterministic overlay sites per selected chunk, gated by both
earned reward and danger bands. Each site gets bounded hostile spawners whose
strength follows the danger band and a one-time tiered emerald/diamond reward.
Feature-outcome IDs persist with the chunk record; on later regeneration the
structure is reconstructed without granting the same reward or duplicating its
spawners. Sites are spatially separated within the chunk and the count is hard
capped at three. This supplies the first concrete kiss/curse opportunity path;
balance/content review and validation remain. Source changes are uncompiled
and untested as requested.

Opportunity site reservation correction (2026-10-14): Generic instability
opportunities are now assigned only after card feature footprints have been
reserved. Chunks occupied by those plans or existing permanent instability
overlays are skipped, preventing the new sites from overlapping the planned
structures. This remains a content-balance placeholder with a three-site cap;
it has not been compiled or validated.

Surface reachability follow-up (2026-10-14): Opportunity planning now accepts
only flat, dry, open 5x5 sites, and the worker constructs the ruin above the
detected ground surface. Spawners and drops use that same ground-relative
position. Chunks without an available site simply receive no opportunity in
that tier instead of failing regeneration. This source change has not been
compiled or runtime-validated.

Effect callback rejection cleanup (2026-10-14): `RegenerationCardSession` now
clears its invocation-scoped request, choice, private roll, and catalyst option
state whenever payload validation or deterministic RNG fails. This prevents a
rejected/replayed callback from leaving private input available to a later
effect. Successful callbacks already cleared this state after appending the
typed output. This implementation-only change has not been compiled or run.

GameSession script policy-context wiring (2026-10-14): GameSession now accepts
a bounded sorted set of policy context keys and initial values before startup,
registers that schema before loading a fresh script catalog, and applies the
configured set/clear values after initialization or snapshot restore. Host
set/clear calls update the live bridge and immediately checkpoint active
sessions, rolling the in-memory value back if that save fails. The adapter
snapshot persists live values; a new host process must
provision its authorized schema and values again. The schema is included in
the script manifest through the existing bridge contract. A separate
host-installed authorizer approves every initial or live key/value set or
clear and fails closed when absent. Wiring that callback to the authenticated
host identity and authoritative generation-policy provider, including durable
trusted-manifest provisioning across processes, remains open. This
implementation-only source change has not been compiled or run.

Durable host-output acknowledgement retry (2026-10-14): CardGame now exposes
its persisted next host-intent sequence. RegenerationCardSession uses that
high-water mark and the pending queue to distinguish queued, previously
acknowledged, and never-issued sequences. GameSession returns success for an
exact output acknowledgement retry after the original checkpoint or after
restore, while still rejecting another session ID, future sequence, pending
out-of-order sequence, and protected policy intents. This closes the lost-ack
response retry case without adding duplicate state to MCSP. Source changes are
implementation-only and have not been compiled or run.

Boat movement network transport slice (2026-10-14): Added versioned reliable
`BOAT_MOVE_INTENT` and `BOAT_MOVE_RESULT` payloads. The server session accepts
intents only from an authenticated peer and passes that peer's authenticated
public key to a host callback; client-supplied actor IDs are never accepted.
The callback is the integration point for mapping that key to the host actor
ID and calling `GameSession::move_boat_for_actor`. The client can send an
intent and receive its result through a registered callback. The runtime now
exposes a setter for the host movement callback, including configuration after
the runtime has accepted peers. This is transport plumbing only: the production
identity resolver, input/UI, and live transport wiring remain open. No
compilation or runtime checks have been run during this implementation pass.

Boat entity fanout follow-up (2026-10-14): Successful boat move results now
carry the authoritative entity state and revision. The server broadcasts that
state as a reliable `ENTITY_UPDATE` to every other connected replication peer;
clients can route received entity updates to a host-installed callback. This
closes the server-to-peer fanout path, while the game client still must install
the callback into its entity renderer/store and provide steering input/UI.
The move callback must fill the success result with the post-commit entity
snapshot. Production public-key-to-actor mapping and live transport setup
remain host responsibilities. These implementation edits have not been
compiled or runtime-validated.

GameSession network movement adapter (2026-10-14): Added a concrete callback
that maps the authenticated peer public key through a pre-start host resolver,
rejects missing/zero actor identities, invokes `move_boat_for_actor`, and
returns the committed entity state and revision in the protocol result. The
separate boat actor authorizer still runs inside GameSession, so identity
resolution alone cannot authorize an action. A host wires
`GameSession::handle_network_boat_move` to
`WorldReplicationServerRuntime::set_boat_move_callback` and must install both
callbacks before play. Host account mapping and runtime registration are
application provisioning; the new source is not yet compiled or validated.
The boat protocol translation units are now listed in the explicit Minecraft
source graph so normal and analytics builds will include them.

Multiplayer occupancy refresh boundary (2026-10-14): `WorldRevisionServer`
now accepts a host occupancy provider and refreshes the World's occupied
chunk set before begin, selection, protection, quote, and start actions. The
World manager then applies its occupied-chunk plus eight-neighbor exclusion
and invalidates stale selection revisions. Network server initialization now
fails closed without the provider, and each regeneration action fails closed
if it fails; the host provider must return
every currently active player's authoritative chunk. The single-player
`GameSession` feed remains separate. Validator host setup and runtime checks
are deferred until the implementation pass is complete.
`GameSession::bind_world_revision_server` now wires the configured provider
into the network server. The GameSession start committer refreshes the same
provider immediately before reserving and charging. This narrows the stale
occupancy window to the provider snapshot/commit scheduling boundary; the host
must run both callbacks on its authoritative simulation owner.
The World Revision validator's server fixture now supplies an explicit empty
occupancy set so its direct network-action setup follows the new fail-closed
contract; this fixture source has not been run.

Generic CardGame host-output transaction pass (2026-10-14): GameSession now
supports a pre-start host consumer configured with prepare, commit, and abort
callbacks. Each output is encoded into the session transaction journal and
checkpointed before callbacks run; callback contracts require idempotent,
resumable prepare/commit/abort behavior keyed by `(session_id, sequence)` so a
prepared delivery can be retried after failure or restore, including after a
failed prepare was aborted. Only the ordered pending head can be delivered, and the
public acknowledgement API requires the corresponding journal record to be
committed. Recovery replays a journaled pending output and completes the
acknowledgement. Policy and draw notices remain excluded from this generic
consumer. Configured consumers replay contiguous journaled outputs in queue
order during session startup, stopping at the first newly queued output that
has not yet been prepared. A failed prepare invokes abort; an abort failure is
returned while the prepared record remains available for retry.
Starting a new session with this consumer requires an explicitly provisioned
nonzero stable session ID; restored sessions may adopt their persisted ID.
Host callback implementation, durable backend semantics, retry behavior after
real process crashes, and crash-boundary validation remain open. Source
changes have not been compiled or runtime-validated, as requested. This
supersedes earlier ledger wording that said all CardGame host-intent delivery
lacked journal coordination; policy and other internal notices still use
their own paths.

Recipe catalogue source audit (2026-10-14): the current bootstrap already
registers bread, bow, arrows, shield, staff, wood/stone/iron/gold/copper/
diamond pickaxes, axes and swords, a wood hoe, and cloth/leather/iron armor
under stable IDs 3001–3005, 3101–3126, and 3201–3224. Do not add duplicate
rows for those outputs.
The catalog's existing `validate_recipe_coverage` checks recipe-acquisition
items against registered output definitions. Still open are progression
discovery-source wiring, broader acquisition/gameplay integration, and
requirement-level runtime verification; this source audit is not a test.

Cache recipe discovery integration (2026-10-14): ruined-village cache loot
tables now map to the Ruined Village Relic recipe, and all three shipwreck
cache reward tiers map to Shipwreck Chart. GameSession registers these
authoritative loot-table sources at startup. A pending spawner death grants
the mapped recipe with a stable `(spawner_id, entity_id)` event key, then
publishes the unlock together with loot drops and death acknowledgement in one
session snapshot; save failure restores the recipe book, ground items, entity,
and spawner state. This only covers cache tables with current loot definitions;
desert pyramid, ancient canopy, and spider-nest structure cache tables now map
to their matching rare recipes. WorldRevisionRegenerator assigns these
structure-specific tables to each structure's cache spawner, rather than
reusing the generic mob table; the loot catalog supplies deterministic bounded
cache rewards. Other exploration/research unlock triggers remain open. Source
changes are not compiled or runtime-validated.

Persistent block-edit API completion (2026-10-14): Source inspection found that
the public `World::delete_block_at` and `World::place_block_at` wrappers still
called legacy edit commands directly, bypassing the saved block-override map
used by mining transactions. Both wrappers now stage and publish the matching
override only when the underlying edit succeeds. Undo/redo now stage the
restored override before changing the chunk, so those edits also survive chunk
reload and session persistence. Existing edit validation and concurrency
remain in the underlying command path. These implementation changes are not
compiled or runtime-validated; testing remains deferred until implementation
work is complete.

Persistent edit publication follow-up (2026-10-14): Undo/redo now capture the
current voxel before mutation, publish the immutable chunk read state after the
write, restore the prior voxel if publication fails, then swap in the staged
persistent override. They also invalidate a World Loom station when history
changes its block. This brings history edits onto the same immediate query and
station-invalidation path as normal edits. Source remains uncompiled and
runtime-unvalidated under the implementation-first constraint.

World Loom draft-owner checkpoint (2026-10-14): `WorldRevisionServer` now
exports and restores a bounded WRS1 checksummed state containing draft owner,
Loom ID, committed-start flag, and the world revision/selection revision it was
bound to. Restore is allowed only before server initialization and fails when
the loaded World revision differs, preventing a stale owner checkpoint from
claiming a different revision. The host must persist these bytes with its own
durable session store and restore them before accepting network traffic. The
server request-result caches and crash-safe generation publication are still
not included. Source changes remain uncompiled and runtime-unvalidated.

World Loom request-cache checkpoint (2026-10-14): `WorldRevisionServer` now
exports/restores WRC1 completed request records, quote results,
instability views, revision summaries, and exact wire payloads. Deserialization
is bounded to 4,096 entries per cache and 64 MiB, validates keys/lengths and
checksum, omits unfinished cache entries to avoid restoring a permanent
in-progress response, stages both maps, then swaps only after full validation.
Action owners still need their own recovery for an interrupted mutation. The World
Revision validator source now covers a server-cache roundtrip and checksum
rejection. The host must still persist the checkpoint after accepted actions
and restore both WRS1/WRC1 before initialization; atomic checkpointing with the
world mutation and committed generation publication remain open. No compilation
or runtime test has been run.

Physical item-definition identity policy (2026-10-14): MinecraftItemCatalog marks boats, tools/armor, physical CardGame items, and filled water/lava buckets as requiring a nonzero physical instance ID and a single-item stack. GameSession's cross-store identity check applies this policy to player inventory and every world-item entity before checking cross-store duplicate IDs. ItemCatalogValidator source covers every current tool/armor and card definition range, boat and filled buckets, plus missing-ID, stacked-item, and stackable-empty-bucket cases. WorldItemStore policy validation also checks the snapshot definition, maximum stack, and quantity against entity metadata. These edits remain source-only pending the user's instruction to finish implementation before testing/compiling. The catalog policy permits IDs on otherwise stackable items where stateful transfers need them, and uniqueness outside the player, world-item, item-entity, and known card stores depends on later store registrations; no generic multiplayer/global owner registry is claimed.

World Loom combined server checkpoint (2026-10-14): WorldRevisionServer now exposes WRP1 serialize_checkpoint/restore_checkpoint to package WRS1 owner-draft and WRC1 completed-request/replay-cache state in one checksummed bounded envelope. Restore validates both parts against the already restored World revision using temporary state, then swaps them into the uninitialized server only after both succeed. WorldRevisionValidator source checks combined roundtrip and corrupt-envelope no-mutation behavior. This is a host checkpoint payload, not automatic disk persistence or atomic coordination with the world/session save; the host must store this bundle together with the World revision and restore it before server initialization. Source-only pending the implementation-first test/build gate.

World Loom checkpoint file helpers (2026-10-14): WorldRevisionServer now saves WRP1 through Libft's atomic file replacement and loads a bounded file transactionally through restore_checkpoint. Validator source adds a file roundtrip. This gives hosts a ready checkpoint file operation, but atomic visibility is not crash durability and a separate file is not atomically coordinated with the world/session save; the host still needs to place the bundle in its world snapshot transaction.

World Loom checkpoint in GameSession persistence (2026-10-14): GameSessionPersistence snapshot v18 adds the opaque combined WorldRevisionServer checkpoint alongside World Revision metadata in the same checksummed, atomically replaced MCSP file. Legacy snapshots through v17 remain readable; legacy load APIs can ignore the optional checkpoint, while GameSession restores it through the dedicated load API and passes it into WorldRevisionServer before network initialization. Binding the server installs a checkpoint committer; completed action, quote, and instability-view cache updates ask GameSession to persist the combined checkpoint. WorldItemStoreValidator source now covers v18 opaque-state roundtrip and v19 rejection. This improves atomic visibility of session metadata and server replay state, but callback failure after an in-memory World mutation still needs crash-boundary recovery validation and a recoverable rollback policy. No compilation or execution has been performed.

World Loom start checkpoint ordering correction (2026-10-14): the successful
`GameSession::commit_regeneration_start` path no longer writes an intermediate
snapshot before `WorldRevisionServer` records the completed confirm request.
The server's existing post-handler checkpoint callback now writes the committed
session mutation and completed request/replay cache together in MCSP v18. A
crash before that atomic replacement leaves the earlier prepared-start snapshot
available for existing transaction recovery. This closes the identified
confirm-result checkpoint ordering window by source inspection; callback-error
recovery after the final checkpoint callback and crash-boundary behavior remain
unverified. The transaction now activates barriers and installs the deadline
before marking its journal record committed. If any step fails after resource
debit, rollback restores the inventory and card snapshot, clears barriers, and
aborts the still-prepared record. Builds and tests remain deferred until the
implementation pass is complete.

Card effect status reason follow-up (2026-10-14): the read-only GameSession
status query now returns an explicit reason when the decision window is
inactive or the requested physical card instance is absent from the hand,
instead of leaving the generic “Status unavailable” fallback. This improves
the host/UI distinction without changing legality or mutation behavior. Source
is not compiled or runtime-validated; checks remain deferred until the
implementation pass is complete.

World Loom checkpoint save-failure retry (2026-10-14): the GameSession
checkpoint callback now retains the latest serialized WRP1 bytes in memory
when the atomic MCSP replacement fails. The server has already advanced, so
discarding those bytes would let a later session save pair newer World state
with stale replay data. The action reports the persistence error; an exact
request retry serializes and attempts the same checkpoint again, and later
session snapshots retain the latest server bytes. This source-only change
does not guarantee durability if the process terminates before a successful
replacement; crash-boundary behavior and runtime verification remain open.

Host-output consumer lifecycle validation (2026-10-14): configuring the
consumer while a GameSession is active now returns `FT_ERR_INVALID_OPERATION`;
partial callback sets and non-null user data without a consumer return
`FT_ERR_INVALID_ARGUMENT`. This makes the pre-start lifecycle contract explicit
and prevents retaining an unused callback context. Source-only; compilation
and runtime validation remain deferred.

Recipe catalogue startup coverage gate (2026-10-14):
`MinecraftRecipeCatalog::initialize` now runs `validate_recipe_coverage` after
bootstrap registration and fails initialization if any registered item lacks
its declared recipe output or if a card output/knowledge policy is missing or
duplicated. Failure tears down the partially initialized catalogue and reports
the missing item ID. This makes the existing source audit an enforced runtime
invariant instead of a validator-only helper. The complete gameplay acquisition
and progression trigger contracts remain open; no build or runtime check has
been run during this implementation pass.

Card Press research action implementation (2026-10-14): `GameSession` now
exposes `research_card_recipe`, authorizes the Card Press station against its
coordinates and revision, and charges two Card Stock for one still-locked
recipe from the finite rare-card recipe pool. Selection uses a deterministic
session/request/inventory/world-revision seed, rejection sampling, and stores
the accepted random value and selected recipe in a versioned `RSR1` journal
payload. The prepare snapshot is persisted before inventory mutation; pending
records recover by replaying the idempotent quantity debit, granting the
registered research discovery source, and persisting the inventory, recipe
book, and committed journal together. Exact committed retries return the
recorded result and retry persistence without rerolling. The cost is a
placeholder balance value under the design's explicit balance decision. This
adds research as a functioning progression trigger; exploration achievement
triggers and multiplayer host integration remain open. Source-only; compiling
and runtime checks remain deferred until implementation work is complete.

Hostile lethal-hit recovery implementation (2026-10-14): When the bounded
server-side hostile attack batch reduces player health to zero, `GameSession`
respawns the player at the session's original safe spawn, restores the
configured initial health and armor, and preserves inventory and world state.
The respawn and character location are checkpointed with the tick's result. If
that checkpoint fails, the session restores the previous camera, health, armor,
and character location and fails the tick so the state can be retried. This selects a
no-inventory-loss placeholder death policy; source has not been compiled or
runtime-validated, per the implementation-first gate.

Paper-reed planting source audit (2026-10-14): The open item noted under the
renewable reed slice is implemented in the live selected-inventory placement
path. `PAPER_REED_FIBER_ITEM_ID` resolves to the immature reed block, and
`GameSession::place_inventory_block` requires supported dirt/grass/mud/sand and
nearby water before the normal journalled placement consumes the fiber. This
closes the planting implementation gap by source inspection; validator coverage
and runtime behavior remain deferred until the implementation pass is complete.

Recipe unlock rollback hardening (2026-10-14): `MinecraftRecipeCatalog::unlock_recipe`
now snapshots the serialized recipe book before mutating its knowledge state.
If the parallel crafting-catalog unlock fails after the book accepts the source
event, the method restores the prior book and crafting locks and clears the
newly-unlocked result. This makes loot, host discovery, and Card Press research
grants all-or-nothing at the catalogue boundary. Source-only pending the
implementation-first build and runtime gate.

Equipment definition metadata (2026-10-14): `MinecraftItemCatalog` now stores
typed equipment slot and material tier plus durability, attack, armor, and
mining values for every registered pickaxe, axe, sword, bow, shield, staff,
hoe, and cloth/leather/iron armor piece. Startup coverage requires metadata for
all 34 durable equipment definitions, ties each to a single recipe acquisition
policy and stack limit 1, and confirms that it requires a physical instance ID.
Arrows remain a stackable recipe item and are excluded from the physical-ID
range check. The numeric stats are provisional content values; they define the
catalogue contract but do not establish a final combat or balance model. The
catalogue API is not yet consumed by player equipment/combat actions. Source
only; build and runtime verification remain deferred.

Equipment durability state (2026-10-14): Newly created equipment carries its
current durability in a reserved item modifier, which follows existing item
copy, inventory, crafting-output, and world-item persistence paths. The
catalogue can read current/maximum durability and stage revision-checked wear.
Legacy equipment without the modifier is read as undamaged. Later entries
connect wear to mining and hoe tilling, and retain broken items as physical
instances with zero durability. Combat durability remains open. Source-only;
builds and runtime verification remain deferred.

Durable mining action integration (2026-10-14): The session mining path enforces axe/pickaxe class and minimum-power requirements and applies one durability wear per successful break. Its original GMIn v2 path validated the strongest usable physical tool in inventory; the later correction below changes active GameSession mining to require the authoritative equipped main-hand tool. V1 no-tool and v2 inventory-tool records remain supported for compatibility callers. Recovery verifies the recorded physical tool before applying wear, and session mining is serialized with other item actions. Source-only; compilation and runtime validation remain deferred until the implementation pass is complete.

Hoe durability transaction integration (2026-10-14): The live farmland till action now includes the physical hoe instance ID in GTIL v2 and stages one point of durability loss inside the same prepared world transaction. Missing, stale, or substituted hoe instances fail closed; transaction rollback restores inventory and soil if wear or checkpointing fails. GTIL v1 records remain readable and replay without retroactive wear. Session recovery now supplies the live character and item catalogue so v2 prepared records can complete the hoe wear step. Exact committed request retries return from the journal before checking the already-advanced inventory revision. Source-only; compilation and runtime validation remain deferred until the implementation pass is complete.

Broken-equipment retention and repair staging (2026-10-14): Equipment reaching zero durability now remains as the same physical inventory instance with the reserved durability modifier set to zero, rather than disappearing. Durability queries expose zero as a broken state, while further damage is rejected. `MinecraftItemCatalog::stage_equipment_repair` now creates a revision-checked snapshot that restores a damaged item to its catalog maximum without mutating the live inventory. The action still needs a material-cost policy, authorized station, journal integration, and user-facing broken state; source-only, no builds or tests run.

Journalled equipment repair action (2026-10-14): Added `WorldEquipmentRepairTransaction` and a `GameSession::repair_equipment` action. The server derives a one-unit repair material from the catalog's equipment tier, requires the authoritative workbench station callback plus a matching live workbench block/chunk revision, and stages material consumption with full durability restoration in one inventory snapshot. GRPR v1 stores request, inventory revision, slot, item and physical instance IDs, and repair cost; prepared actions recover during session startup and exact committed retries are idempotent. The transaction source is registered in the explicit Minecraft build graph. Material costs are placeholder values; UI exposure and a full equipped-stat pipeline remain open. Source-only; no compile or runtime validation has been run.

Equipment repair gameplay integration follow-up (2026-10-14): The repair path is now live through `GameSession::repair_equipment`. GRPR v1 binds the station coordinates and revision as well as the inventory/tool/material identity, so changed-payload retries are rejected; committed and prepared retries consult the journal before station checks, preserving idempotency after station removal or restart. New requests require the host's WORKBENCH station authorization and a matching current workbench block/chunk revision. The catalog derives a one-unit tier-matching material and stages its consumption together with full durability restoration as one inventory revision. Startup recovery dispatches prepared repair transactions, and the explicit source graph includes the transaction implementation. UI rendering, broader equip-derived combat stats, and provisional repair-cost balancing remain open. No compilation or runtime validation has been run.

Equipment repair quote API (2026-10-14): Added a read-only `GameSession::query_equipment_repair` result for authorized Workbench UI flows. It reports the selected physical item's current/maximum durability, server-derived material cost, available matching stackable materials, and whether repair is currently possible; it consumes no inventory and returns no partial output on failure. Source-only; no builds or tests run.

Repair rollback recovery hardening (2026-10-14): `WorldEquipmentRepairTransaction` now retains a serialized PREPARED journal image while applying the staged inventory snapshot. If rollback restores the inventory, the journal returns to its prior state; if inventory restoration itself fails, the prepared intent is kept for startup recovery. Rollback, journal restoration, and compensating checkpoint errors are propagated instead of silently ignored. Source-only; verification remains deferred.

Equipment repair material coverage gate (2026-10-14): Catalog startup validation now requires every durable equipment definition's maximum durability to fit the persisted signed modifier and verifies its tier-derived repair material resolves to a registered stackable item. This prevents publishing a repair quote/action for equipment whose recovery cost cannot be fulfilled. Source-only; builds and validation remain deferred.

Bootstrap progression source audit (2026-10-14): The registered initial recipe book grants the basic progression chain directly: all three plank types, sticks, workbench, furnace, paper mill, paper/card stock, and Card Press are initially known. The core station recipes are therefore not missing implementation; the known-but-locked set is limited to explicitly advanced recipes. No catalogue change was needed.

Equipment slot foundation follow-up (2026-10-14): Source review found Libft's legacy character equipment component already supports head, chest, and weapon, but lacked legs, feet, and off-hand slots required by Minecraft's six-slot catalog metadata. Added those three slots and wired them through equipment initialization, destruction, move, equip, unequip, and item lookup; the legacy character save/load codec now stores these items while old saves with no extra slot fields continue to load as empty. This is a source implementation only: the Minecraft session still needs catalog-validated equip/unequip transactions, physical inventory transfer and persistence, and derived combat/mining stats. No build or tests were run under the implementation-first gate.

Revision-checked equipment transfer support (2026-10-14): Added `game_inventory::copy_item_at_revision`, which only returns a shared handle to the exact inventory item when the captured inventory revision still matches under inventory/map locks. This preserves the item's physical instance ID and modifiers for a loadout transfer without exposing the mutable inventory map. The following entry adds the Minecraft session transaction and catalog slot validation. Source-only; builds and tests remain deferred.

Journalled Minecraft loadout actions (2026-10-14): Added `GameSession::equip_inventory_item` and `unequip_equipment_item`, backed by `WorldEquipmentLoadoutTransaction` (GLEQ v1, journal type 15). Equip checks the catalogued slot, a unique physical instance, and nonzero durability; replacement returns the old instance to inventory. Unequip returns the same equipped item instance. Both actions bind request ID, inventory revision, slot and physical identity in the journal, checkpoint PREPARED before mutation, commit inventory plus character equipment in the same session snapshot, and recover prepared actions at startup. Session identity validation and output-ID allocator synchronization now include equipped item IDs; restored loadouts are checked for catalog and slot validity, and legacy equipped items without physical IDs are assigned IDs during startup migration. The transaction source is in the explicit build graph. Main-hand attack stats and combat wear remain open. Source-only; compilation and runtime validation remain deferred.

Equipped armor application (2026-10-14): Hostile physical hit resolution now sums catalog armor points from equipped head/chest/legs/feet pieces and subtracts that flat reduction while preserving at least one damage for a connected hit. Broken armor contributes zero. Shield guard remains an explicit separate action and does not grant passive reduction. This uses the equipment rules in the live session tick; source-only and unverified, with armor wear and attack-stat combat actions still open.

Combat baseline alignment (2026-10-14): The session's initial and respawn health now matches the roadmap's 100-HP combat model; armor content values are aligned to the provisional cloth/leather/iron totals of 5/10/20. These are implementation values from the current roadmap fixture, not balance validation. Source-only; no compilation or tests have run.

Equipped armor combat wear (2026-10-14): Resolved hostile hits now apply
durability loss to each equipped, non-broken armor piece, bounded at zero.
The wear is saved with the same session checkpoint as the health result; a
failed checkpoint restores the previous durability along with health and
character position. This closes the hostile-hit armor wear slice by source
inspection. Wear balance and behavior are unverified, and player attack/weapon
combat plus an active shield guard action remain open. No tests or builds have
been run, following the implementation-first gate.

Shield guard implementation pass (2026-10-14): GameSession now exposes a
server-side guard toggle that requires a live off-hand shield, caps each guard
window at 40 simulation ticks, and enforces a 20-tick recovery after release
or expiry. Hostile attack events now carry their source position; attacks
within the shield's 120-degree forward arc receive the catalogued flat shield
reduction, and each blocked hit wears the shield once. Shield wear is saved
with the same checkpoint and restored if that checkpoint fails. The guard is
transient and is not restored as active after reload. Weapon attacks,
projectiles, full-set bonuses and derived-stat caching remain open. Source-only;
no build or runtime validation has been run.

World Loom chunk-commit checkpoint pass (2026-10-14): `World` now accepts the
GameSession checkpoint callback for regeneration commits. After a generated
chunk, its instability provenance, feature spawners and loot drops are
published, the session snapshot is atomically saved before the commit is
accepted. If that save fails, the prior chunk/read state and instability record
are restored, the newly added spawners are removed, and staged drops roll back.
The instability store now has a revision-checked single-record restore for
this rollback. This closes the live checkpoint-failure window for those session
participants. The snapshot still does not store generated voxel arrays;
deterministic restart replay/publication recovery and crash-boundary validation
remain open. The applier now recognizes an already checkpointed matching
generation by its rules hash, generation epoch and feature-outcome IDs, and
does not publish its spawners or loot again when that pending revision is
replayed after restart. Source-only; compilation and tests remain deferred.

Card-session action-history bound validation (2026-10-14): RCS1 persistence
now enforces the shared 4096-entry limit across play, draw, discard, and
choice-expiry result caches during both serialization and deserialization.
This keeps malformed snapshots from multiplying the intended bounded result
history across action categories. The cache remains a fixed-retention history
that returns FULL when exhausted; eviction and authenticated session-ID
provisioning remain open. Source-only; tests and compilation are deferred.

Script policy-context restore correction (2026-10-14): GameSession now applies
host-configured script-context defaults only when it creates a fresh card
session. On restore, the RCS1 snapshot's persisted values remain authoritative,
so authorized runtime changes survive restart instead of being overwritten by
startup defaults. Startup also compares the restored registered key IDs with
the host-configured schema and rejects a mismatch before making the session
active. The existing script-context authorizer still gates updates. Persistence
regression coverage and runtime verification remain pending under the
implementation-first gate.

Choice-expiry retry payload binding (2026-10-14): `resolve_expired_choices`
now compares a retry's authoritative epoch with the epoch stored for its
request ID. An exact retry returns the retained resolution result; changing
the epoch under the same ID returns `FT_ERR_INVALID_ARGUMENT`, matching the
changed-payload rejection contract for other session actions. Regression
execution and compilation are deferred until implementation work is complete.

Mining transaction rollback error propagation (2026-10-14): The prepared
mining transaction now checks cleanup results when drop creation or tool-wear
staging fails. It attempts to remove every newly spawned drop and restore the
original block or unloaded-cell override, and returns a rollback error if any
participant could not be restored. The journal remains PREPARED so startup
recovery can finish the operation. Source-only; tests and compilation remain
deferred.

Equipped-tool mining correction (2026-10-14): The active GameSession mining
path now reads the main-hand physical tool, checks its catalogued class/tier
and current durability, and no longer accepts a stronger pickaxe/axe merely
because it is somewhere in inventory. Mining transaction v3 records the
equipped-tool sentinel and identity; checkpoint recovery verifies the same
equipped instance and applies one wear point. Compatibility callers can still
use the v2 inventory-tool record. No tests or builds have been run.

Revision-checked entity damage primitive (2026-10-14): `WorldEntityStore` now
provides `apply_damage`, an atomic revision-checked operation for live
non-player, non-boat entities. Damage is positive and lethal damage sets health
to zero and the action state to DEAD in the same revision update. This is a
storage primitive used by the journalled melee action below. Its companion
`rollback_update` restores the prior entity value and revision only when the
current revision still matches the transaction result. Source-only; testing
and compilation remain deferred until the implementation pass is complete.

Journalled player melee action (2026-10-14): `GameSession::attack_entity` now
checks active-session and regeneration state, target revision/liveness, 3.5
block reach, a 55-degree forward arc, and solid-block obstruction. It requires
a catalogued main-hand item with positive attack power and nonzero durability,
applies catalog damage and one durability point, and checkpoints the entity,
equipment and committed request result together. Entity compensation is
revision-checked and restores the prior revision exactly. Exact request retries return
the persisted health/revision outcome; changed target or revision payloads are
rejected. Failed checkpointing compensates entity state, weapon durability and
the journal. Network hosts now have a fail-closed `attack_entity_for_actor`
entrypoint whose pre-start authorizer scopes replay IDs per authenticated
actor; direct `attack_entity` is the local authoritative entrypoint. A
configurable simulation-tick interval (default 10, accepted range 1–200) now
limits successful attacks; retained exact retries bypass the cooldown and do
not repeat damage or wear. The accepted simulation tick is persisted in the
committed action record, and session restore derives the remaining cooldown
from the restored world tick.
The local primary-click path now chooses the nearest live entity within a
narrow camera ray before falling back to block mining; excessive nearby entity
counts fail closed instead of selecting from a truncated query.
Projectiles, armor-set bonuses, full loot/reward integration verification,
and runtime coverage remain open. Source-only; no tests or builds have been
run.

Projectile simulation foundation (2026-10-14): Added a bounded authoritative
projectile tick for persisted projectile entities. It advances each projectile
at most once per server tick, applies gravity, checks loaded terrain and nearby
live non-player entities, applies revision-checked damage, expires projectiles
after a bounded lifetime, and checkpoints entity/item changes together. A
projectile in an unloaded position is held until terrain is available instead
of being discarded. The tick is wired into GameSession and the source is in
the explicit build graph. The bow launch action and collision-path sampling
were completed in the follow-up entries below. Projectile tuning and runtime
verification remain open. Source-only; compilation and tests remain deferred
until the implementation pass is complete.

Journalled bow launch action (2026-10-14): Added a GameSession bow action and
right-click integration when a bow is equipped. It requires a live catalogued
main-hand bow, one stackable arrow, a matching inventory revision, and an
available slot under the active-projectile cap. The action stores the
authoritative camera pose and launch tick in a PREPARED journal record before
consuming the arrow, wearing the bow, and spawning a projectile; the committed
checkpoint contains all three changes. Exact committed retries are
idempotent, changed inventory-revision retries are rejected, and prepared
actions recover from the stored launch pose after session restore. A per-actor
10-tick cooldown is derived from journal records, and projectile damage comes
from the equipped bow's catalogued attack power. Projectile terrain and
entity collision now sample the bounded movement segment to avoid tunneling.
The source is registered in the explicit build graph. Projectile balance and
implementation-pass testing/build/runtime verification remain open.

Functional armor-set bonus (2026-10-14): Hostile-hit mitigation now grants a
provisional +2 armor points when all four head/chest/legs/feet slots contain
functional armor of the same catalogued material tier. Missing, broken, or
mixed-tier pieces disable the set bonus; each piece's normal armor value still
applies independently when functional. The bonus flows through the same
authoritative mitigation and checkpoint path as equipped armor. The +2 value
is an implementation constant pending balance work. Source-only; tests and
compilation remain deferred until the implementation pass is complete.

Equipped-armor derived-stat cache (2026-10-14): GameSession now caches the
catalog-resolved equipped armor total, including the complete-set bonus, using
an exact fixed-size fingerprint of each armor slot's item ID, physical
instance ID, and durability modifier. Equip/unequip, replacement, breakage,
repair, and durability changes alter the fingerprint and trigger recomputation;
the cache is transient and is rebuilt after restore. The cache is read on the
authoritative hostile-hit path. Shield reduction and offensive/mining stats
are not included yet. Source-only; tests and compilation remain deferred until
the implementation pass is complete.

Physical-instance uniqueness ledger correction (2026-10-14): The earlier
item-catalogue row said CardGame physical card IDs were only included in the
allocator high-water mark. Current `GameSession::validate_physical_item_identity_uniqueness`
collects card IDs from every CardGame zone and passes them to the shared
inventory/ground/entity duplicate scan; equipped IDs are included as well.
Startup and session checkpoints invoke this validation. This closes the
documented cross-store duplicate-scan gap by source inspection; it does not
establish a process-global registry across multiple independently running
sessions. Compilation and runtime validation remain deferred.

Ground-item visibility overlay (2026-10-14): Added a bounded render-only
WorldItemStore query that includes published items during owner pickup-lock
windows while continuing to hide pending transaction drops. VoxelRenderer now
projects nearby item locations into simple occlusion-checked colored markers
in both GPU and software modes; GPU debug overlay caching remains active while
item markers refresh with camera movement. This supplies visual world presence,
not item-specific 3D meshes, labels, or multiplayer item replication. Source
only; compilation and visual/runtime verification remain deferred.

Ground-item replication snapshot path (2026-10-14): Added a bounded
`WORLD_ITEM_SNAPSHOT` protocol message, server-side per-chunk snapshot creation
from published `WorldItemStore` entries, reliable snapshot delivery when a
chunk is requested, and a client runtime callback that validates session/world
identity before dispatch. The wire record includes physical item identity,
modifiers and position, and excludes owner-lock identity. Added a bounded
`WorldItemReplicaStore` that transactionally replaces one chunk's replica and
can be attached to the client runtime without a custom callback. Empty item
stores yield empty snapshots. `WorldItemStore` now advances a visible-change
revision on publish, despawn, merge, and pickup. The server refreshes subscribed
chunk snapshots when that revision changes, with a 64-snapshot per-tick budget
and round-robin peer/chunk cursors; this carries spawn, merge, pickup, and
despawn changes through the replica replacement path. These updates use full
chunk snapshots rather than item-specific deltas, and reconnect/revision race
coverage remains open. Source only; compilation and network/runtime
verification remain deferred until the implementation pass is complete.

Main-hand combat-rule cache (2026-10-14): `GameSession` now caches the
catalog-resolved main-hand equipment rule used by melee attacks, keyed by item
definition, physical instance ID, and durability modifier. A wear, break,
repair, or equipment replacement changes the key and forces a fresh lookup;
the cache contains the full rule including attack and mining power. Melee uses
the cached rule; mining still resolves its tool requirements in the journalled
mining transaction and is not yet connected to this cache. Source only; tests
and compilation remain deferred until implementation work is complete.

Immutable inventory recipe-query slice (2026-10-22): Libft now exposes
`game_crafting::query_recipe_at_station_snapshot(...)`, evaluating ingredient
alternatives, rarity allocation, station eligibility, and deficits from a
caller-captured `game_inventory_snapshot` without live inventory access or
mutation. It validates slot/weight aggregates, stack bounds, and duplicate
physical IDs while respecting the inventory's unlimited-capacity and
`USE_INVENTORY_SLOTS` configurations. Minecraft's aggregate
`MinecraftRecipeCatalog::query_recipes(...)` captures inventory once and uses
that same snapshot for all rows; its ItemCatalog validator exercises a
craftable filtered query. Focused Libft query tests pass (4/4), `make -j4 normal`
builds both normal and analytics executables, and `--validate-item-catalog`
passes on both. The full Libft suite passes 6,507/6,507 from its supported
working directory outside the sandbox. `make validate-all` was attempted but
failed at the block-edit emissive-delete convergence timeout before the full
suite completed; this is outside the recipe-query change and remains open.
Concurrent recipe-book/catalog revision
consistency and broader query-result versioning remain outside this change.
