# Configurable card-game simulation engine design

## Document status

This is an implementation and testing handoff for evolving `Modules/CardGame`
from its current foundation into a deterministic engine capable of simulating
complete card games. The compatibility targets used to expose missing concepts
are:

- Magic: The Gathering, using Magic Origins as the card corpus;
- Yu-Gi-Oh! using Legend of Blue Eyes White Dragon as the initial card corpus,
  but applying modern rules and also testing Fusion, Ritual, Synchro, Xyz,
  Pendulum, and Link mechanics;
- Hearthstone using the original 2014 Basic and Classic card pool, pinned to
  patch `1.0.0.5832` rather than current rebalanced cards.

These games are conformance profiles, not built-in Libft rules. The engine must
remain capable of representing substantially different games through data,
rule policies, effect programs, and registered native callbacks.

This document does not authorize copying proprietary card text or artwork into
Libft. Tests should use stable official identifiers, mechanics tags, minimal
paraphrased fixtures, or externally supplied data whose use is permitted.

All implementation must follow the repository-root `AGENTS.md`. In particular,
new APIs use Libft types and containers, explicit lifecycle methods and error
codes, no exception-based control flow, transactional failure behavior, and
test-only hooks that are absent from release archives.

## Goals

- Simulate a full match from deck validation and setup through a deterministic
  winner, draw, concession, or rules-defined termination.
- Separate a small rules-neutral kernel from game-specific policy modules.
- Express cards and rules through terrain-style configuration plus verified
  scripting/effect programs, with native function pointers as controlled
  extension points.
- Make every legal player choice representable, serializable, replayable, and
  suitable for an authoritative server or simulation agent.
- Preserve hidden information while retaining a complete authoritative state.
- Support timing, response, trigger, replacement, continuous-effect, combat,
  randomization, and identity semantics broad enough for all three profiles.
- Provide a testing framework that can prove individual cards, interactions,
  full games, replay determinism, and information isolation.

## Non-goals

- Baking Magic layers, Yu-Gi-Oh! chains, or Hearthstone's event ordering into
  the core engine.
- Parsing arbitrary natural-language card text.
- Letting a callback directly mutate match internals.
- Using raw pointers, board indexes, or process addresses as persistent object
  identity.
- Treating one large scripted integration test as proof of rules correctness.
- Reproducing online matchmaking, rendering, collection ownership, or economy.

# 1. Current CardGame audit

The present module is a useful foundation. It already provides configurable
players, board and hand limits, card/type/zone definitions, unique deck-instance
IDs, ordered decks, LIFO/FIFO resolution stacks, configurable phases, registered
effects, modifiers, combat modes, commands, snapshots, deltas, and replay
records.

It cannot yet simulate complete games from any of the three target profiles.
The main gaps are architectural rather than missing individual keywords.

| Current element | Limitation | Required evolution |
| --- | --- | --- |
| Four fixed card types | Cannot describe multi-types, subtypes, supertypes, faces, or user-defined types | Interned/tagged type and characteristic sets configured by a ruleset |
| Board-array indexes identify targets | Indexes change when objects move or compact | Stable physical instance ID plus zone-incarnation ID |
| Effects are function pointers | Pointers are not serializable; unrestricted callbacks can bypass validation | Stable effect IDs resolving to verified programs or operation-emitting callbacks |
| Three command kinds | Cannot represent attacks, activations, responses, payments, choices, mulligans, or concessions | Generic intent plus typed choice/response protocol |
| Health/mana/event operations | Cannot move cards, damage objects, attach, counter, transform, copy, reveal, or replace | Extensible tagged operation vocabulary |
| Linear phase `next_phase_id` | Cannot branch, repeat, skip, insert turns, or expose priority windows | Configured turn graph and scheduler policy |
| Event batch | No trigger discovery, ordering, optionality, intervening conditions, or response windows | Event pipeline and pending-trigger collector |
| Permanent/end-turn modifiers | No layers, dependency ordering, source lifetime, conditional effects, or other durations | Continuous-effect evaluator with policy-defined layers and expiry predicates |
| Fixed combat helper | Too narrow for blockers, positions, direct attacks, damage steps, or simultaneous groups | Pluggable combat state machine |
| Ordered deck | Good primitive, but no generalized hidden zones, visibility, search/reveal choices, or replacement draws | First-class zone model and viewer-specific snapshots |
| Resolution stack admission | Useful policy, but insufficient for priority, chains, speed restrictions, unanswerable actions, or immediate resolution | Ruleset-owned timing and resolution policy |
| Bounded arrays/logs | Overflow may prevent full games or silently weaken authoritative history | Explicit capacities, growth/flush policy, and exact capacity errors |
| Mutable callbacks | A failure can leave partial state if mutation has started | Prepare, validate, and atomically commit operation transactions |

The existing API should remain usable while the new kernel is introduced, but
new full-simulation code must not build more semantics around unstable board
indexes or direct state mutation.

# 2. Architecture: kernel plus ruleset profiles

## 2.1 Rules-neutral deterministic kernel

`card_game_engine` should become the coordinator for these rules-neutral tasks:

- object, player, zone, and ruleset identity;
- immutable definitions and mutable match state;
- validated intent submission and pending choices;
- atomic operation transactions;
- event recording and deterministic sequencing;
- snapshot, redaction, delta, replay, and state hashing;
- deterministic random-oracle access;
- resource and execution budgets.

It must ask the active ruleset profile how legality, timing, trigger ordering,
continuous effects, combat, and win conditions work. It must not contain a
branch such as `if game == MAGIC`.

## 2.2 Ruleset composition

A configured ruleset is an immutable composition of policy interfaces:

```text
card_game_ruleset
  zone policy
  deck-construction policy
  setup/mulligan policy
  turn-graph policy
  timing/priority policy
  cost/payment policy
  trigger-order policy
  resolution policy
  continuous-effect policy
  replacement/prevention policy
  combat policy
  state-check/win policy
  visibility policy
  randomness policy
```

Each policy may be implemented by configuration, a verified script program, or
a registered native callback. Native callbacks are identified by stable IDs in
configuration. Raw function pointers stay inside an immutable process-local
registry and are never serialized.

Callbacks receive a read-only state view and an operation builder. They return
validated proposed operations and meaningful error codes; they never receive a
mutable engine pointer. The engine validates the complete proposal before one
transactional commit.

## 2.3 Terrain-style configuration

Use the same overall model as terrain configuration:

- schema-versioned files define zones, resources, types, phases, windows,
  limits, cards, effects, and policy selections;
- defaults are resolved explicitly and the resolved configuration can be
  serialized canonically;
- unknown fields, duplicate stable IDs, invalid references, cycles, impossible
  capacities, and unsupported versions fail transactionally;
- a canonical ruleset hash covers resolved configuration, effect bytecode,
  native-registry ABI/version, and the imported card corpus;
- saved games and replays record that hash and reject incompatible rulesets.

Configuration should describe data and compose reusable effect primitives. It
must not grow into an untyped collection of game-specific boolean flags.

## 2.4 Sets, printings, rarities, and sealed products

Collection and booster simulation must be a separate layer from match rules.
The CardGame engine may consume a generated card pool, but the rules kernel
must not assume that every card is acquired from a booster or that rarity has a
fixed meaning across games.

### Set and printing identity

Use separate stable identifiers for:

- a logical card definition and its rules/effect program;
- a set, expansion, core set, starter product, promotional pool, or reprint
  product;
- a printing/card-set entry;
- a language, region, release, legality, and rules-text revision;
- a rarity slot assignment;
- a cosmetic or physical treatment.

Two printings may reference the same logical card definition while having
different set IDs, collector numbers, rarities, frames, finishes, art, borders,
languages, or availability. A printing may also have more than one rarity
classification when a product or database needs both a base rarity and a
special-treatment rarity. The configuration must state whether the rarity is:

- a single forced value;
- one of several allowed values depending on product/slot;
- a multi-label classification such as `rare` plus `foil`;
- a treatment overlay that does not change the card's gameplay definition.

The engine must never infer rarity from card ID, collector-number formatting,
alphabetical order, or an enum whose values are shared by all games.

Recommended records are:

```text
card_definition
  definition_id, rules schema/version, effect program, gameplay characteristics

set_definition
  set_id, name, release/version, total numbered pool, supplemental pool,
  legality/profile tags, source and content hash

set_printing
  printing_id, definition_id, set_id, collector number, rarity labels,
  treatment IDs, language/region, replacement/eligibility flags

treatment_definition
  treatment_id, finish/frame/art/border metadata, rarity overlay, availability

product_definition
  product_id, set/pool references, pack recipe, collation rules, odds version
```

Each set manifest must identify every card in its pool, including basic cards,
tokens, checklist/reference cards, supplementary cards, promos, alternate-art
entries, and cards that are not available in ordinary boosters. The manifest
must state whether an entry contributes to deck legality, collection ownership,
sealed construction, or only a cosmetic/product pool.

### Rarity and slot model

A booster is an ordered or unordered sequence of configured slots. A slot may
be defined by:

- eligible pool or sheet;
- rarity or treatment distribution;
- number of cards selected;
- replacement and exclusion rules;
- whether duplicates are allowed within the pack;
- whether the slot is guaranteed, weighted, or conditional;
- whether it replaces another slot when a special result occurs;
- collation constraints such as color, class, type, or sheet position;
- whether a card may appear in multiple slots or only one slot.

Do not represent a booster as simply “one rare and ten commons.” The recipe
must support, for example:

- a fixed common/uncommon/rare distribution;
- a weighted rare slot where mythic/legendary results replace ordinary rare
  results at a configured probability;
- a foil or special-treatment slot that may draw from a different pool;
- basic land, token, ad, rules insert, or wildcard slots;
- a wildcard slot that can replace a normal slot;
- guaranteed cards or guaranteed rarity bands in starter products;
- separate sheets with different card frequencies inside the same rarity;
- collation rules preventing or permitting duplicate definitions;
- regional/language/product-specific pools;
- bonus, serialized, signed, promo, or box-topper cards outside normal odds;
- products whose slot count or probabilities differ from ordinary boosters.

The recipe must distinguish:

- **slot count**: how many results are generated;
- **rarity odds**: probability of selecting a rarity/treatment class;
- **card frequency**: probability of a particular eligible card within that
  class;
- **replacement probability**: whether a special result consumes or replaces a
  normal slot;
- **collation constraint**: correlation between slots or packs;
- **product guarantee**: a guarantee over one pack, a box, a case, or an entire
  configured opening sequence.

The probability model must support exact rational weights, not only floating
point percentages. Store numerator/denominator or normalized integer weights,
then use the deterministic match/collection random oracle. A pack-opening
simulation records the product ID, recipe version, seed/oracle draws, selected
slots, replacement decisions, printing IDs, and resulting collection changes.

### Collection and acquisition rules

The collection layer must support:

- owned physical copies versus an unlimited card-definition catalog;
- printing-specific ownership and gameplay substitution policy;
- maximum copies in a deck independent of collection quantity;
- duplicate protection, pity/guarantee counters, and bad-luck protection when a
  product explicitly provides them;
- foil/golden/alternate-art treatment as separate owned items or overlays;
- crafting, dusting, trading, rewards, promos, and generated cards as distinct
  acquisition sources;
- bound/untradeable, cosmetic-only, region-limited, and event-limited entries;
- opening one pack, a box/case sequence, or a large statistical simulation;
- collection snapshots and deterministic replay of openings;
- configurable duplicate behavior: allow, reroll within slot, convert to
  currency, or replace with a defined fallback;
- cards with multiple valid rarities and cards forced to one specific rarity in
  a particular product.

Collection legality and gameplay definition must be resolved separately. A
special printing normally points to the same definition, while a rules revision
or functional rebalanced version may point to a different definition ID or
ruleset generation. A cosmetic treatment must never accidentally alter card
behavior.

### Set manifest requirements for the target profiles

The initial manifests should include:

- all 272 main Magic Origins cards plus the separately identified 16
  supplementary cards, with numbered-pool membership, rarity, and treatment
  metadata;
- all 126 official Legend of Blue Eyes White Dragon entries, including their
  rarity and product/list membership, while keeping modern rules in the game
  profile;
- the pinned 2014 Hearthstone Basic/Classic card catalog, where acquisition is
  not a physical booster simulation by default but can use a configurable
  collection/reward product model if needed.

If a game does not expose physical booster collation as part of its normal
rules, leave the product layer disabled for that profile instead of inventing
odds. The manifest still records card-set membership, rarity-like labels, and
availability where those are meaningful.

### Pack and set test requirements

For every set manifest, test:

- every card is present exactly as intended in the correct pool or explicitly
  marked supplemental/unavailable;
- duplicate collector numbers, missing definitions, invalid rarity labels,
  treatment references, and contradictory forced-rarity rules are rejected;
- a card with several rarity labels resolves correctly in each product;
- a forced-rarity card cannot leak into an ineligible rarity slot;
- every slot produces the configured count and only eligible printings;
- replacement slots preserve total pack-card count when required;
- special slots, foil/treatment slots, tokens, inserts, wildcards, and bonus
  slots follow their individual policies;
- duplicate, reroll, pity, guarantee, and currency-conversion policies are
  deterministic and transactional;
- exact pack odds match rational-weight expectations for small exhaustive
  recipes and statistical confidence bounds for large simulations;
- seeded openings reproduce identical printing IDs, collection state, and
  oracle trace across platforms;
- box/case guarantees are enforced across the complete sequence rather than
  incorrectly applied independently to each pack;
- failed opening, allocation, or collection-update transactions leave both the
  oracle position and collection unchanged;
- collection redaction prevents one player from learning another player's
  unopened pool or future seeded results.

The test harness should provide both a scripted oracle for exact edge cases and
a statistical runner that opens enough packs to detect materially incorrect
weights without treating random variance as a functional failure. Every
statistical test records its seed, sample count, expected interval, and recipe
hash.

## 2.5 Optional card and class-object layer

The card-definition and class-based object framework is optional. It must be
implemented as a separately buildable CardGame extension rather than becoming
a required dependency of the rules-neutral simulation kernel.

The core engine depends only on stable IDs, fixed-width schemas, configured
zones/resources, validated operations, effect-program IDs, and profile
callbacks. A user may therefore run a lightweight game using configuration
records alone, without constructing card classes or linking the optional
object layer. This supports tests, servers, tools, and games whose cards are
entirely data-driven.

The optional layer may provide card-definition and printing classes,
class-specific faces and characteristics, typed card/effect factories,
configuration loaders, and adapters that translate class methods into the
kernel's validated operations and choices. It must never mutate kernel state
directly: it submits intents, operations, or registered effect programs while
the kernel retains authority over validation, permissions, transactionality,
event ordering, hidden information, and state hashes.

The extension may be omitted at compile time or disabled at runtime without
changing the core engine ABI or deterministic behavior. Its tests must run
both with and without the extension: the linked mode verifies class-to-kernel
translation, while the configuration-only mode verifies generic operations,
replays, snapshots, and deltas. Serialized data must contain stable IDs and
plain state only, never C++ type information, raw function pointers, or class
object addresses.

# 3. Universal runtime model

## 3.1 Identity and object incarnation

At minimum, keep these distinct:

- `definition_id`: which card definition this is;
- `physical_instance_id`: which copy in the match this is;
- `object_id`: the current rules object represented by that instance;
- `incarnation`: incremented when policy says a zone transition creates a new
  object;
- `owner_id` and `controller_id`;
- `source_object_id` and `effect_instance_id` for abilities/effects.

This supports duplicate cards, copies, control changes, transformation, hidden
objects, attachments, and the Magic/Yu-Gi-Oh! rule that returning from another
zone generally does not mean an effect can keep tracking the old object. A
physical card may retain its physical ID while obtaining a new object ID or
incarnation.

Never target a board-array index. Handles must detect stale incarnations rather
than accidentally selecting whatever later occupies that index.

The card-definition corpus is separate from per-match zone capacity. The
implementation keeps definitions in a CMA-backed registry that starts at 256
entries and grows geometrically up to 65,536 definitions. This permits a full
set corpus such as Magic Origins without expanding every match object's fixed
board, deck, and snapshot storage.

## 3.2 Characteristics and faces

An object needs configurable characteristics rather than a fixed creature
struct:

- name/definition and active face;
- types, subtypes, supertypes, classes, tribes, attributes, and tags;
- numeric and symbolic properties;
- printed/base, copied, derived, and current values;
- visibility/orientation/position;
- controller, owner, timestamps, counters, damage, attachments, and materials;
- effect programs exposed by the active face and zone.

Multiple faces and context-dependent interpretation are required for Magic
Origins double-faced cards and Yu-Gi-Oh! Pendulum monsters. Transforming,
flipping, copying, silencing, and changing types must be operations interpreted
by the profile's characteristic evaluator.

## 3.3 First-class zones

The rules-neutral zone storage must be implemented independently from profile
semantics. A configured zone has a stable ID, capacity, owner scope, and
allowed-card-type mask. The zone store owns ordered physical entries
(`instance_id` plus `card_id`) and exposes insertion, inspection, removal,
peeking, and pop operations with transactional validation. Profiles assign
meaning to IDs such as library, hand, battlefield, graveyard, exile, stack,
reserve, command zone, or a custom pile; the kernel must not hardcode those
meanings.

Owner-scoped zones resolve to one ordered pile per player. Shared zones resolve
to one common pile while retaining caller access policy at the profile layer.
Every future engine move must validate source membership, destination capacity,
duplicate instance ownership, and destination type masks before committing.
Failed moves must leave both zones unchanged. Zone order and physical IDs are
included in snapshots, deltas, state hashes, and replay records once connected
to match state. The standalone store is the reusable foundation for that
integration.

A zone definition needs configurable:

- shared, per-player, per-team, or object-attached ownership;
- ordered, unordered, slotted, graph-shaped, or capacity-limited topology;
- public, owner-visible, controller-visible, count-only, or fully hidden data;
- face-up/down and orientation rules;
- legal entry/exit paths and replacement destinations;
- whether movement creates a new object incarnation;
- insertion, top, bottom, indexed, random, search, reveal, inspect, draw, and
  shuffle permissions;
- slot adjacency, link, and directional metadata.

This extends the existing ordered deck operations into a common abstraction for
libraries/decks, hands, graveyards, discard piles, exile/banished zones, fields,
monster/spell slots, secrets, Extra Decks, sideboards, command zones, and custom
game areas. Read-only inspection must honor the requesting viewer's visibility.

### 3.3.1 Shareable deck codes and deck identity

Deck import/export is a first-class corpus/deck service. It must provide both
directions without losing exact counts or configured zone membership:

```text
card_game_deck_encode(deck, output_code)
card_game_deck_decode(input_code, deck)
card_game_deck_hash(deck, sha256_hash)
```

The code format is versioned binary data encoded as Base64URL. Its header
contains the format version, profile/game ID, format ID, card-corpus version,
and flags for main, extra, side, commander, companion, hero, quest, reserve,
or future custom zones. Integers use bounded varints and decoding rejects
truncation, overlong values, unsupported versions, invalid zone counts, unknown
cards, illegal copy counts, and trailing garbage.

Each zone is canonicalized before encoding. Entries are sorted by stable
`definition_id`, then grouped by copy count (one-copy, two-copy, three-copy,
and arbitrary-count entries). Equivalent decks therefore produce exactly the
same code regardless of insertion order. The default representation uses
gameplay `definition_id`; an optional printing-aware flag stores `printing_id`
when exact set treatment matters. Canonicalization must not change the
ordered runtime deck, so imported decks retain a deterministic shuffle seed or
explicit order when the profile requires it.

The payload ends with a CRC32C checksum for typo/corruption detection. A
checksum failure returns a dedicated deck-code error and never partially
mutates the destination. Separately, SHA-256 of the canonical binary form is
the fixed 32-byte deck identity used by match setup, caching, replay headers,
and duplicate comparison. The hash is an identity tool, not validation or
authentication.

Deck identity must support decks from 25 through 500 cards, counting every
configured zone included by the profile. A compact optional digest may use
size tiers such as 128 bits for 25--64 cards, 192 bits for 65--128 cards, and
256 bits for 129--300 cards, with a 320- or 384-bit tier for 301--500 cards
when the selected hash family supports it. The tier, total card count,
included-zone counts, profile, corpus version, and hash algorithm are part of
the identity metadata and must be encoded with the digest. Thresholds are
versioned configuration, not implicit implementation details.

No finite hash can mathematically guarantee uniqueness for every possible
deck. The reversible canonical deck code is therefore the exact identity; a
digest is only a fast probabilistic index. Whenever equality affects game
correctness, the engine must compare the canonical bytes after matching the
digest. Callers that need one stable opaque identifier should use the complete
256-bit SHA-256 digest for every deck size. Tests must cover 25, 64, 65, 128,
129, 300, 301, and 500 cards, boundary tier selection, one-card/count/zone
changes, cross-profile separation, deterministic output, and forced digest
collisions using a test hash provider to prove canonical-byte comparison
rejects unequal decks.

Deck-code tests cover every zone flag, one/two/three/arbitrary copy groups,
maximum legal counts, definition-only and printing-specific codes, Base64URL
edge bytes, checksum failures, truncation at every byte, unknown corpus
versions, illegal deck lists, canonical equality under input permutation,
deterministic round trips, and hash changes for every semantic deck change.
Fuzz decoding uses strict byte and allocation budgets.

## 3.4 Intents, actions, choices, and costs

A complete simulation cannot expose only `play_card()`. Use this pipeline:

```text
player/agent intent
  -> enumerate and validate legal action
  -> request modes, targets, ordering, values, and payment choices
  -> lock declared choices and pay costs
  -> create action/effect object
  -> open policy-defined response window
  -> resolve transactionally
  -> perform state checks and collect triggers
  -> request trigger ordering/optional choices
  -> continue until a stable decision point
```

Choices are serializable state objects with a stable choice ID, acting player,
option schema, legal option set or validator, visibility, deadline/default
policy, and continuation. Required choice kinds include targets, modes, one or
more objects, ordered lists, numbers, yes/no, payment decomposition, replacement
selection, trigger ordering, mulligan, and random selection by the oracle.

Costs and effects must be separate. Paying life, mana, discarding, sacrificing,
tapping, tributing, detaching materials, revealing, and exhausting a use limit
are committed as costs before resolution according to the profile. Negating or
countering the effect does not retroactively refund a committed cost unless a
configured rule explicitly says so.

## 3.5 Configurable resources and action allowances

### Current implementation hardening requirements

Every rejected operation must preserve authoritative state. Capacity checks for
`add_units` happen before publishing the unit or advancing counters. Any later
fallible preparation uses a shadow copy and commits only after all checks pass.

Resource units expose a locked quantity, not just a boolean. Locking three from
a ten-unit entry leaves seven spendable. Independent timed locks require
separate `(unit_id, amount, unlock_epoch)` records; if that table is not yet
available, an incompatible second schedule must be rejected without changing
the first one. Expiry and lock duration are distinct state transitions.

Payment plans are hostile input at the commit boundary. Repeated unit IDs are
aggregated before mutation and rejected on overflow or overuse. Multi-component
costs reserve amounts rather than merely excluding unit IDs, so components may
consume different portions of one unit. Generic, colored, hybrid, alternative,
variable, additional, and non-resource costs are explicit solver inputs. A
zero-cost plan is a valid no-op.

Allowance records contain stable `predicate_id` and `predicate_context_id`
values. Raw callbacks and `void *` context belong only to a process-local
registry and are never authoritative state or serialized data. Missing IDs,
expired allowances, duplicate IDs, and use-count overflow reject atomically.

The mandatory regression matrix covers failed capacity additions, duplicate and
overlarge payment entries, shared-unit multi-component costs, partial and
conflicting timed locks, exact expiry boundaries, zero-cost plans, alternative
allocations, missing predicate IDs, and independent additional allowances.

Resources must not be represented by one `current_mana` integer. A ruleset may
define any number of resource pools and action allowances. Both use a common
ledger architecture but have different spending semantics:

- a **resource pool** contains units that may be produced, reserved, spent,
  refreshed, emptied, locked, or expired;
- an **action allowance** authorizes a classified action a limited number of
  times during a configured window, possibly with restrictions.

Each resource definition must configure:

- stable resource ID and display category;
- scalar, colored, typed, or tagged units;
- current, maximum, temporary, reserved, locked, and spent quantities;
- whether maximum capacity grows automatically and its cap;
- refresh, empty, unlock, decay, and expiry events;
- whether unused units carry between phases or turns;
- who owns the pool: player, team, object, zone, action, or match;
- production and spending restrictions;
- replacement effects that alter production, type, amount, or destination;
- visibility and whether opponents see exact, approximate, or no values;
- underflow, overflow, and maximum-reduction behavior;
- deterministic ordering when several refresh, lock, and expiry rules apply.

Resource units may carry predicates rather than being freely interchangeable.
Examples include mana usable only for creatures, mana usable only for activated
abilities, temporary mana that expires at the end of a phase, a discount that
applies only to the next spell of a type, and a crystal that is locked next turn.
Represent such units as entries with a stable source, amount, type/tags,
spending predicate, expiry event, and creation sequence. Do not encode every
condition as a new global resource type.

The cost solver receives a cost expression and the currently eligible resource
entries. It must enumerate legal payment plans when player choice matters. A
payment plan identifies exactly which units, discounts, alternative costs, and
non-resource costs will be consumed. The selected plan is revalidated and
committed atomically. Automatic payment may be a profile policy, but its
selection order must be deterministic and must not waste restricted resources
when an unrestricted equivalent can satisfy the cost unless the profile asks
the player to choose.

Required cost-expression nodes include:

- exact typed/colored amount;
- generic amount payable by allowed types;
- any-one-of and combinations of types;
- variable `X` or player-selected amounts;
- hybrid/either-or payment;
- additional and optional costs;
- alternative complete costs;
- increases, reductions, minimum costs, and cost-setting effects;
- life, discard, sacrifice/tribute, tap/exhaust, counter removal, material
  detachment, reveal, and other operation-backed costs;
- conditional cost nodes based on object, zone, action, turn, or match state.

Cost calculation must have a profile-defined ordered pipeline. It should derive
a cost from immutable printed/configured cost, apply alternatives and additions,
then increases, reductions, setters/minima, and finally payment selection. The
exact order remains configurable because different games do not share all of
Magic's cost rules.

### Conditional resource production and spending

Resource producers and modifiers use predicates over a read-only state view.
The predicate may inspect controller, active player, card/object tags, action
type, origin zone, destination zone, phase, turn count, damage state, prior
events, and configured counters. It may not mutate state or consume randomness.

Examples the model must support include:

- produce one of several colors chosen on activation;
- produce bonus mana only for a creature or a particular subtype;
- reduce the next matching card's cost this turn;
- provide temporary mana only during the current phase;
- gain or refresh a crystal only if a condition was met last turn;
- lock crystals on the following turn as part of a cost/effect;
- prevent a resource from paying a particular class of cost;
- convert one resource type into another through a validated action;
- make a card free without changing its printed/base cost;
- increase an opponent's costs while a continuous source remains active.

Conditions are checked at the ruleset-defined points. A production condition is
checked when producing; a spending predicate is checked while constructing and
again while committing the payment plan. A changing condition must not silently
turn previously spent units into invalid payment after the action has legally
been committed.

### Action allowances

An allowance record contains:

```text
allowance ID and action classification
owner/player/team
remaining and maximum uses
window/epoch in which it is valid
eligibility predicate for the action and object
source object/effect and source-lifetime policy
consumption point
priority and combination policy
```

Allowances handle one-Normal-Summon-per-turn, additional land plays, one hero
power per turn, one Pendulum Summon per turn, and configurable actions in other
games. An allowance may be unrestricted, restricted to a subtype/level/zone, or
valid only while its source remains active.

When an action could consume several allowances, the engine must identify the
eligible set and apply the profile's selection policy. Some games require the
player to choose which additional allowance is being used; other profiles may
choose deterministically. Consuming one allowance must not accidentally consume
all matching allowances.

Distinguish these cases explicitly:

- increase the base maximum for an action;
- grant one additional use from an independent source;
- replace the normal action with a restricted action;
- perform an action through an effect without consuming the normal allowance;
- permit an action only if the normal allowance has or has not been consumed;
- prohibit an action despite unused allowances;
- copy or transfer an allowance;
- expire an unused allowance when its source leaves or its window ends.

The consumption point is policy-defined: declaration, successful legality
check, committed cost, successful performance, or resolution. Failed and
negated attempts therefore have explicit behavior rather than implicitly
restoring or consuming an allowance.

### Resource and allowance operations

Extend the atomic operation vocabulary with typed operations for:

- create/destroy a pool or allowance;
- change current or maximum quantity;
- add/remove/transform a restricted resource entry;
- reserve, commit, release, refresh, empty, lock, unlock, or expire resources;
- grant, consume, restore, restrict, suspend, or expire an allowance;
- schedule a future refresh, unlock, expiry, or maximum-capacity change.

Every operation records its source, reason, before/after value, and expiry or
reset epoch. Replays and viewer deltas must be able to explain why a payment or
action was legal without serializing callback pointers.

### Required resource and allowance tests

- exact, generic, colored/typed, hybrid, variable, optional, additional, and
  alternative payments;
- multiple valid payment plans and deterministic/player-selected resolution;
- restricted units mixed with unrestricted units;
- temporary resources expiring at every configured boundary;
- maximum growth, reduction below current quantity, caps, overflow, and refresh;
- locked resources across one and several turns, including partial unlocking;
- simultaneous discounts, increases, setters, minima, and conditional effects;
- condition changes before declaration, payment commit, and effect resolution;
- rollback after reservation, partial cost preparation, or allowance selection;
- one base allowance, several independent additional allowances, and restricted
  allowances whose predicates overlap;
- attempted, illegal, negated, failed, and successful actions for every
  consumption-point policy;
- source leaving before allowance use and after an action has committed;
- replay, snapshot, delta, redaction, and cross-platform hash stability.

## 3.6 Atomic operation vocabulary

The operation representation should be a tagged union with stable schema IDs.
Initial operations should cover:

- move, create, copy, transform, destroy, and remove objects;
- draw, mill, discard, reveal, inspect, search, shuffle, and reorder;
- attach/detach, control change, orientation/position, and slot movement;
- damage, heal, life/resource change, resource payment and refresh;
- add/remove/set counters, tags, base values, and continuous modifiers;
- declare attacker/blocker/target and create combat groups;
- create/dismiss tokens and generated cards;
- register/remove/suspend triggered, activated, static, or replacement effects;
- enqueue action, trigger, delayed event, phase, turn, or choice;
- record random result, concession, loss, win, or draw.

Validation operates on a shadow transaction. If allocation, callback,
validation, choice, or commit preparation fails, authoritative state and RNG
position remain unchanged. Event emission occurs from the committed transaction,
not while it is being prepared.

The callback operation buffer follows the same policy: it starts at 256
operations, grows through CMA-backed geometric reallocation, and stops only at
the shared 16,777,216-record defensive ceiling. Reallocation occurs before
the new operation is written, so an allocation failure cannot partially append
an operation. `FT_ERR_FULL` means the defensive ceiling was reached; it never
means that an older operation was silently discarded.

The initial operation implementation includes player health and mana changes,
event emission, board-instance damage and healing, and permanent or
end-of-turn instance stat modifiers. Instance operations validate the target,
amount, and modifier duration before changing state. A callback's complete
operation batch runs inside the engine snapshot rollback path: if any
operation fails, the engine restores the state captured before the callback,
including the earlier operations from that batch.

## 3.7 Event, trigger, and timing model

Events need pre-event, replacement, committed-event, and post-event stages.
An event contains stable sequence IDs, cause/source, actor, affected objects,
old and new values, visibility, and links to its transaction.

The profile controls:

- which actions use a stack/chain/queue or resolve immediately;
- who receives priority or a response opportunity and in what order;
- which effect categories may respond in each window;
- how simultaneous triggers are collected and ordered;
- mandatory versus optional triggers;
- trigger conditions checked at trigger time and again at resolution;
- state checks that repeat until stable;
- whether new actions may enter while a stack is resolving.

This generalizes the existing LIFO/FIFO/admission configuration. A resolution
container remains a primitive; the profile determines when and how it is used.

Trigger callbacks must have an explicit configured priority. The kernel
dispatches matching callbacks in ascending priority order and uses their stable
effect ID as the tie-breaker. Registration timing, pointer addresses, and host
container iteration must not alter the result. The priority is part of the
ruleset hash and is preserved by replay compatibility checks. A profile may use
the priority to model mandatory trigger ordering, replacement ordering, or
simultaneous-trigger policies, while the callback can only submit validated
operations through the operation builder.

The resolution container itself must also be heap-backed. Its configured
capacity is allocated for active and deferred entries independently, allowing
large profiles to request substantially more than 256 entries without placing
two large arrays in every stack object. Allocation is transactional during
initialization and both buffers are released during destruction or move. A
shared 16,777,216-entry profile ceiling remains a defensive bound against malformed
configuration and unbounded trigger loops; reaching a configured capacity
returns `FT_ERR_FULL` without consuming the attempted entry.

Event storage must not impose a 256-event match limit. The live pending-event
queue, rollback snapshots, and deltas use CMA-backed buffers with an initial
capacity of 256 and geometric growth. Growth allocates and copies into a new
buffer before publishing the pointer; allocation failure therefore leaves the
old queue and event sequence unchanged. The implementation uses a shared
16,777,216-record ceiling as a corruption/runaway-resolution guard, not as
normal storage. Reaching that ceiling returns `FT_ERR_FULL` explicitly.
Snapshot and delta
objects own their event buffers and release them on destruction, so the larger
ceiling does not create multi-megabyte stack objects. Tests must emit 257,
1024, and a large stress batch, then verify event order, sequence numbers,
rollback, snapshot application, delta application, and failure-injected growth.

## 3.8 Usage-limit ledger

Yu-Gi-Oh! demonstrates why a generic `used_this_turn` flag is insufficient.
Represent a limit as:

```text
limit key      = stable effect/name/group ID plus optional parameters
subject scope  = object incarnation, physical copy, player, team, or match
window scope   = action, chain, phase, turn, round, match, or custom epoch
attempt policy = consume on attempt, activation, cost payment, or resolution
count          = configured maximum
reset policy   = event or epoch transition
```

This can express the commonly called soft once-per-turn behavior using an
object/incarnation subject and hard once-per-turn behavior using a card-name or
effect-group key plus player and turn. It also handles Hearthstone hero-power
uses and Magic loyalty activations. Leaving and returning creates a fresh soft
scope only when the active profile's identity policy says so.

The ledger is not a card-only subsystem. Its `limit key` may identify a card
activation, an effect definition, a named effect group, a triggered ability, a
hero power, a summon procedure, a player action, or any other ruleset-defined
operation. The subject scope separately identifies the card instance, effect
source, player, team, or match to which the limit applies. This supports one
activation per physical card, one hard once-per-turn limit shared by every copy
of an effect, one hero-power use per player turn, or one triggered effect per
source and chain. Effect programs use stable key and source IDs; raw function
pointers and object addresses are never used as limit identity.

An effect requests consumption at its declared attempt policy. The kernel
checks the limit before committing the effect, consumes it at the configured
attempt, activation, cost-payment, or resolution point, and rolls it back if
the surrounding transaction fails. A failed resolution therefore cannot
silently consume a limit unless the profile explicitly selects `on_attempt`
semantics.

### Effect usage-limit API contract

The public API must not require callers to pretend that every limited action is
a card. The effect system should be able to declare and consume a limit using
stable identifiers, for example:

```text
register_usage_limit(
    key_id          = EFFECT_KEY_BATTLECRY,
    subject_id      = source_card_instance,
    scope           = TURN,
    window_epoch    = active_turn,
    maximum_uses    = 1,
    attempt_policy  = ON_RESOLUTION,
    source_instance = source_card_instance)
```

The same call shape must support `EFFECT_KEY_HERO_POWER`,
`EFFECT_KEY_SYNCHRO_SUMMON`, `EFFECT_KEY_TRIGGERED_ABILITY`,
`EFFECT_KEY_PLAYER_ACTION`, or a configured effect-group key. `subject_id` is
not implicitly a card ID: it may identify an effect source, ability
incarnation, player, team, chain, or match according to the configured scope.
Copies of a card therefore receive independent subjects when the rules require
a soft limit, while a shared effect-group key and player subject can implement
a hard once-per-turn restriction across all copies.

Effect execution must follow this sequence:

```text
validate effect and locate its stable usage key/subject
        -> check can_consume without mutation
        -> create the surrounding transaction snapshot
        -> consume at the configured policy point
        -> execute the effect callback/program
        -> commit on success, restore usage snapshot on failure
```

`ON_ATTEMPT` is consumed before any effect work and intentionally remains used
when the attempt fails. `ON_ACTIVATION` is consumed only after activation and
cost validation succeed. `ON_RESOLUTION` is consumed only when the effect
actually resolves. These semantics belong to the rules profile, not to the
card class, so the same engine can model card effects, triggered effects,
summon procedures, hero powers, and non-card player actions.

Usage-limit records and their snapshots must contain no function pointers,
object addresses, or card-only assumptions. They must be included in game
snapshots, deltas, state hashes, and replay state so restoring or replaying an
effect cannot create a different remaining-use count. Tests must cover:

- two physical copies sharing a hard effect limit;
- two physical copies with independent soft limits;
- a triggered ability and a player action using the same configured group;
- hero-power, summon-procedure, chain, phase, turn, round, match, and custom
  reset windows;
- all three attempt policies, including failure after each policy point;
- failed effect resolution restoring the ledger for activation/resolution
  policies but not for attempt policy;
- snapshot, delta, replay, and deterministic-hash equality for effect limits;
- duplicate keys/subjects, expired windows, exhausted limits, invalid IDs, and
  allocation failure without partial ledger mutation.

## 3.9 Continuous, replacement, and prevention effects

Do not implement continuous rules by repeatedly editing stored base values.
Keep base state and evaluate a derived state through a configured pipeline:

- applicability and affected-object selector;
- value/type/ability/control transformation;
- layer/category;
- dependency and timestamp ordering;
- source lifetime;
- duration such as while-source-present, until event, end of turn, N turns,
  permanent base modification, or custom predicate.

Replacement/prevention effects inspect a proposed event before commit and may
decline, replace it, modify it, redirect it, or ask the affected player to choose
among multiple applicable replacements. Loop detection and a per-event rule
that prevents one replacement instance applying repeatedly are mandatory.

This is needed for Magic type-changing and rules layers, Yu-Gi-Oh! continuous
effects and replacement destruction, and Hearthstone aura, silence, transform,
Divine Shield, and healing-to-damage interactions.

## 3.10 Deterministic random oracle

All shuffle, random target, random discard, generated-card selection, and coin
flip requests go through a match-owned oracle. Each draw records purpose,
domain, result, stream ID, and sequence number in the authoritative replay.
Scripts and native callbacks cannot access process RNG.

A simulation may substitute a scripted oracle that explores every branch, a
seeded oracle for reproducibility, or a recorded oracle for exact replay.

The engine-facing default must own the active random stream. Its current
nonzero state and stream position are part of the authoritative snapshot,
delta, and state hash; an engine-owned shuffle therefore advances that state
only after the zone mutation succeeds. Restoring a snapshot or applying a
delta restores the same random position before any later draw or shuffle.
Explicit caller-supplied RNG state APIs may remain as a low-level integration
path, but they must be documented as external streams and must not be used by
the server authority unless their state is recorded in the match replay.
Tests must verify equal seeds produce equal orders and state positions,
different seeds can produce different orders, failed shuffles do not advance
the state, and snapshot/delta/replay restoration reproduces the next random
result exactly.

## 3.11 Hidden information and authoritative views

Store one complete server state and derive viewer-specific snapshots/deltas.
Redaction must operate on semantic fields, not by zeroing bytes after generic
serialization. Hidden objects use opaque viewer handles that cannot reveal a
definition ID, stable server ID, deck order, secret identity, or random result
before the rules permit it.

Tests must compare two player views and a spectator/server view after every
step. The engine should detect accidental hidden-data references in public
events and rejected client commands.

## 3.12 Match-start procedure and starting state

Match start must be a ruleset-configured, deterministic transaction rather
than an implicit constructor side effect. All values in this section must be
provided by the match configuration or profile so that two matches can use
different starting conditions without changing engine code. The profile
supplies, at minimum:

- legal player count, seat assignment, and turn order;
- starting life or health, including per-player overrides;
- starting resource pools, maximum resource capacity, and initial lock state;
- required main, extra, side, commander, hero, or other deck sizes;
- maximum hand size and whether setup may temporarily exceed it;
- opening draw count and draw timing;
- first player, first priority, and the initial response window;
- initial phase, turn number, active player, and priority state;
- setup-trigger timing and ordering;
- visibility and private dealing rules; and
- mulligan/replacement, setup allowance, and setup win-check policies.

The engine must execute setup in an explicit ordered pipeline:

```text
validate profile and seats
    -> validate deck/corpus legality and required zones
    -> create physical card instances with stable IDs
    -> place cards into configured zones
    -> initialize life, resources, counters, allowances, and turn state
    -> perform configured opening draws/deals
    -> open the configured mulligan or replacement window
    -> resolve setup replacements and shuffles
    -> publish the initial authoritative snapshot and state hash
    -> grant first priority and emit setup-complete events
```

Every pipeline step prepares changes in a shadow state and commits only after
the complete step succeeds. Allocation failure, invalid deck data, invalid
starting resources, callback failure, or a rejected setup choice must leave
the engine unstarted and the supplied deck/state unchanged. The random-oracle
position used for shuffling and dealing must roll back with the transaction.

Life, hand size, resources, and all opening counts are data rather than kernel
assumptions. A profile may configure 20 life and seven cards, 30 health and
three cards, zero starting cards, multiple starting hands, or a custom
resource model. Zero is legal only when the profile explicitly permits it;
otherwise configuration validation rejects it before state publication.

The starting hand is a first-class hidden-information zone. Draw/deal keeps
physical instance IDs and order, while viewer projections reveal only cards
authorized for that viewer. The configuration selects the hand-overflow
policy: reject, discard, mill, return to deck, or create a pending choice.

Mulligans are configured choices and zone operations, not a special boolean.
The policy controls kept-card selection, return order, shuffling, replacement
draws, whether replacement cards can be selected again, simultaneous versus
seat-order resolution, and when the final opening deck order is established.
The kernel provides a deterministic physical-instance mulligan primitive that
can be used by those policies; it validates the complete selection before
removing anything, reinserts selected entries, applies the configured RNG
shuffle, and draws replacements transactionally. Profile-specific return order,
simultaneous decisions, and replacement eligibility remain configuration and
policy responsibilities rather than being hardcoded into the kernel.

The initial snapshot includes configured life/health, resource state, hand
contents for authorized views, deck order where authorized, active player,
turn/phase/window state, allowances, pending choices, and setup events. The
server keeps the complete state; clients receive a projection that strips
hidden opponent zones. Replaying setup must reproduce IDs, draws, shuffles,
choices, events, and state hashes from recorded random-oracle results.

Required tests cover minimum and maximum player counts, custom life and hand
sizes, zero-value validation, full and undersized decks, duplicate physical
IDs, opening-draw boundaries, every hand-overflow policy, simultaneous and
ordered mulligans, first-player selection, initial priority, setup-trigger
ordering, hidden-zone projections, deterministic reruns, and failure injection
at every allocation and callback boundary. Each rejected case verifies that
no match state, RNG position, deck order, or published event changed.

### 3.12.1 Match-start rule precedence and scoped overrides

Starting values need an explicit precedence model. The match-start
configuration is the authoritative baseline for the match; optional card,
class, leader, commander, hero, scenario, and profile extensions may override
only the fields they are explicitly allowed to modify. An extension must never
silently replace the complete start configuration.

The phrase “game rules take priority” means that the configured match rules are
the source of truth and default starting state for the match. A class or hero
does not get an implicit right to replace those rules merely because it is
present. It can change a starting value only through an explicit, registered,
field-scoped override that the selected profile declares legal. This gives a
Hearthstone-style class/hero exception without making extensions stronger than
the ruleset itself. For example, the game rules may set the baseline health to
20, while a selected hero definition may legally replace that one field with
30. An undeclared health change, or an extension disabled for the match, has no
effect.

Apply rules in this order:

```text
engine safety limits and schema validation
    -> selected profile defaults
    -> match-start configuration
    -> seat/player configuration
    -> optional class/hero/commander/scenario modifiers
    -> ordered setup effects and replacement rules
    -> final legality and invariant validation
```

The engine safety limits are hard bounds and cannot be overridden. Profile
defaults fill fields omitted by the caller. The match configuration then sets
the values for this particular match. Per-player configuration may specialize
those values for a seat. Optional extensions, such as a Hearthstone class or
hero, may then provide a declared override such as `starting_health = 30`,
extra starting armor, a different opening hand size, an extra allowance, or a
class-specific starting resource rule.

Precedence is field-specific, deterministic, and intentionally narrow:

1. Safety limits and schema invariants always win. No configuration or card can
   exceed engine bounds, create an invalid player count, or bypass legality.
2. The selected profile and match game rules establish the baseline values,
   including starting health, hand size, resources, first player, phases, and
   action allowances.
3. Seat/player settings may specialize the baseline where the profile permits
   it.
4. An explicit class/hero/leader/commander override may replace or modify only
   its declared field. A replacement such as starting health is applied to the
   baseline value; unrelated fields remain governed by the game rules.
5. Setup effects run after the resolved starting record and can change values
   only when their configured operation and priority are legal for that field.
   They cannot silently undo a class override or a game-rule invariant.
6. Final validation rejects conflicts, illegal combinations, and values outside
   limits before cards are dealt or random state is advanced.

Overrides are field-scoped and typed. A class effect that changes starting
health must not also change hand size, first player, or deck legality unless
its manifest explicitly declares those capabilities. Each override carries a
stable source ID, target player/seat, field mask, value or operation, priority,
and conflict policy. Supported operations should include set, add, subtract,
multiply/scale where legal, clamp, and replace-with-choice. The kernel applies
only operations supported by the field's schema; arbitrary callback mutation of
the live match state is forbidden.

Conflicts are deterministic. For one field, higher declared priority wins when
the policy is `highest_priority`; equal-priority competing sets are an error
unless the field explicitly allows ordered composition. Additive modifiers are
combined in stable source-ID order, then clamps and final validation run. A
profile may instead declare first-wins, last-wins, or mutually-exclusive
override groups, but the choice must be part of the serialized ruleset hash.
Registration order, pointer addresses, and host iteration order must never
decide the result.

For example, if the match config starts every player at 20 health and the
selected Hearthstone hero/class extension declares a legal starting-health set
to 30, the resolved player state is 30. A later setup effect may add armor or
modify another declared field, but cannot undo the class override unless its
schema and priority explicitly permit it. If the extension is disabled, the
same match configuration resolves to 20 without changing the core engine.

The resolved start record must retain both the baseline and the applied
override list. Replays, snapshots, hashes, diagnostics, and viewer projections
must use the resolved values while retaining source IDs and operation order for
deterministic reconstruction. Failed or conflicting overrides are rejected
transactionally before cards are dealt or the RNG advances.

Tests must cover baseline-only matches, per-player overrides, class/hero
starting-health changes, conflicting equal-priority sets, additive and
replacement composition, disabled optional extensions, illegal field masks,
override priority changes, deterministic source ordering, snapshot/delta
round-trips, replay reconstruction, and allocation/callback failure at every
resolution stage. Every failed case must prove that the baseline config,
published state, deck order, RNG position, and event log remain unchanged.

# 4. Compatibility profile: Magic with Magic Origins

The Magic profile is the strongest test of ordering and continuous state. It
requires:

- library, hand, battlefield, graveyard, exile, stack, sideboard, and command
  or profile-defined zones;
- colored and generic mana, mana pools, variable/additional/alternative costs,
  cost increases/reductions, and payment choices;
- the five normal mana colors, colorless mana, generic requirements, and
  configurable additional symbols without teaching the kernel their meaning;
- mana abilities and other production actions, producer restrictions, choices
  of produced color/type, replacement effects on production, and conditional
  mana that can pay only a configured subset of costs;
- mana-pool emptying at configured step/phase boundaries, temporary mana,
  cost-specific discounts, and effects that preserve otherwise expiring mana;
- land-play action allowances separate from mana and spell casting, including
  the base per-turn allowance, additional/restricted land plays, and effects
  that put a land onto the battlefield without consuming a land-play allowance;
- turn-based actions, priority passing, last-in-first-out stack resolution, and
  state-based actions repeated until stable;
- activated, triggered, static, mana, delayed, and replacement abilities;
- modes, targets, target legality on resolution, division, optional choices,
  and ordered simultaneous triggers;
- attacking, blocking, blocker order, combat damage assignment, first/double
  strike, trample-like policies, evasion, and remove-from-combat state;
- tokens, counters, attachments, control changes, copying, prevention, and the
  legend/planeswalker rules selected by rules version;
- a configurable continuous-effect layer/dependency/timestamp policy;
- zone-change identity and last-known information;
- losing from life, attempted draw from an empty library, poison or configured
  counters, and card-created win/loss conditions.

## Magic Origins conformance corpus

Import and version the 272-card main set and separately tag the 16 supplemental
cards described in the release notes. The corpus must cover at least:

- all five transforming creature/planeswalker double-faced cards;
- renown and its one-time successful-player-damage condition;
- spell mastery checking graveyard contents during resolution;
- prowess triggering from casting a noncreature spell and resolving before it;
- menace block legality, including blockers later leaving combat;
- scry and player-controlled top/bottom ordering;
- planeswalker loyalty costs and attacks;
- X costs and counters, Auras/Equipment, tokens, delayed triggers, copy and
  control effects, conditional replacement effects, and mass zone movement;
- difficult representative interactions such as a choice that may not repeat,
  a turn-ending effect, type-changing enchantments, damage/healing replacement,
  and a targeted-trigger control effect.

Use official release notes as the card-ruling oracle, but pin a comprehensive
rules revision in the test manifest. A newer comprehensive rulebook can change
the behavior of an old set, so the manifest must state whether the target is
historical Origins rules or current Magic rules applied to Origins cards.

# 5. Compatibility profile: modern Yu-Gi-Oh! with LOB and modern summons

The initial card corpus is the official 126-card Legend of Blue Eyes White
Dragon list. Apply modern field, turn, chain, terminology, and card-database
rulings rather than recreating 2002 procedures.

The profile needs:

- owner versus controller, face-up/down state, attack/defense position, Set,
  Flip, Normal/Special/Tribute summons, and summon-negation windows;
- Main Deck, hand, Graveyard, banished zone, Extra Deck with face-up state,
  Main/Extra Monster Zones, Spell/Trap/Pendulum Zones, Field Zone, and linked
  zone topology;
- chains, activation legality, spell-speed/response restrictions, activation
  negation versus effect negation, costs, targeting, and resolution without
  revalidating unrelated conditions;
- trigger timing, simultaneous triggers, mandatory/optional ordering, Damage
  Step restrictions, missed-timing policy where applicable, and precise
  conjunction semantics represented by effect programs rather than text parsing;
- cards that remain attached as Equip Cards or Xyz materials, counters, tokens,
  control changes, lingering restrictions, and alternate victory conditions;
- per-object and per-name/effect usage limits using the generic ledger.

### Normal Summon and Set allowances

Represent the ordinary once-per-turn Normal Summon or Set as one shared base
action allowance. A Normal Summon and a Normal Set compete for that same use;
they are not two separate counters. The allowance resets at the profile's turn
boundary and is owned by the turn player.

Normal Summon procedures must validate the monster's current Level and other
characteristics, available monster zone, required Tribute count, Tribute
eligibility, and effects that change or waive Tributes. Setting uses the same
allowance and cost/procedure framework while producing face-down Defense
Position according to modern rules.

Cards and rules may grant additional Normal Summons/Sets. Model each grant as an
allowance with its own source and predicate rather than incrementing one global
counter. It must be possible to express:

- one additional unrestricted Normal Summon/Set this turn;
- one additional summon only for a named archetype, type, attribute, level
  range, owner, origin zone, or other configured predicate;
- an additional summon that can only be used immediately while an effect
  resolves;
- a summon performed by an effect that does not consume the standard Normal
  Summon/Set allowance;
- an effect that changes Tribute requirements without granting another summon;
- a prohibition that blocks Normal Summons even when an allowance remains;
- several additional-summon effects whose official combination rule permits or
  forbids using more than one of them in the same turn;
- an allowance whose source is negated, leaves the field, changes control, or
  stops satisfying its continuous condition before use.

The Yu-Gi-Oh! profile must supply an allowance-combination policy. Some
additional Normal Summon effects modify the number of available actions, while
others provide a separately restricted additional action; they cannot all be
treated as freely cumulative. The engine must expose eligible allowances and
let the policy or player select the one consumed. The replay records that
selection.

The standard allowance is consumed when the summon or Set reaches the
rules-defined committed point, not merely when a client submits an invalid
request. Summon attempts, summon negation, effect-granted summons, and actions
that resolve without successfully summoning need separate golden tests based on
the pinned modern rule/card ruling.

## Summoning mechanics

- **Fusion:** validate named or predicate materials, move them according to the
  selected fusion procedure, and summon an Extra Deck object.
- **Ritual:** model a main-deck ritual monster, activating ritual effect,
  level/value requirements, tributes/material choice, and resolution failure.
- **Synchro:** choose a permitted face-up Tuner and non-Tuners whose effective
  levels exactly satisfy the Synchro monster and any material predicates.
- **Xyz:** select monsters satisfying rank/material predicates, place them as
  ordered attached materials rather than sending them to the Graveyard, and
  detach as a cost or effect.
- **Pendulum:** support dual monster/spell faces, scales, once-per-turn summon
  action, face-up Extra Deck routing, and legal destinations for summons from
  hand versus face-up Extra Deck.
- **Link:** support Link Rating, exact material contribution, arrows, linked and
  co-linked zones, Extra Monster Zones, no Defense Position/DEF, and destination
  legality derived from the board topology.

These mechanics should be configured summon procedures composed from material
selectors, numeric constraints, destination policies, and cost/move operations,
not six unrelated hardcoded functions.

## LOB and modern-mechanic tests

LOB conformance must include normal monsters, Flip effects, effect monsters,
normal/equip/field/ritual spells, normal and continuous traps, Fusion,
destruction, revival, control change, direct damage/healing, attack modifiers,
turn-counting restrictions, hand/deck interaction, and Exodia's alternate win.

Modern mechanics should use small synthetic card definitions or legally
supplied fixtures designed to isolate each rule. Add scenarios for:

- hard and soft once-per-turn effects after control change, negated activation,
  negated effect, face-down transition, leaving/returning, and copied names;
- effect use versus activation use and when the limit is consumed;
- simultaneous mandatory/optional triggers owned by both players;
- illegal responses by speed/window and Damage Step;
- target becoming illegal versus non-targeted selection during resolution;
- Xyz material detachment and destination behavior;
- Pendulum cards destroyed as monsters or spells and later summoned;
- Link arrows changing legal Extra Deck destinations;
- exact Synchro levels after continuous level changes;
- summon procedure interrupted before and after costs are committed.

# 6. Compatibility profile: Hearthstone 2014 Basic/Classic

Pin the profile to Blizzard's documented 240-card 2014 Classic format corpus
and patch `1.0.0.5832`. Do not silently use current Legacy/Core values.

The profile needs:

- class and neutral deck legality, 30-card decks, copy/legendary limits, starting
  player selection, mulligan, and the second-player compensation policy;
- heroes, hero health/armor, class hero powers, weapons and durability;
- automatic mana growth/refill, overload-like locked resources, one hero-power
  use per turn, and configurable hand/board capacities;
- immediate spell/Battlecry resolution with no general player response stack,
  but deterministic event queues, Secrets, Deathrattles, and triggered effects;
- minion attack readiness, Taunt, Charge, Windfury, Stealth, Divine Shield,
  Silence, Transform, Freeze, enrage-style conditions, tribal tags, and auras;
- ordered board positions and adjacency;
- draw, burn at full hand, fatigue, generated cards, random targeting, and
  deterministic random ordering;
- temporary and permanent buffs, set-stat effects, damage separate from health
  changes, healing replacement, control changes, and end-of-turn return;
- simultaneous-death batching and the pinned event ordering used by the client.

### Hearthstone mana-crystal model

Represent mana capacity and spendable mana separately:

- `maximum_crystals`: permanent unlocked capacity, normally increased at the
  start of a turn up to the configured cap;
- `filled_crystals`: currently spendable units after refresh and spending;
- `temporary_crystals`: spendable units that do not permanently increase the
  maximum and expire at the configured boundary;
- `locked_next_turn` and `locked_current_turn`: capacity unavailable because
  of a delayed lock/Overload-like effect;
- scheduled crystal gains, destruction, locks, unlocks, and refresh changes.

At turn start, process scheduled maximum changes and lock transitions in a
profile-defined order, grow maximum capacity when applicable, calculate usable
capacity, and refill spendable crystals. This order must be encoded and tested;
it cannot depend on event registration order.

The model must support conditional and restricted mana behavior even when the
2014 corpus uses only part of it:

- temporary crystals granted for the current turn;
- crystals or discounts usable only for a card class/type, hero power, or next
  matching action;
- cost reduction/increase while a source is active;
- setting a card's current cost without changing its base cost;
- a card becoming free under a condition;
- maximum-crystal gain, loss, destruction, and restoration;
- gaining an empty versus filled crystal;
- a conditional start-of-turn crystal or refresh modifier;
- lock effects scheduled now and applied on one or more future turns.

Spending selects ordinary, temporary, and restricted units through the generic
payment solver. The Hearthstone profile defines its preferred automatic order
and whether any unusual fixture needs a player choice. Temporary or restricted
mana must not be silently converted into permanent maximum crystals.

Add focused tests for turns zero through the cap, going first/second setup,
partial spending, refresh, temporary mana expiry, gaining empty/filled crystals,
destroying maximum crystals below current spendable mana, current/future locks,
several lock sources, discounts and cost setters, conditional free cards,
insufficient mana, full rollback, snapshot/replay, and viewer-visible values.

Representative conformance scenarios must cover:

- Battlecry and Combo-like conditional entry effects;
- Deathrattle ordering and simultaneous deaths;
- Secrets with overlapping trigger conditions;
- random target selection and mass random survival;
- spell-cast triggers and damage-trigger chains;
- auras entering/leaving, Silence, Transform, copied minions, and set-stat
  effects interacting with buffs/damage;
- temporary control and return when board capacity changes;
- weapons, hero attacks, armor, durability, Freeze, and Windfury;
- full hand draw burn, empty-deck fatigue, and generated-card identity.

# 7. Testing framework

## 7.1 Deterministic scenario runner

Add a CardGame test harness that loads a ruleset and scenario description:

```text
given
  ruleset hash and card corpus version
  players, decks, zones, starting state, active player/phase
  deterministic RNG script
when
  intent or engine event
  explicit responses and choices
then
  legal action/choice set
  authoritative state hash
  per-viewer redacted state hashes
  ordered operation/event trace
  winner and pending decision point
```

After every operation and stable boundary, run invariants for unique object
ownership, one-zone membership, valid handles/incarnations, legal attachments,
capacity, nonnegative configured resources, trigger/choice ownership, resolution
container consistency, and hidden-information isolation.

Failures should print the smallest useful trace: scenario, seed, rules hash,
last successful action, pending choices, transaction operations, event/trigger
queue, RNG draws, and state diff. Meaningful return codes must be asserted, not
cast to `void`.

### 7.1.1 Match replay, result records, and reconstruction

Every authoritative match produces a schema-versioned replay stream and a
separate result record. The replay header stores the profile/ruleset hash,
card-corpus hash, engine schema version, player/seat IDs, canonical deck
hashes, initial snapshot hash, visibility policy, and master RNG seed or a
complete recorded-randomness stream. It never stores raw callbacks, function
pointers, process addresses, or unrevealed private data in a public replay.

Each event records a monotonic sequence number, turn/phase/window, acting
player, command or event kind, stable object/incarnation IDs, explicit choices,
selected payment plan, relevant RNG result, outcome, and post-event state hash.
Viewer-specific redaction is derived from the same stream using opaque handles
for hidden cards and random results.

Replays support two explicit visibility modes:

- **Full-information mode:** stores the complete authoritative match history,
  including every player's hidden-zone contents, hidden card identities,
  private choices, unrevealed random results, and all server-only state. This
  mode is for debugging, judging, balancing, trusted spectators, and exact
  simulation analysis. It must be access-controlled because it reveals
  information that players were not entitled to see during the match.
- **Player-view mode:** stores one player's permitted view. That player's own
  hand, deck contents when known to them, private choices, and private results
  remain available; opponent hidden-zone contents are stripped rather than
  replaced with recoverable encrypted or opaque card data. Opponent public
  plays, revealed cards, public choices, visible zones, turn/phase changes,
  combat, effects, and legal public outcomes remain visible. Unknown cards may
  be represented by non-linkable opaque handles or counts, but the replay must
  not retain enough metadata to reconstruct their identities or original deck
  order.

The visibility mode is immutable in the replay header and included in exported
metadata. A player-view replay must be generated from the viewer-authorized
projection before storage/export, not by hiding fields at playback time. The
loader must reject attempts to upgrade a player-view replay into full
information. Reconstruction of a player-view replay replays all public events
and the viewer's private events while treating stripped opponent state as
unknown; it must never invent hidden information or use it to affect legal
decisions. Full-information reconstruction verifies the complete state hash,
while player-view reconstruction verifies the viewer-visible hash and public
event trace.

A trusted full-information replay may be projected into a player-view replay
for a selected player. Projection walks the complete event and snapshot stream,
retains that player's authorized private data and all public information, and
removes opponent hidden-zone contents, hidden identities, private choices,
unrevealed random results, and metadata that could reconstruct them. The
projection happens before persistence or export and emits a new replay header
containing the selected viewer ID, player-view mode, source replay hash, and
projected initial/viewer-visible hashes. This is one-way: the derived file must
contain no recoverable full-information payload, and loading it must never
permit conversion back to full information.

Replay loading reconstructs the initial state, verifies all referenced hashes,
then feeds recorded commands and choices through the normal authoritative
engine. It re-runs validation, costs, effects, triggers, timing, RNG, and state
hashing instead of directly applying saved mutations. At each event the
recorded hash and expected result are compared. A mismatch reports the first
sequence number, state diff, RNG position, pending choices, and trace, then
stops transactionally.

The API must support save/load from buffers and files, metadata-only
inspection, bounded seeking through periodic snapshots, and viewer-redacted
export. Appending an event is transactional, and optional compression occurs
after canonical event encoding without changing hash identity. Schema migration
is explicit; unknown schemas fail closed.

Replay event storage grows geometrically from a small initial allocation
through the CMA backend. It never silently discards old events when a match
exceeds the initial capacity. A shared explicit safety ceiling (currently
16,777,216 records) prevents malformed input or an unbounded live match from
exhausting memory; reaching that ceiling returns `FT_ERR_FULL` before state is
changed. The command recorder used by a live engine is a separate
implementation detail and must be sized or streamed consistently when it is
used as the replay source; it must not silently impose a smaller effective
replay limit.

Replay tests cover complete and empty matches, every command and choice kind,
priority windows, simultaneous triggers, random and hidden events, invalid
commands, effect failures and rollback, disconnect/timeout, concessions,
draws, corrupted/truncated records, wrong ruleset/corpus/deck hashes,
viewer-specific redaction, snapshot seek equivalence, compressed versus
uncompressed equivalence, cross-platform byte/hash stability, and repeated
reconstruction to identical final hashes and results.

## 7.2 Card corpus manifests

Each imported definition has a manifest entry:

```text
official/source identifier
ruleset and set version
definition hash
mechanics tags
required isolated scenarios
required interaction suites
oracle/source references
implemented/blocked/verified status
```

Conformance levels are:

1. schema and reference validation for every card;
2. isolated legal play/activation and primary resolution for every card;
3. all alternate modes, targets, costs, zones, and failure paths;
4. tagged pairwise and multi-card interactions;
5. seeded full-game simulation and replay verification.

A set is not “supported” merely because all definitions parse.

## 7.3 Test layers

### Kernel unit tests

- identity/incarnation and stale-handle behavior;
- every zone topology, move, inspect, search, shuffle, draw, and visibility rule;
- transaction rollback under failure injection at every allocation/commit step;
- choice lifecycle, stale/forged choice IDs, cancellation, and defaults;
- cost commitment versus failed/negated resolution;
- stack/queue admission and nested resolution policies;
- usage-limit scopes and reset epochs;
- continuous-effect dependency/order/cycle handling;
- replacement selection, recursion prevention, and event suppression;
- RNG scripted/seeded/recorded modes;
- snapshot, delta, replay, schema migration, and canonical hash stability.

### Profile conformance tests

Implement profile tests from sections 4–6 as data-driven scenarios. Every
mechanics tag in a corpus manifest must map to at least one focused test. Tests
must cite the pinned rule or official ruling used as their oracle.

### Interaction matrix

Generate pairwise coverage from mechanics tags instead of attempting every card
pair blindly. High-risk combinations include:

- move/transform/copy with continuous effects;
- trigger with replacement/prevention;
- control change with owner-based zone movement;
- hidden search/reveal with shuffle;
- cost payment with negation/countering;
- simultaneous death/destruction with leave-play triggers;
- stat setting with additive/multiplicative modifiers and damage;
- once-per-period limits with identity changes;
- attachment/material relationships across zone changes;
- turn/phase changes while delayed effects exist.

Add explicit three-way scenarios where pairwise coverage cannot expose ordering
cycles or replacement interactions.

### Full-game simulation

Supply deterministic agents:

- scripted agent for golden matches;
- first-legal-action agent for state-space smoke tests;
- seeded random legal agent for broad exploration;
- exhaustive bounded agent for small synthetic games;
- adversarial agent that submits stale, illegal, hidden-information, and
  out-of-turn commands.

Run complete games using representative legal decks for each corpus. Assert
termination or a configured turn/action cap, no invalid state, reproducible
winner and trace, replay equivalence, and identical cross-platform state hashes.
When a cap is reached, report it as a distinct inconclusive result, not a win or
engine failure.

## 7.4 Property, metamorphic, and fuzz testing

- Shuffling preserves the exact multiset and produces recorded deterministic
  order for a seed.
- Serialize/deserialize and snapshot/replay preserve canonical state.
- Redacting a state never adds information when viewer permissions decrease.
- An operation that fails before commit leaves state and RNG unchanged.
- Copying a deck and applying the same commands/choices/RNG yields equal hashes.
- Renaming non-semantic fixture labels does not change a match result.
- Permuting unordered internal storage does not alter canonical decisions.
- Fuzz configuration, effect bytecode, commands, choices, snapshots, deltas,
  and replay loaders with strict size/instruction budgets.
- State-machine fuzz legal and illegal action sequences and minimize failures
  into scenario fixtures.

## 7.5 Independent oracles and differential checks

Do not make tests calculate expectations by calling the same production helper.
Use small independent reference models for zones, usage limits, resource payment,
and simple combat. For complex official interactions, use reviewed golden traces
with a rule citation and corpus version.

When licensing and automation permit, compare selected scenarios against an
external rules implementation. Treat discrepancies as review items, not as
automatic proof that either side is correct.

## 7.6 Network and security tests

- Server accepts intents, never client-authored operations or state deltas.
- Rejected commands cannot advance RNG, consume limits, reveal cards, or mutate
  sequence numbers.
- Per-player deltas reveal only authorized fields and opaque handles.
- Reconnect snapshots and subsequent deltas converge to authoritative state.
- Duplicate, delayed, reordered, and replayed commands are idempotently rejected.
- Ruleset/corpus hash mismatch stops the match before setup.

## 7.7 Reliability and performance

Run ASan, UBSan, TSan, CMA named-stage failure injection, lifecycle-abort tests,
and release archive checks. Fuzz and stress hooks compile only for tester builds.

Benchmark large decks/zones, trigger storms, continuous-effect reevaluation,
large choice sets, deep but bounded resolution, many simultaneous objects, full
snapshot versus delta, redaction for several viewers, and thousands of seeded
games. Set explicit limits for operations, triggers, replacement depth, choices,
script instructions, and turns so malicious configurations fail predictably.

# 8. Delivery plan

## Phase 0: specification freeze

- Version the action, operation, event, choice, object, and zone schemas.
- Pin the three profile rule revisions and corpus manifests.
- Decide compatibility and deprecation paths for current index-based APIs.

## Phase 1: universal state kernel

- Stable object/incarnation identity and generalized zones.
- Authoritative and redacted views.
- Deterministic RNG and canonical state hashing.
- Atomic operation transactions and failure injection.

## Phase 2: decision and execution kernel

- Generic intents, legal-action enumeration, choices, and costs.
- Event pipeline, resolution containers, timing windows, and trigger collection.
- Usage-limit ledger and delayed scheduling.

## Phase 3: derived rules

- Continuous-effect and replacement/prevention pipelines.
- Configurable combat and state/win checks.
- Verified scripting programs and operation-only native callbacks.

## Phase 4: profiles and corpora

1. Hearthstone 2014 profile, because its mostly immediate resolution provides a
   smaller first full-game target.
2. Yu-Gi-Oh! LOB profile, followed by isolated modern summon/timing fixtures.
3. Magic Origins profile after priority, replacement, and layered continuous
   state are proven.

The order is implementation risk management, not a claim that one game's rules
are strictly simpler in every interaction.

## Phase 5: simulation and networking

- Golden, random, exhaustive-bounded, and adversarial agents.
- Full-game corpus gates and interaction matrices.
- Viewer-safe authoritative command/delta integration.
- Performance baselines and long deterministic soak runs.

# 9. Acceptance criteria

- No full-simulation API identifies an object by a mutable array index.
- Every fallible effect path is transactional and test-injectable.
- Every player decision is explicit, serializable, and replayable.
- The same ruleset, state, choices, and RNG record produce identical hashes and
  traces on Linux, macOS, and Windows.
- Hidden-information tests prove that each viewer sees only permitted data.
- All definitions in each pinned corpus pass schema/reference validation and
  have mechanics-tag coverage; representative decks complete full games.
- Modern Yu-Gi-Oh! summon procedures and hard/soft usage-limit edge cases pass.
- Magic Origins transformation, priority, triggers, replacements, layers, and
  combat scenarios pass.
- Hearthstone 2014 event ordering, random effects, Secrets, Deathrattles,
  Silence/Transform, capacity, and fatigue scenarios pass.
- Replays remain stable for their recorded schema/rules hash or fail with an
  explicit incompatibility error.
- Tester-only controls, fuzz hooks, and diagnostics are absent from normal
  release archives.

# 10. Research sources and versioning notes

Use these primary sources when building the initial manifests and golden
scenarios:

- Magic comprehensive and basic rules:
  <https://magic.wizards.com/en/rules>
- Magic Origins official card gallery:
  <https://magic.wizards.com/en/news/card-image-gallery/magicorigins>
- Magic Origins mechanics:
  <https://magic.wizards.com/en/news/feature/magic-origins-mechanics>
- Magic Origins release notes and card-specific rulings:
  <https://magic.wizards.com/en/news/feature/magic-origins-release-notes-2015-07-08>
- Yu-Gi-Oh! official rulebook hub:
  <https://www.yugioh-card.com/en/rulebook/>
- Yu-Gi-Oh! official modern beginner/summoning guide:
  <https://www.yugioh-card.com/en/forbeginners/>
- Yu-Gi-Oh! official Legend of Blue Eyes White Dragon card database list:
  <https://www.db.yugioh-card.com/yugiohdb/card_search.action?ope=1&page=1&pid=11101000&rp=99999&sort=2>
- Yu-Gi-Oh! official LOB product summary:
  <https://www.yugioh-card.com/eu/product/legend-of-blue-eyes-white-dragon/>
- Hearthstone official description of the pinned 2014 Classic format and its
  240-card corpus:
  <https://news.blizzard.com/en-us/article/23620129/introducing-the-core-set-and-classic-format>

Record retrieval date, locale, rules revision, set/product ID, and a content
hash in every imported manifest. Official pages and databases can change or
apply errata; silently refreshing them would make old golden tests and replays
nondeterministic.
