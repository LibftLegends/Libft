# CardGame, Scripting, Inventory and Recipe-Book Integration

Status: implementation in progress. The design remains the contract; the
implementation ledger below records only verified slices and must be kept
current until every required Libft and Minecraft item is implemented.
Date: 2026-10-02.

Reviewed branches and fetched revisions:

- Libft: `agent/compression-analytics-cardgame-scripting`, `4922eb88a4ebe571d4bb0b65af5f4f84e5b24dcb`.
- Minecraft: `agent/analytics-performance`, `f3ccaf988d97d12e76e8877be31f6a6eeb26d614`.

### Implementation ledger (2026-10-02)

These entries describe the current uncommitted Libft worktree, not the pinned
branch commits above. They do not imply that the full design or Minecraft
integration is complete.

| Slice | Current status | Evidence / remaining contract |
|---|---|---|
| Typed host-intent schemas and transactional callback output | Implemented in Libft | Focused host-intent tests cover schema validation, ordered acknowledgement, and rollback when a callback fails. |
| Pending host-intent queue in in-memory snapshots/deltas and state hash | Implemented in Libft; state format v2 | `test_card_game_host_intents_survive_snapshot_delta_ack_and_revert` passes. Byte-level persistence and serialized command/replay delivery are still absent. |
| Script effect adapter | Minimal Libft bridge implemented; manifest fingerprint added | `emit_host_intent` is type/range checked and uses the event transaction. `scripting_engine::get_native_manifest_hash` hashes canonical ordered native IDs/names; the bridge `get_manifest_hash` also hashes event bindings and serialized script programs, excluding pointers. Five focused manifest tests pass. Context/query/RNG natives, rules/content manifest integration on session import, persistence compatibility, and callback unregistration/lifetime ownership remain open. |
| Hand-instance discard and policy-aware draw | Libft primitives implemented; full session/action contract remains open | `CARD_GAME_INTENT_DISCARD_HAND` records physical instance and destination. `configure_draw_policy` validates owner-scoped discard/burn zones and registered two-U32 host-event schemas. Draw actions accept a bounded count up to 100, process each attempt independently, reshuffle discard through persisted engine RNG, burn each physical instance drawn while the hand is full, and queue typed empty/overflow host intents. Command-log v5 preserves draw count and accepts v1–v4. Mandatory discard obligations are represented in snapshots, deltas, hashes, command records and replay; while pending, only the obligated actor can discard a real hand instance to the configured discard zone. Empty-hand resolution creates no impossible obligation. Command-log v6 reads v1–v5. Required discard configuration is independent of whether the deck may reshuffle. Focused CardGame tests cover the behavior; rerun after the final independent-reshuffle regression. Byte-level session persistence, action expiry/default-choice policy, and atomic inventory/host journal coordination remain open. |
| Request-idempotent action submission | First Libft slice implemented; complete action contract remains open | `card_game_action` and `submit_action` retain accepted request IDs in version-4 command records, return the committed state hash for exact retries, reject a reused ID with a different command, and preserve IDs through command-log serialize/deserialize and deterministic command replay. Focused test `test_card_game_submit_action_is_idempotent_and_serialized` passes. Session IDs, full action kinds/choice payloads, bounded result history, durable session snapshot encoding, and host transaction coordination remain open. |
| Inventory add and transfer transactions | Libft API implemented; durable transaction layer remains open | Single-item `add_item` stages deep-copied stacks and publishes only on success. `transfer_to` verifies both expected inventory revisions, stages source/destination contents, enforces slot/weight limits and preserves item metadata before publishing; successful removals and transfers advance revisions. Focused transaction suite passed 68/68, including opposite-direction concurrent transfers. Public mutable `get_items()` and shared mutable item handles can bypass revision tracking; callers must externally serialize such mutation. Durable transaction IDs, saved recovery journal, and atomic coordination with host intents remain open. |
| Recipe book persistence/default grants | Foundational Libft API implemented | `game_recipe_book` registers stable numeric IDs, seeds caller-configured default unlocks, records idempotent grants by nonzero source-event ID, and imports/exports sparse version-1 state transactionally. Unknown IDs are rejected while preserving book and input position; forward-compatible quarantine/migration remains open. Eight focused tests pass. |
| Recipe unlock, read-only query and inventory crafting transaction | Libft foundation implemented; richer recipe contract remains | `game_crafting` supports explicit locked registration, recipe-owned output definitions, batch `prepare_craft`/`commit_craft`, stale-inventory rejection, and non-mutating `query_recipe` with cumulative rarity-aware ingredient deficits. Focused tests cover locked/missing recipes, duplicate inputs, output-full rollback, batch overflow, recipe-owned outputs, stale prepared state, query deficits and query non-mutation. Station context, tags/alternatives/byproducts/timed processing and Minecraft's complete item/recipe catalog remain open. |
| Minecraft revision selection and opening-deal price quote | Partial API implemented; no session commit/debit | Existing `select_revision_chunk(x,z,bool)` supports select/unselect; `update_revision_chunk_selection(expected_revision, maximum_selected_chunks, ...)` adds compare-and-set selection, enforces a selection cap, requires a loaded, unprotected chunk, and reports the current revision. Actual selection and protection/eligibility changes advance a 64-bit selection revision, exposed by `world_revision()`, so stale price quotes are rejected. `query_revision_start_cost(expected_selection_revision, ...)` rejects stale quote requests; the read-only query uses the server-held selected set and a supplied policy with currency item ID, validates 4–7 cards, performs checked arithmetic/cap checks, and returns separate chunk/opening/total currency costs plus both revisions. The validator now exercises cap, stale revision, protection invalidation and duplicate-selection behavior; rerun it after the latest assertion correction. Request-ID deduplication, actor/loom authorization, occupied-player exclusion rings, pricing-policy revisioning, player balance, CardGame startup/deal, reservation/barriers, and atomic `commit_regeneration_start` remain unimplemented. This is an API contract only; no UI is included. |
| Minecraft card catalogue, action/session integration, regeneration, instability, inventory and recipe contents | Not implemented by this slice | Continue with the ownership and test split in Sections 5–10; update the Minecraft roadmap as each slice is integrated and validated. |

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

### Current push batch (2026-10-02)

Implemented in this batch, still without any UI:

- Libft CardGame: typed host intents, transactional effect callbacks, intent
  snapshots/deltas and state hashing; stable scripting-native/content manifest
  hashes; request-idempotent action submission; bounded multi-draw; discard
  reshuffling, overflow burn and empty-draw intents; and physical-instance
  mandatory-discard obligations. The inventory agent reports 68/68 focused
  transfer tests; the discard agent reports 47/47 focused obligation tests.
  The latest change separates mandatory discard from the independent
  reshuffle policy and adds a regression case, but that new case has not yet
  been run. Recipe book, recipe query, and transactional crafting foundations
  are present; the detailed limits remain in the ledger above.
- Minecraft API: loaded-chunk selection with compare-and-set revision and a
  selection limit, explicit unselection, stale-quote detection, and a
  read-only, overflow-checked quote for one selected-chunk cost plus a 4–7
  card opening deal priced at `B4, 2*B4, 4*B4, 8*B4`. Protection changes
  invalidate the selection revision. This is not yet a committed start
  transaction and does not start/deal a CardGame session.
- Cross-platform diagnostic format fixes use Libft's integer-format macros.
  `make -j4 analytics` succeeds after those fixes and the current API changes.
- The current `./ft_vox_analytics --validate-world-revision` run passes and
  reports `world-revision: ok regenerated=1 skipped=0`. The validator was
  corrected to exercise duplicate selection, expect unloaded chunks to be
  rejected, and avoid assuming an unrelated chunk must be skipped.

Known validation failures and unfinished work:

- The latest `make -C Libft -j4 test-executable` reaches the final
  `Test/Full_Libft_test.a` archive step and fails because `ar -M` reports its
  process-specific temporary archive (`.tmp.11`) missing. The test executable
  therefore does not contain the just-added independent-reshuffle regression.
  Investigate/fix the archive staging race/recipe, then rebuild and run the
  focused CardGame, inventory, crafting, recipe-book, scripting, and full test
  suites. Do not treat earlier agent-reported focused results as verification
  of the latest combined tree.
- The quote/selection API still has no per-session actor/World Loom identity,
  request-ID deduplication, occupancy/exclusion-ring checks, authoritative
  pricing-policy revision, dust balance/debit, CardGame startup/deal,
  persistent resource/chunk reservation, glowing barriers, multiplayer
  deadline, or atomic non-cancellable start commit.
- Libft still lacks durable session and host-intent delivery journals,
  complete CardGame action/choice/expiry policy, the full Minecraft card
  catalogue/effect adapter, and the remaining inventory/crafting contracts.
  Minecraft still needs its item/recipe catalogues, card content and effect
  integration, chunk-local instability persistence/generation, multiplayer
  regeneration lifecycle, and end-to-end failure/recovery validators.
- Keep the design and roadmap active. Move this design document to
  `Libft/Docs/old` only after the complete design contract has been implemented
  and verified. API/UI boundary: this work defines backend APIs only; all UI
  layout and implementation remain explicitly out of scope.

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
| `game_recipe_blueprint`, `game_crafting::register_recipe`, `craft_item` | Reuse recipe infrastructure | Book unlocks, station constraints, output quantities/byproducts, transactional processing |

The current crafting path consumes ingredients before `inventory.add_item(result)` succeeds. It must be replaced/extended transactionally, not called directly as the authoritative crafting action. Audit repeated ingredients, rarity-only matching versus consumption, mutable map iteration during removal, and caller-supplied output objects. A recipe must determine its own outputs; the client never supplies a result item. Preserve old API compatibility with a documented wrapper where possible.

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

`card_game_action` includes schema version, session ID, actor ID, request ID, command sequence, expected state sequence, action kind, card **instance** ID, choice ID and stable selected option IDs. Definition ID alone cannot identify the physical card to consume. Validate sizes before allocating. Request retry returns the stored result; mismatching payload under the same request ID is rejected.

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

Mandatory choices can span actions: persist a resolving-card record, queued follow-up effects, and obligation. Do not retain callback stack frames across frames. While a discard is pending, only permitted choice/timeout actions are accepted. Confirm waits until the engine resolves obligations.

## 4. Deck, hand and discard rules

Minecraft policy: initial owned deck 20 cards; configurable accepted deck size 20–100; maximum four copies per definition; hand cap 10. Audit existing engine deck/zone limits against the 100-card maximum before choosing storage. Higher library hard limits do not change gameplay limits.

Zones: draw pile, hand, resolution, discard, burned-for-session, and optionally consumed external cards. A physical deck card keeps one ID through every zone. Session burns prevent repeated draw/burn cycling, but do not silently delete a player's permanent collection. Permanent consumption of crafted items is an explicit separate economic rule. Deck-copy validation includes cards across all deck zones and any persisted deck membership.

Draw procedure resolves one requested draw at a time:

- Draw from the draw pile using engine RNG/order.
- If the pile is empty and discard reshuffling is allowed, shuffle the eligible discard once using persisted RNG, then retry. Burned/resolving cards are excluded.
- If no eligible card remains, record an empty-deck attempt and emit the configured instability event; do not fabricate a card or recursively draw forever.
- If the hand is already full, move the drawn instance to the burn zone and emit overflow instability. Do not simply return the existing `FT_ERR_FULL` and lose the draw.
- A normally successful draw moves that instance into hand. Each event has its own stable identity and exact resulting counts.

This is the proposed explicit resolution of roadmap reshuffle wording: update the roadmap to the same policy at implementation. Instability amounts remain versioned Minecraft configuration. Empty-draw and overflow costs accumulate alongside a card's own instability cost.

For Double Draft, move the played instance to resolution first, resolve two draws, then require one discard from the resulting hand. If the hand is empty because no draws succeeded, resolve the impossible obligation with the documented no-card outcome; otherwise require exactly one valid owned instance. Discard is not burn and does not grant another draw. Expiry selects a deterministic valid default, never a random UI choice.

Craft-only cards are not inserted into the starter draw pool. Designate an authorized inventory/reserve source with one-use session consumption. If later allowed into decks, it must be an explicit world policy and still respect copy/size constraints. Ownership transfer removes the item from its source exactly once, not copies its payload.

## 5. Minecraft regeneration adapter

Suggested consumer files (new names are proposals): `src/cards/RegenerationCardCatalog.*`, `RegenerationCardSession.*`, `RegenerationEffectHost.*`, `src/inventory/ItemCatalog.*`, `src/crafting/RecipeCatalog.*`, and focused validators. Integrate existing World Loom/session and generation classes instead of creating competing state machines.

Expose the start flow through the Minecraft-owned World Loom/session service,
not through `card_game_engine` (which must not know chunk coordinates or dust
economy). Proposed calls are:

| API | Contract |
|---|---|
| `begin_regeneration_selection(actor, loom_id, request_id, session_out)` | Authorize use of this loom and open a bounded, revisioned selection draft. It reserves neither chunks nor resources and does not yet start the CardGame session. Exact retry returns the same draft. |
| `select_regeneration_chunk(session_id, actor, coordinate, expected_revision, request_id, result)` | Add one chunk if it is in bounds, known/eligible, unprotected, unoccupied, outside every occupied-player exclusion ring, not already reserved, and below the selection cap. Increment selection revision only on change. |
| `unselect_regeneration_chunk(...)` | Remove exactly the requested selected coordinate; duplicate retry is idempotent and cannot remove a different selection. |
| `query_regeneration_start_cost(session_id, actor, starting_card_count, result)` | Read-only calculation from the server-owned selected set and versioned pricing config. Return paid/bonus chunk counts, chunk cost, starting-card cost, total, currency ID, selection revision and rejection reason; no dust/deck mutation. |
| `commit_regeneration_start(session_id, actor, expected_revision, starting_card_count, request_id, result)` | Revalidate everything, reserve chunks, debit/reserve exact dust, initialize the CardGame with the requested opening hand, persist the idempotency result and install barriers in one recoverable transaction. Exact retry returns the same outcome. This is the non-cancellable start boundary. |

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

Lifecycle: validate selection → reserve target chunks and resources → install visible glowing barriers → draw cards/open five-minute multiplayer decision window → resolve card actions → freeze rules → generate off-thread → revision-checked publication → remove barriers.

Occupied chunks and all eight adjacent chunks are ineligible until players vacate; validate again atomically when reserving. Selection expansion from cards repeats occupancy/protection checks. Barriers reject entry, teleports, vehicles and other authoritative movement paths, not just walking. Existing players cannot be trapped inside. Multiple sessions cannot reserve overlapping chunks or unsafe adjacent areas.

The default is no player cancellation once regeneration has started; implement the exact server lifecycle boundary and communicate it before resource spending. Volatile Stabilizer cannot reopen cancellation. Five minutes is measured by authoritative monotonic server time with restart recovery, not a user clock. Expiry resolves pending choices by defaults and generates with all accepted effects; spent resources are not refunded. Server safety failures use recoverable abort/repair rules, not a player-facing reroll mechanism.

Generation never queries a mutable session. Feature footprints are reserved atomically, including required halo; multi-chunk structures cannot half-publish. Stale results cannot replace current chunks. Rendering priorities remain accepted block edits first, light updates second, new chunk publication third; this design does not move generation/lighting into the render loop.

Persist per chunk: initial instability (default zero, optional world challenge setting), current instability, permanent modifier IDs/versions, feature outcome IDs, generation revision, rules hash and reward/danger bands. No floating global number may substitute for per-chunk records. Kiss/curse tables provide real reward opportunities coupled to danger, monotonic tiers and hard caps; increased instability does not hand out unconditional free loot.

## 6. Complete current card catalogue and call mapping

Every row registers one stable definition and effect ID via existing registration APIs. Gameplay enters through proposed `submit_action(PLAY_...)`; generic draw/discard operations stay in Libft, domain intent implementation stays in Minecraft. Names below are typed proposed **host intents**, not existing Libft functions. Parameters must be validated IDs, quantities and versioned bounds.

### Starter deck: exactly 20 instances

| Card / copies | Ordered effect and host calls | Validation / no-effect behavior |
|---|---|---|
| Biome Exclusion / 4 | Choice selects one biome; emit `biome_remove(id)` | Remove only from current eligible list. Already absent has no effect; reject removal of the final viable biome. UI states the no-effect reason |
| Deep Seam / 4 | Choice selects ore; emit `ore_scale(id, 3, 2)` | Increase **current** selected spawn rate by 50%, checked integer rational arithmetic and configured cap, not ambiguous extra ore count |
| Village Charter / 2 | Emit `feature_add(village, 1)` | Each use adds another village plan; validate full safe footprint/cap. Do not collapse multiple uses to a boolean |
| Surface Magma / 2 | Emit `feature_add(surface_lava_pool, configured_count)` | Surface-only supported, bounded pools; reserve feasible positions, contain fluid, exclude protected areas |
| Double Draft / 2 | Emit generic `request_draw(2)`, then `require_discard(1)` | Hand-cap burns and empty draws apply; required discard blocks further normal actions |
| Biome Compass / 4 | Emit `biome_weight_adjust(configured_set)` | Only currently eligible biome IDs; cannot restore excluded biomes implicitly; normalize deterministically within caps |
| Surveyor's Reach / 2 | Emit `target_scope_add(1)` and required chunk choice | One additional chunk this session, subject to occupancy/protection and budget; reserve before accepting |

The starter multiset is `4 + 4 + 2 + 2 + 2 + 4 + 2 = 20`. Cards with targets offer stable server-generated choices. Reject impossible structural guarantees before consuming their card; optional generation rolls cannot satisfy a guaranteed feature promise. A capped numerical effect can remain playable as an explicitly shown no-op if world policy permits it.

### Craft-only rare cards

Each accepted rare use consumes its authorized physical item and emits a configured instability adjustment in addition to the following effects, except that Volatile Stabilizer uses its own reduction/overload rule. Do not silently apply a common default amount to all cards. World policy sets footprint and progression requirements.

| Card | Ordered effect / host calls | Constraints |
|---|---|---|
| Volcano Mark | `feature_add(volcano, 1)`, instability adjustment | Regeneration-only; minimum planned 9×9 chunk footprint and late progression/boss gates; confirm against roadmap before shipping |
| Ultimate Greed | `request_draw(3)`, instability adjustment | Overflow burns and empty-deck events add their own costs; no compulsory discard unless configured separately |
| Biome Rupture | Two distinct biome choices, `biome_remove` twice, instability adjustment | Cannot remove last viable biome; absent targets are clearly identified as no-effect, not silently substituted |
| Ore Overgrowth | `ore_scale(selected, 2, 1)`, instability adjustment | Doubles current rate, checked arithmetic and safety cap; composes in play order with Deep Seam |
| Shipwreck Chart | `feature_add(shipwreck, 1)`, underwater-hostile plan, instability adjustment | Ocean only, navigable underwater structure; loot/danger use final instability tier |
| Ruined Village Relic | `feature_add(ruined_village, 1)`, instability adjustment | Hostile spawners and rare loot; biome-adapted variant only where explicitly supported |
| Unstable Catalyst | Persist hidden deterministic `permanent_modifier_add(selected)`, instability adjustment | Backend knows result; client does not. Never removable by ordinary cards, reload, cancellation or reselecting a seed |
| Desert Pyramid Seal | `feature_add(desert_pyramid, 1)`, instability adjustment | Desert, sandstone, reachable central loot and bounded spawners |
| Ancient Canopy Seal | `feature_add(giant_forest_tree, 1)`, instability adjustment | Forest, climbable ladder interior, trapdoors, deliberately dark hostile chambers and loot |
| Spider Nest Seal | `feature_add(cave_spider_nest, 1)`, instability adjustment | Must connect to an existing cave system; bounded spider spawners, loot-bearing corpses |
| Volatile Stabilizer | Increment use counter; roll configured overload chance; emit reduction or irreversible surge plus danger/reward opportunity | Example risk curve 5%, 15%, 35%, 65%, 100%; illustrative only. No fixed instability numbers in this design. Immediate resolution, persisted use count and RNG, no cancellation/reroll |

Unstable Catalyst's validated dangerous/potentially dangerous pool includes shipwreck, volcano and ruined village variants plus eligible generic hostile overlays. Cards do not unlock unsafe placement. Generic instability content includes hostile towers, camps, spawner complexes, cursed mines, fortifications, nests and vaults. Towers may use a taller ocean-supported variant above water. Docks are not a required gameplay feature. Biome-specific reward structures remain card content; generic danger remains instability content, with explicit shared feature planners permitted.

Hidden results must be redacted from client snapshots, previews, ordinary logs and accessible RNG state. Server replay retains them. Hashing a small secret option alone does not provide secrecy; use redacted views and authenticated server records rather than exposing enumerable hidden hashes.

### Later cards (not required starter content)

Biome Guarantee emits a feasible guaranteed biome plan and optionally `request_draw(1)` only if its definition says so. Fortune's Draw requests one draw. Register separate definition IDs and recipes when enabled. Do not turn these into undocumented effects on starter cards.

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
| `context_value(key_id)` | Whitelisted immutable values; no access to save files or live World |

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

## 9. Recipe-book API and unlocks

Extend `game_recipe_blueprint`/catalogue rather than inventing an unrelated crafting grid. Proposed fields: stable recipe/version ID, exact tagged/exact-ID ingredients, quantities, bound output definitions and quantities, returned containers/byproducts, station tags, unlock policy, processing ticks, fuel units, limits and compatible variants. Matching duplicate ingredient groups must allocate each input unit once. Alternatives are resolved deterministically and shown before confirmation.

Proposed APIs:

| API | Behavior |
|---|---|
| `game_recipe_book::initialize(catalogue, defaults)` | Validate and grant only configured bootstrap unlocks |
| `unlock_recipe(recipe_id, source_event_id, result)` | Idempotent grant with audited source, no client self-unlock |
| `query_recipes(filter, inventory_view, station_context, output)` | Known/unknown/available/missing ingredients; no mutation |
| `game_crafting::prepare_craft(request, inventory_view, book, station_context, output)` | Recipe-owned results, checked batch quantities, all capacity and access checks |
| `commit_craft(prepared, transaction_id, result)` | Atomic input consumption, outputs/byproducts and optional unlock grant |
| `start_processing(...)`, `advance_processing(server_ticks, budget, output)` | Reserved input/fuel, bounded timed furnace work, completion pending if output full |
| `export_book(output)` / `import_book(input, catalogue)` | Stable-ID unlock persistence, migration and world policy validation |

Minecraft supplies verified station context: actual station entity/block revision, usable range and permissions. The library must not accept an arbitrary client `has_workbench=true`. Unknown recipes remain locked even if the player guesses ingredients.

Default unlocks must include planks, sticks, workbench and all necessary basic gathering/processing recipes to avoid progression softlocks. Proposed discoverable unlocks: rare card recipes, advanced gear, special inks and stronger processing tiers. Discovery sources are loot blueprints, exploration achievements or research; **which source unlocks each stronger item is a balance decision still open**. The implementation must support all of them through a data-driven source grant, not hard-code a final progression choice.

Persist recipe book with character/world binding policy. Portable characters carry allowed unlocks, but world-disabled recipes remain unavailable. Migration maps stable IDs; unknown newer recipes are retained or quarantined, not silently converted into another item. Research synthesis is a distinct uncertain action with bounded deterministic pool and recorded RNG; exact known recipes never randomly yield another card.

## 10. Complete item and acquisition coverage

The roadmap's item catalogue is the required content scope. At implementation, enumerate the canonical Voxel registry: every built-in/runtime block must have exactly one explicit acquisition/drop policy. Do not freeze today's registry count into the validator.

- All non-air/non-fluid built-ins get placeable self-drop block items by default, including stone, ores, decorative plants, logs/leaves, snow/ice, sandstone and crystal/mineral blocks.
- Grass drops dirt; air drops nothing; water/lava use containers. Bedrock has an admin/creative item mapping but is not ordinarily breakable. New technical blocks may explicitly have no item.
- Add blocks/items for planks where applicable, workbench, furnace, Paper Mill, Card Press, World Loom, chest/storage, farmland, wheat and paper plants, including configured crop-state/drop mappings.
- Processed resources: sticks, coal, iron/gold/copper bars, diamond/emerald products if their extraction recipes are enabled, plant fiber/binder, paper reed fiber, paper sheets, card stock, pigments, dark/colored/advanced ink and magical dust.
- Containers/vehicle: empty/water/lava buckets and boat; filled buckets stack to one and have validated fluid state.
- Food/farm: wheat, seeds, bread, apple, pear, raw/cooked beef/pork/chicken/mutton; animal materials wool, leather, feathers and eggs.
- Gear: configured wood/stone/iron/gold/copper/diamond pickaxes, axes and swords; bow, arrows, shield, staff; cloth/leather/iron armor for head/chest/legs/feet. Material variants are explicit enabled definitions, not assumed combat balance.
- Cards: all seven starter definitions and eleven current craft-only definitions; later Biome Guarantee/Fortune's Draw only when enabled. Each output is a unique physical card.

Every item must declare exactly one primary acquisition policy: exact recipe, world/block acquisition, mob/harvest acquisition, loot/discovery, or restricted admin/technical. Not every obtainable block needs an artificial crafting recipe. No enabled craftable item may lack a recipe. This exhaustive policy is what makes “all items and recipes” testable without pretending bedrock or apples must be crafted.

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

Logs/leaves/terrain, fruits, seeds, raw meat, wool, leather, feathers and eggs use harvesting/drops rather than fictional manufacturing recipes. Crystal samples can initially be their registered block items; do not duplicate IDs merely to invent a catalyst. Farmland is a validated world/tool transformation, not an item duplication recipe. Future health/focus consumables, repair, glass, bricks and storage blocks require explicit definitions/recipes when enabled, and are not silently claimed complete here.

## 12. Placeholder card recipes and discovery

All exact card recipes use Card Press, output one unique card item, and use stable recipe IDs linked to a card definition. They never directly create a second deck instance. Basic recipes are initially known; rare recipes require an audited unlock. Costs are placeholders; replace through catalogue versions and balance tests.

Common substrate for starter recipes: card stock × 1 + basic ink × 1 + binder × 1, plus row catalyst. Rare substrate: card stock × 2 + advanced ink × 1 + quartz × 2, plus row catalyst. “Sample” references an existing registered item. Acquisition of recipe knowledge and acquisition of ingredients are distinct gates.

| Card | Additional catalyst | Knowledge |
|---|---|---|
| Biome Exclusion | Sand/dirt/snow sample × 1, chosen declared biome tag | Basic default |
| Deep Seam | Selected ore sample × 1 | Basic default; typed ore target remains selected at play unless explicitly bound by definition |
| Village Charter | Planks × 2 + wheat × 1 | Basic default |
| Surface Magma | Volcanic rock × 1 | Basic default; alternate common stone catalyst may be configured to avoid biome lock |
| Double Draft | Paper sheets × 2 | Basic default |
| Biome Compass | Flower sample × 1 | Basic default |
| Surveyor's Reach | Stone × 2 | Basic default |
| Volcano Mark | Volcanic rock × 4 + obsidian × 2 | Rare blueprint/progression unlock |
| Ultimate Greed | Gold bars × 2 + amber × 1 | Rare unlock |
| Biome Rupture | Two distinct biome samples × 1 each | Rare unlock |
| Ore Overgrowth | Selected ore × 2 + emerald material × 1 | Rare unlock |
| Shipwreck Chart | Planks × 2 + copper bar × 1 + seagrass × 1 | Ocean exploration/rare unlock |
| Ruined Village Relic | Cracked stone × 2 + iron bar × 1 | Rare unlock |
| Unstable Catalyst | Shimmer stone × 2 + amethyst × 1 | Rare unlock; effect selection is only at accepted play |
| Desert Pyramid Seal | Sandstone × 4 + gold bar × 1 | Desert discovery/rare unlock |
| Ancient Canopy Seal | Forest log × 4 + ladder × 1 | Forest discovery/rare unlock |
| Spider Nest Seal | Plant fiber × 4 + brown mushroom × 1 | Cave discovery/rare unlock |
| Volatile Stabilizer | Quartz × 2 + amber × 1 | Rare unlock; no previous-session overload counter carried accidentally |
| Biome Guarantee (later) | Selected eligible biome sample × 2 | Later unlock, not enabled until effect/recipe exists |
| Fortune's Draw (later) | Paper sheet × 1 | Later unlock, not enabled until effect/recipe exists |

Crafted cards cannot be consumed as their own unlock prerequisite. Optional research has a separate API/action, exact ingredient reservation, server RNG, finite candidate pool and guaranteed persisted result. Retrying or saving/reloading does not reroll. No magical dust in card recipes by default; it remains a regeneration resource unless reviewed balance explicitly changes that.

## 13. Required tests: Libft

Add focused files under `Test/Test`, following `AGENTS.md` naming; extend existing CardGame engine/simulation/snapshot tests, Game inventory/crafting/catalogue tests and Scripting tests. All Minecraft names/content in library tests are replaced with synthetic domains.

| Suite | Required assertions |
|---|---|
| Action sequencing | Invalid actor/phase, stale revision, duplicate request, different payload same ID, overflow IDs, unavailable action; replay yields identical state/output/RNG |
| Card identity/zones | Instance exists in exactly one zone; transactional transfer; failed move retains source; four-copy policy across all deck zones; boundary sizes and invalid policies |
| Draw policy | Full-hand burn, partial multi-draw, empty pile, legal reshuffle, no eligible cards, empty+overflow costs exactly once, no infinite refill loop |
| Obligations/choices | Draw-two-discard-one, multi-option validation, duplicate discard, foreign instance, zero hand, timeout default, save/load pending resolution, action blocked until complete |
| Effect transactions | Callback/script fail after emitted output, allocation failure at every preparation point, host rejection, rollback including RNG/outputs; durable retry protocol fake-host recovery |
| Generic schemas | Unknown intent/version, bad quantities, arithmetic overflow, output/node/byte caps, callback reentrancy, no raw-pointer wire payload |
| Script bridge | Native and scripted equivalent effects; stable bindings, wrong manifest, invalid bytecode, instruction/native/output caps, diagnostics, no world/I/O access, concurrent isolated owners |
| Persistence | Snapshot/delta round trips, unknown versions/IDs, corrupted lengths/checksum, rules mismatch, pending choices, burns, use counters and idempotency restored |
| Inventory | Stack compatibility, split/merge, weight/capacity, durable/unique items, partial pickup plan, concurrent revision conflicts, multi-container rollback, forged instance rejects |
| Crafting | Missing/unlocked recipes, duplicate ingredient overlap, tagged alternatives, output full, returned bucket capacity, invalid output catalogue, batch overflow, ingredient/result allocation failures |
| Recipe book | Default grants, idempotent discovery, sparse stable-ID persistence/migration, unknown IDs, query does not mutate, no recipe knowledge spoof |
| Timed processing | Fuel/input once, paused/full output, save/load/unload, capped tick catch-up, completion retry no duplicate result |

Use the existing failure-injection conventions: allocation limit zero disables limits, not forces failure. Test all lifecycle initialization/destroy/move failures and Linux/Windows/macOS serialization compatibility. Run production dependency/build checks so a test-only binding does not leak into archives.

## 14. Required tests: Minecraft

Add deterministic card, inventory, recipe and regeneration validators alongside existing validators; wire accepted validators into `make validate-all` and the platform-equivalent runner.

- Catalogue exhaustiveness: every registered block has one drop policy; every enabled craftable item has a valid recipe; every enabled card has an effect, item, recipe, knowledge policy and preview description. Starter counts total 20 and no definition exceeds four.
- One table-driven test per card verifies legal targets, ordered effect, caps, no-op display, instance consumption, rare sourcing, instability output and clean base rules next session.
- Sequential composition: remove biome then compass/guarantee; Deep Seam then doubling; multiple villages; final instability applied consistently to feature loot/spawners. Test incompatible ocean/desert/forest/cave footprints and unsupported required feature failure before spending.
- Unstable Catalyst secrecy/permanence, pool coverage, reload/retry consistency; Volatile Stabilizer escalating curve/session counter, deterministic overload and irreversible outcome; kiss/curse reward opportunities and caps.
- Multiplayer occupied chunk plus eight-neighbor exclusion, simultaneous reservations, glowing-wall movement/teleport/boat paths, disconnect/restart, five-minute timeout, no voluntary cancellation, resource journal recovery.
- World Loom start APIs: select/unselect exact coordinates with revision/idempotency checks; cost query is non-mutating; opening counts 4–7 cost `B4, 2B4, 4B4, 8B4`; overflow/insufficient dust/stale selection/occupied chunk leaves dust, reservations, barriers and CardGame hand unchanged; retry never charges or deals twice; committed start freezes selection and cannot be cancelled.
- Per-chunk instability save/reload and challenge defaults; untouched chunks unchanged; multi-chunk feature atomicity and stale generation rejection.
- Each ordinary/card recipe expands into a content-driven craft test with expected costs/yields/byproducts. Test inventory capacity failures preserve ingredients and unlock state. Book default workbench path, rare discovery and portable/world-bound character rules.
- World integration: station reach/claims/destruction race, grass-to-dirt drops, ore-block extraction, fluid-container transfers, crop/fruit/animal acquisition, boat item/entity transfer and save/reload exactly once.
- Performance: matched idle/play/draw/discard/research/crafting/timeout workloads, maximum legal deck and output limits, continuous block breaking during regeneration, inventory saves and chunk publication. Report frame mean/p95/p99, action latency, script operations, bytes/snapshot, generation/commit time, queue depth and allocations. No render-thread unbounded loop.

Libft tests prove mechanics; Minecraft tests prove named content and world rules. Do not duplicate a hard-coded Minecraft recipe/biome catalogue in Libft just to run library tests. Golden replay fixtures use versioned definitions and seed, not unspecified RNG outputs.

## 15. Implementation order and roadmap tracking

| Order | Deliverable | Depends on | Acceptance / roadmap update |
|---|---|---|---|
| 0 | Baseline API/limit audit, build/tests, dependency and save-version plan | Nothing | Record actual existing passing tests; no new feature marked done |
| 1 | Inventory prepare/commit and recipe-owned transactional outputs | Baseline | Failure/duplicate/concurrency tests pass; inventory/crafting foundations integrated status only |
| 2 | Generic action envelopes, non-combat resolution, outputs and durable protocol | Baseline | Replay/rollback/snapshot tests pass; update Libft CardGame API status |
| 3 | Draw/burn/mandatory discard/choice policies and copy limits | 2 | Full boundary matrix; update deck/hand decision mechanics status |
| 4 | Script bridge and stable manifest | 2–3 | Native/script equivalence, bounded failures, platform replay tests |
| 5 | Recipe book/catalogue extensions and timed processing | 1 | Unlock, output/byproduct, save/load/fuel tests |
| 6 | Minecraft item/drop/recipe catalogue and starter card host | 1–5 | Full registry coverage, all starter effects, bootstrap economy and content validators |
| 7 | Regeneration reservation/lifecycle and per-chunk persistence | 6 + existing generation | Multiplayer barriers/timeouts, base snapshot isolation and atomic publication |
| 8 | Rare cards, instability overlays, discoveries/research | 7 | Every current rare card tested, hidden/permanent outcomes and kiss/curse balance checks |
| 9 | UI/network/save integration, security and matched performance | All | Normal/analytics builds and full validators pass on target platforms |

Independent work can run in parallel after contracts are decided: generic inventory transactions, command envelope tests and content-table documentation. Avoid concurrent workers changing the same snapshot/wire schema. Narrow worker tasks specify exact files, behavior, constraints and tests; lead reviews integrations before marking complete.

For each delivered task record: affected APIs/files, schema versions, Libft tests, Minecraft validators, platform results, performance observations, reviewed commit IDs and remaining limitations. Update both this ledger and roadmap headings in the same implementation change. A generic API is not a completed Minecraft feature; a passing unit test is not a verified multiplayer world-save pipeline.

### Open decisions before production content freeze

Instability amounts/caps and overload curve; starting draw count; recipe unlock sources and portability restrictions; fuel/duration balance; material gear tiers; guaranteed-feature infeasibility UX; catalogue migration policy; research costs/pool; exact irreversible-session boundary. Defaults above provide implementable test fixtures, not hidden permanent balance commitments. Resolve these explicitly and update both documents instead of silently changing gameplay in implementation.
