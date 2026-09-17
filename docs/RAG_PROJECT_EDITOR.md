# FAULTMINE generic editor and project contract

This document records the FM-008 manual-editor/lock/history contract and its FM-010 project-v2 evolution. `RAG_LINEAGE_CROSSOVER.md` is authoritative for crossover, favourites and specimen-lineage semantics.

## Layering

FAULTMINE keeps these state classes distinct:

1. **canonical genome** — topology, stable instance IDs, typed parameters, enabled state and root seed; with normalized source this determines canonical pixels;
2. **project/evolution state** — source provenance, mutation locks, retained specimen lineage and favourites;
3. **manual edit history** — deliberate genome/lock undo/redo snapshots;
4. **non-semantic UI/session state** — selected operator, canvas view and proxy preference.

Locks, lineage, favourites, selection and view state never participate in canonical genome identity or canonical pixels by themselves.

## Generic editor model

`EditorModel` owns the full default registry, active genome, mutation locks and manual edit history. The native UI consumes `OperatorDescriptor` / `ParameterDescriptor` metadata rather than reimplementing operator semantics.

Supported manual operations include add/remove/duplicate/reorder, enable/bypass, exact typed parameter editing, numeric nudges, whole-operator and per-parameter mutation locks, undo/redo, and deterministic root-seed reroll.

Manual add/duplicate IDs are derived deterministically from persisted root/instance/type context. Undo/redo restores exact snapshots rather than regenerating identities.

Exact parameter entry uses the genome primitive kinds and descriptor mutation metadata for range/choice/bitmask/colour/palette/LUT validation. Executor validation remains authoritative for cross-parameter constraints.

## Mutation locks

Locks protect mutation/crossover search, not deliberate manual editing. They live outside `Genome`, so toggling a lock does not alter canonical genome SHA-256 or rendered pixels.

Removing an operator removes its associated locks. Reorder preserves lock identity. Duplicating an operator does not implicitly copy protection to the new instance.

FM-009 additionally treats a whole-operator lock as a topology anchor during mutation. FM-010 crossover follows the protection rules in `RAG_LINEAGE_CROSSOVER.md`.

## Manual edit history

History snapshots contain the complete canonical genome and complete lock state. Source changes, project loads and lineage activation/promotion are explicit history boundaries. View changes are not history entries.

Repeated numeric nudges may be coalesced while one gesture is active. Structural edits and explicit Apply operations form their own boundaries.

Manual undo/redo is intentionally not specimen ancestry navigation.

## Project schema history

File extension: `.fmproj`.

### Project v1 — FM-008 migration baseline

Schema version 1 established strict human-readable project JSON with top-level fields:

```text
project_version
source
genome
locks
session
ui
```

It persisted source path/normalized identity, active genome, locks, proxy/session state, selected operator and separately identified canvas UI state. It contained no specimen-lineage evidence.

Version 1 remains accepted as an explicit migration input under FM-010; it is no longer the current save format.

### Project v2 — FM-010 current format

Schema version 2 adds durable evolutionary state. Canonical top-level order is:

```text
project_version
source
genome
locks
lineage
session
ui
```

The canonical text ends with one LF. The active top-level genome/locks must exactly match `lineage.active_specimen_id`. Detailed lineage representation and v1 migration semantics are defined in `RAG_LINEAGE_CROSSOVER.md`.

Unknown future project versions remain rejected rather than partially interpreted.

## Source

```json
{"path":"C:/art/source.png","identity":"<64 hex SHA-256>"}
```

Path is convenience/relink metadata. The normalized FM-003 source identity is authoritative provenance.

Project load tries the recorded path, normalizes through the ordinary WIC source path, compares content identity, and requires explicit relink when missing/unreadable/mismatched. A replacement is accepted only when normalized identity is identical.

Therefore moved identical content is accepted; missing and changed content are distinguished; changed same-path content is never silently accepted as the same source.

## Genome and locks

The normal canonical genome object is embedded directly. Genome/operator versions remain governed by `RAG_DETERMINISM.md`.

Lock records are canonicalized by stable instance ID and contain whole-operator protection plus sorted parameter names. Duplicate records/parameters, missing instances and undeclared parameters are rejected.

FM-007 palette/LUT assets remain embedded canonical asset JSON inside operator parameters; project v2 does not add an independent colour-asset reference system.

## Session and UI

Session state persists deterministic proxy preference/specification and selected operator stable ID. Unsupported proxy method versions fail clearly.

UI state persists `fit`/`actual`/`custom` view mode, scaled integer zoom/pan and before/result selection. These values never enter genome identity.

## Project parsing

The strict existing JSON parser remains authoritative:

- duplicate keys rejected;
- schema-owned unknown fields rejected;
- required fields/type/ranges checked;
- embedded genomes validated against the registry;
- source identity validated;
- lock targets validated;
- selected operator validated;
- proxy/view values validated;
- project v2 lineage/DAG/source binding validated;
- v1 migrated explicitly to one `legacy-project-root` node;
- unsupported future versions rejected.

## Native editor baseline

The FM-008 right-side editor remains descriptor-driven and exposes operator add/remove/duplicate/reorder/bypass, lock controls, generic parameter editing, numeric nudges and palette/LUT loading without introducing a heavyweight UI framework.

FM-009/010 add the Explore surface without changing the manual editor's semantic role. Evolutionary promotion and lineage navigation refresh the editor around the selected retained genome but do not manufacture undo entries representing ancestry.

## Verification

`editor_project_contracts` continues to cover generic descriptor editing, stable IDs/history, non-canonical locks, canonical project round-trip, future-version rejection, source classification/relink, palette persistence and restored output/locks/selection.

`crossover_lineage_project_contracts` adds project-v2 lineage/favourite persistence, v1 migration, DAG validation, crossover provenance and lineage navigation. All earlier deterministic suites plus Debug/Release real D3D11/native smoke remain mandatory.
