# FAULTMINE deterministic mutation and specimen-search contract

This document records the accepted FM-009 mutation-policy and specimen-tray contracts. It extends the deterministic core, project/editor and session/presentation contracts without changing canonical rendering semantics.

## Architectural boundary

Mutation is a pure core/model operation. `generate_descendant()` accepts a canonical parent genome, the operator registry, an explicit mutation request and project/exploration lock state. It returns a typed descendant genome plus derivation metadata. Win32/D3D code never owns mutation semantics.

A tray thumbnail is a non-canonical presentation of the exact descendant genome. Promotion validates and renders that same genome through the full canonical CPU path; thumbnail pixels are never adopted as canonical output.

Locks, tray selection, pins and mutation controls are exploration state. They do not enter canonical genome identity or change pixels by themselves.

## Mutation policy v1

The persisted/provenance-facing mutation policy version is `1` (`kMutationPolicyVersion`). The implementation also validates every mutable parameter's descriptor-local mutation policy version before generation.

One descendant is independently addressed by:

- canonical parent genome identity;
- explicit mutation seed;
- descendant index;
- categorical mutation radius;
- global mutation-policy version;
- stable operator instance and gene/purpose identity for local streams;
- descriptor-local mutation-policy version where a gene is mutated.

All entropy comes from FM-002 named deterministic streams. There is no process-global sequential mutation RNG. Generating descendant `N` therefore does not depend on generating `0..N-1`, tray render order, worker completion order or thread count.

The current mutation engine permits at most 64 operators in a descendant stack. Every completed descendant is validated against the current operator registry before it can be returned.

## Typed radius semantics

Radius is categorical rather than one generic scalar multiplied across unrelated parameter kinds.

### Low

Low radius preserves pipeline topology and mutates exactly one eligible gene when one is available.

- signed/unsigned ranged integers move by one declared descriptor step;
- choices move to an adjacent declared alternative;
- bitmasks flip one bit;
- colours, palette entries and LUT entries make a small local byte change;
- booleans toggle.

### Medium

Medium radius preserves pipeline topology while selecting up to two eligible genes. Numeric, choice, mask and colour-domain changes may move farther within each typed descriptor's declared domain.

### High

High radius selects up to three eligible genes with broad typed movement. It may additionally perform at most one bounded topology mutation selected deterministically from:

- compatible registered-operator insertion;
- unlocked-operator removal;
- reorder within lock-safe topology regions;
- unlocked-operator substitution by another registered type.

New operator instance IDs for insertion/substitution are derived deterministically from mutation seed, parent identity, descendant identity/purpose and operation-specific stable data. A topology candidate that cannot satisfy registry, lock or size constraints is not forced through by weakening validation.

## Lock semantics during mutation

FM-008 exposes whole-operator and individual-parameter mutation locks. FM-009 gives those locks the following topology meaning.

### Whole-operator lock

A whole-operator lock protects all mutation-relevant state for that instance and acts as a **topology anchor**:

- its parameters are never mutated;
- it is never removed or substituted;
- it is never moved;
- insertion, removal or reorder elsewhere may not change its stack index.

Consequently, insertion/removal candidates are restricted so every whole-operator anchor keeps its exact position. Reorders may occur only without crossing an anchor. If every operator in a non-empty stack is whole-operator locked, high-radius topology mutation is disabled and the child is an intentional no-op unless some other explicitly eligible mutation surface is introduced by a later policy version.

### Parameter lock

A parameter lock protects that exact gene. Because deleting or substituting its owner would destroy the protected gene, the owner is excluded from removal/substitution while that parameter lock exists. A parameter lock does **not** by itself freeze the owner's stack index; whole-operator lock is the topology-anchor mechanism.

Locks remain outside the canonical genome. Changing only lock state therefore does not alter canonical genome identity or output.

## No-op descendants

A descendant may equal its parent only when the current policy has no applicable unlocked mutation for the request, such as a fully whole-operator-locked stack. This is represented as a successful `MutationResult` with `changed == false` and a non-empty explanatory `no_change_reason`; it is not silently treated as a mutation failure.

## Specimen tray model

The FM-009 tray is session exploration state. Its default population is 8 specimens; the current native surface offers 4, 8 and 12, while the model supports up to 32 and has no semantic dependency on one tray count.

Generation stores exact descendant genomes and provenance before thumbnails are rendered. Thumbnail rendering is incremental and uses the deterministic nearest-neighbour proxy contract from FM-004. A generation token makes work from an obsolete reroll safe to discard. Job completion order is never specimen identity or display order.

The tray supports:

- explicit mutation seed and deterministic reroll seed;
- low/medium/high radius selection;
- select a specimen and compare its proxy on the main canvas;
- promote the selected descendant after a fresh full-resolution canonical validation;
- pin/unpin specimens so they survive subsequent rerolls in the current session;
- visible descendant index/seed/identity provenance and render/busy/error state.

Pins are intentionally session-only in FM-009. Durable favourites and lineage persistence belong to FM-010.

Tray generation does not enter manual undo/redo history. Promotion is an exploration transition to a new active genome, not a fabricated sequence of manual parameter edits.

## Native controls

The Explore menu and tray expose:

- `Ctrl+Alt+G` — generate using the explicit current seed;
- `Ctrl+Alt+R` — deterministically advance the mutation seed and reroll;
- `Ctrl+Alt+Enter` — promote selected specimen;
- `Ctrl+Alt+P` — pin/unpin selected specimen;
- `Ctrl+Alt+Left` / `Ctrl+Alt+Right` — decrease/increase mutation radius.

Mouse selection remains available for dense tray browsing; the principal generation/promotion/pin/radius loop is keyboard-accessible.

## Verification contract

FM-009 automated coverage proves at least:

- identical parent/seed/radius/index gives byte-identical canonical child genome;
- descendant identity is independent of lower-index generation and shuffled execution order;
- low-radius locality obeys descriptor semantics;
- high-radius descendants remain registry-valid and executable;
- inserted/substituted stable instance IDs repeat exactly;
- whole-operator and parameter locks are honoured, including topology anchors and fully locked no-op behaviour;
- changing the explicit mutation seed changes the population reproducibly;
- proxy thumbnails are renders of the same child genome later promoted canonically;
- obsolete tray-generation work cannot mutate a newer tray;
- tray generation remains separate from manual edit history.

Debug and Release CI both exercise the mutation/tray contracts and the native application smoke path.