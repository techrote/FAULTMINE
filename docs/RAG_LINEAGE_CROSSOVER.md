# FAULTMINE crossover, favourites and lineage contract

This document records the FM-010 evolutionary persistence contract. It extends the FM-009 mutation/tray model and supersedes any earlier implication that a tray pin can only be session-local.

## State boundaries

FAULTMINE keeps three histories deliberately separate:

- the **canonical genome**, which together with canonical source content determines pixels;
- **manual edit history**, used only for deliberate editor undo/redo;
- the **specimen lineage DAG**, used for retained mutation/crossover ancestry, favourites and evolutionary navigation.

Lineage, favourites, mutation/crossover provenance and locks are project state. They do not enter canonical genome identity and do not alter canonical pixels by themselves.

## Durable specimen identity

A retained specimen node is keyed by the canonical genome SHA-256 identity from `RAG_DETERMINISM.md`. Each node stores:

- the specimen/genome identity;
- the canonical source identity it belongs to;
- the full canonical genome needed to reactivate it;
- the lock state associated with that retained genome;
- one or more derivation records;
- a durable favourite flag;
- a non-semantic creation ordinal.

Rendered thumbnail/full pixel buffers are not persisted in lineage. They remain disposable cache/presentation data.

If the same canonical genome is retained again, FAULTMINE merges it into the existing node rather than creating an indistinguishable duplicate. A node may therefore accumulate more than one truthful derivation path. Favourite state is never erased merely because the same genome is encountered again.

## Derivation records

Every retained node has at least one explicit derivation record. Supported FM-010 kinds are:

- `manual-root` — current/manual state retained without claiming an evolutionary parent;
- `mutation` — one parent plus mutation policy version, mutation seed, descendant index and mutation radius;
- `crossover` — two or more parents plus crossover policy version and crossover seed;
- `imported-genome` — explicit unparented imported state reserved for compatible import paths;
- `legacy-project-root` — project-v1 migration root, deliberately carrying no fabricated pre-v1 ancestry.

Parent IDs are canonical genome identities and are normalized into deterministic order where order has no semantic meaning.

## Lineage DAG invariants

Lineage is a directed acyclic graph, not a mutable undo stack.

- every referenced parent must already be retained;
- self-parent edges are invalid;
- adding a derivation that creates a directed cycle is rejected;
- every retained node must match the project source identity;
- each stored genome must validate against the current registry;
- stored locks must target existing operators/declared parameters;
- the active specimen ID must identify a retained node when loading a persisted lineage;
- unfavouriting never deletes a node or its ancestry.

FM-010 intentionally provides no automatic lineage garbage collection. That avoids orphaning descendant provenance. A future explicit pruning feature must prove reachability/provenance safety before deleting nodes.

## Typed crossover policy v1

Crossover is a core typed genome operation with `crossover_policy_version = 1`. It accepts 2..8 distinct canonical parent genomes and one explicit 64-bit crossover seed.

Parent selection order is normalized by canonical genome identity before any entropy is consumed, so selecting the same parent set in another UI order cannot change the child.

### Operator alignment

For each operator in the deterministic backbone parent, compatible operators in other parents are aligned conservatively in this order:

1. same stable instance ID, type ID and type version;
2. otherwise the same type/version at the same stack position when unused;
3. otherwise the first unused same-type/version operator.

This prefers stable identity where branches share ancestry while still allowing independently-created compatible operators to cross.

When aligned operators carry different stable IDs, the child receives a deterministic derived instance ID. The derivation uses only persisted parent/crossover inputs and the named deterministic entropy contract. It never uses wall-clock/process randomness.

### Gene inheritance

For an aligned operator, enabled state and each declared parameter are selected independently by named deterministic streams derived from the explicit crossover seed and stable alignment context. This is typed parameter inheritance: FAULTMINE does not splice canonical JSON text or corrupt host memory.

Unmatched operators are handled explicitly:

- a whole-operator or parameter-protected unmatched operator is always retained;
- an unprotected unmatched operator is included/excluded by a named deterministic stream;
- child topology remains bounded by the FM-009 64-operator exploration limit.

The child root seed is the explicit crossover seed. The returned provenance records sorted parent genome identities, crossover policy version, crossover seed and resulting child identity.

## Lock interaction

Crossover respects the same project lock intent as mutation.

- a protected aligned parameter must retain its protected value;
- if multiple aligned protected parameters disagree, crossover fails with a structured conflict rather than choosing one silently;
- an unmatched protected operator/parameter is conservatively carried into the child;
- inherited protection is transferred onto any synthesized child instance ID;
- whole-operator protection cannot be discarded by an unmatched-operator coin flip.

Whole-operator protected candidates that cannot be reconciled without violating protection are rejected rather than silently weakened.

## Validation boundary

Crossover candidates are registry-validated before they can be returned. Session promotion/breeding then performs the normal full-resolution canonical CPU render before adopting the child as active state. Invalid genomes or failed canonical renders never become silently-active specimens.

A crossover child whose canonical genome identity exactly equals one of its selected parents is not inserted as a self-parent lineage edge; the session reports that the seed should be changed instead.

## Favourites

The FM-010 favourite action is durable project state. Marking a mutation descendant favourite first retains its truthful mutation derivation if needed and then sets the node favourite flag.

Favourites:

- survive tray rerolls because the durable flag lives in lineage, not in thumbnail layout;
- survive project save/reload;
- remain reachable through lineage activation even after their original thumbnail is gone;
- may be unfavourited without deleting the lineage node.

The FM-009 in-memory `SpecimenTrayItem::pinned` flag remains useful presentation state for the current tray; FM-010 synchronizes user favourite actions with durable lineage rather than treating that flag as authoritative persistence.

## Project v2

FM-010 advances `.fmproj` to schema version 2. Top-level canonical order is:

```text
project_version
source
genome
locks
lineage
session
ui
```

The top-level `genome` and `locks` remain the active editor state for straightforward inspection and compatibility with the existing session boundary. In v2 they must exactly match the lineage node named by `lineage.active_specimen_id`.

`lineage` contains the active specimen ID and canonicalized retained specimen records. Each retained record contains source identity, embedded genome, locks, favourite state, creation ordinal and canonicalized derivation records.

The parser validates the complete DAG and source binding before accepting project state.

## Project-v1 migration

Project v1 remains a supported migration input. It had no lineage field and therefore contains no evidence from which historical parents can be reconstructed.

On v1 load FAULTMINE:

1. preserves the exact source reference, genome, lock, session and UI state;
2. creates exactly one retained node for that genome;
3. labels its derivation `legacy-project-root`;
4. assigns no parent IDs and no mutation/crossover policy claim;
5. normalizes the in-memory document to project v2 for the next save.

This is an explicit truthful migration. It does not infer or fabricate ancestry.

## Native exploration surface

The native Explore surface keeps FM-009 tray generation and adds FM-010 evolutionary operations. The menu/buttons/hotkeys provide:

- generate/reroll descendants;
- promote selected descendant with its mutation derivation;
- toggle durable favourite;
- toggle selected descendants as crossover parents;
- breed selected parents using the visible exploration seed;
- navigate to a retained parent or descendant without using undo/redo;
- activate the next durable favourite;
- inspect active provenance in the status surface;
- adjust mutation radius.

Baseline shortcuts are `Ctrl+Alt+G`, `Ctrl+Alt+R`, `Ctrl+Alt+Enter`, `Ctrl+Alt+P`, `Ctrl+Alt+M`, `Ctrl+Alt+B`, `Ctrl+Alt+Up`, `Ctrl+Alt+Down`, `Ctrl+Alt+F`, `Ctrl+Alt+I`, and `Ctrl+Alt+Left/Right` for radius.

## Verification

`crossover_lineage_project_contracts` covers:

- deterministic parent-order-independent crossover;
- reproducibility from parent identities + policy + explicit seed;
- typed alignment and deterministic synthesized IDs;
- protected-gene/unmatched-operator preservation and conflict rejection;
- invalid candidate rejection before rendering;
- duplicate specimen deduplication and durable favourite state;
- multi-parent DAG edges and cycle rejection;
- project-v2 canonical save/reload;
- explicit project-v1 migration without invented ancestry;
- session favourite/select/breed/promote/navigation behavior separate from edit undo;
- source identity validation after evolutionary project reload.

The real Windows smoke additionally generates/render descendants, retains a favourite, selects two distinct parents, breeds a child, verifies multi-parent lineage and renders the promoted result. Debug and Release smoke remain required CI gates.
