# FAULTMINE generic editor and project contract

This document records the FM-008 manual-editor, lock, history and project-persistence contracts. It is authoritative until an explicit later version supersedes a rule.

## Layering

FM-008 keeps three state classes distinct:

1. **canonical genome** — operator topology, stable instance IDs, typed parameters, enabled state and root seed; this alone determines canonical still rendering together with the normalized source;
2. **project/exploration state** — mutation locks, source reference/provenance and editor/session state;
3. **non-semantic UI state** — selected operator, canvas view and proxy preference.

Locks, selection and view state never participate in genome identity or canonical pixels.

## Generic editor model

`EditorModel` owns the full default fault registry, active genome, mutation locks and manual edit history. The native UI consumes `OperatorDescriptor` / `ParameterDescriptor` metadata rather than reimplementing operator semantics.

The registry now exposes its ordered descriptor collection for UI/add-operator enumeration. This is metadata access only; canonical execution still uses the same `FaultRegistry` executors.

Supported manual operations are:

- add a registered operator;
- remove;
- duplicate;
- reorder;
- enable/bypass;
- exact typed parameter entry;
- numeric small/large nudging;
- operator mutation lock;
- parameter/gene mutation lock;
- undo/redo;
- deterministic root-seed reroll.

### Stable IDs for manual topology edits

Manual add/duplicate never uses wall-clock or process randomness.

- add derives a candidate stable ID from root seed + a type-derived stable parent ID + purpose `manual-add-operator` + the first collision-free ordinal;
- duplicate derives from the source instance ID + purpose `manual-duplicate-operator` + the first collision-free ordinal.

Once created, IDs are ordinary persisted genome data. Undo/redo restores exact snapshots, so redo never regenerates an ID. Project reload likewise restores the persisted ID exactly.

## Descriptor-driven parameter editing

Exact editor input uses the existing genome primitive kinds:

- boolean: literal `true` / `false`;
- signed integer: decimal or `0x` hexadecimal, with optional sign;
- unsigned integer: decimal or `0x` hexadecimal;
- text: UTF-8 text.

Mutation metadata also supplies editor validation hints:

- signed/unsigned ranges and steps;
- choice lists;
- bitmask numeric entry;
- `colour_rgba` canonical colour literals;
- `palette` strict FM-007 palette JSON;
- `lut` strict FM-007 LUT JSON.

Palette/LUT edits are parsed through the FM-007 asset parser and stored in canonical asset text. This does not add a second asset format.

Executor validation remains authoritative for cross-parameter semantic constraints that cannot be expressed by a single parameter descriptor. A render error is surfaced; state is never silently coerced into another artistic meaning.

## Mutation locks

Locks protect future FM-009 mutation, not deliberate manual edits.

Two lock forms exist:

- whole operator lock by stable instance ID;
- individual parameter lock by stable instance ID + parameter name.

Locks are stored outside `Genome`. Adding/removing a lock therefore leaves canonical genome SHA-256 and rendered pixels unchanged.

Removing an operator removes its associated project locks. Reordering and duplicating do not transfer locks implicitly to the new duplicate.

## Manual edit history

History snapshots contain:

- complete canonical genome;
- complete lock state.

Source changes and project loads are explicit history boundaries. View changes are not semantic edit-history entries.

Repeated numeric nudges may be coalesced while one keyboard gesture is active; `end_coalesced_edit()` terminates that group. Structural edits and explicit Apply operations each create their own boundary.

Edit history is intentionally separate from FM-009/010 specimen lineage.

## Project v1

File extension: `.fmproj`

Project schema version: `1`

Canonical project JSON uses these top-level fields in order:

```text
project_version
source
genome
locks
session
ui
```

The canonical text ends with one LF.

### `source`

```json
{"path":"C:/art/source.png","identity":"<64 hex SHA-256>"}
```

Path is convenience/relink metadata. Identity is the normalized FM-003 source identity and is authoritative provenance.

### `genome`

The normal canonical genome object is embedded directly, not quoted as a secondary string format. Genome schema/engine/operator versions remain governed by `RAG_DETERMINISM.md`.

### `locks`

Lock records are canonicalized by stable instance-ID order:

```json
{"instance_id":"<32 hex>","operator":true,"parameters":["amount","boundary"]}
```

Parameter names are sorted. The parser rejects duplicate lock records, duplicate parameter locks, missing instances and undeclared parameter names.

### `session`

Project v1 persists:

- proxy enabled flag;
- proxy max width/height;
- proxy method version;
- selected operator instance ID (or empty string).

Unsupported proxy method versions fail rather than changing preview semantics silently.

### `ui`

Non-semantic canvas state is separate:

- `view_mode`: `fit`, `actual`, or `custom`;
- `zoom_milli`;
- `pan_x_milli`, `pan_y_milli`;
- before/result selection.

Scaled integers avoid making project parsing dependent on floating-point text formatting. UI values never enter genome identity.

### FM-007 assets

FM-007 palettes/LUTs remain embedded canonical JSON text in the operator parameters that use them. Project v1 preserves them through the embedded genome; it does not introduce path-referenced colour assets before such a reference model is explicitly versioned.

## Project parse and migration baseline

Project parsing uses the existing strict JSON parser:

- duplicate keys rejected;
- unknown schema-owned fields rejected;
- required fields checked;
- project version other than `1` rejected;
- embedded genome validated against the current default registry;
- source identity must be 64 hexadecimal digits;
- lock targets validated;
- selected instance validated;
- proxy/view ranges validated.

Version 1 is the migration baseline. There is no historical project schema to migrate yet. Future schema support must add an explicit version transition rather than partially interpreting unknown future state.

## Source open/relink contract

When a project opens:

1. try the recorded path;
2. normalize the candidate with the ordinary WIC source path;
3. compare normalized identity with the project identity;
4. if missing/unreadable/mismatched, explicitly offer relink;
5. accept a replacement only when its normalized identity is identical.

Therefore:

- moved identical-content source: accepted;
- missing source: distinguished and requires relink;
- changed content at the old path: rejected as the same provenance;
- user-cancelled relink: previous valid application session remains intact.

## Native instrument surface

FM-008 adds a compact native right-side editor containing:

- registered operator picker + Add;
- stack list with enabled and mutation-lock indicators;
- Remove, Duplicate, Up, Down, Bypass and operator-lock buttons;
- generic parameter selector;
- exact edit field or descriptor-backed boolean/choice dropdown;
- Apply and numeric +/- controls;
- parameter-lock button;
- palette/LUT asset load button when appropriate.

No heavyweight UI framework is introduced.

## Baseline shortcuts

- `Ctrl+O` — open image;
- `Ctrl+Shift+O` — open project;
- `Ctrl+S` / `Ctrl+Shift+S` — save / save-as project;
- `Ctrl+Z` / `Ctrl+Y` — undo / redo;
- `Ctrl+Insert` — add selected registered type;
- `Ctrl+D` — duplicate selected operator;
- `Delete` — remove selected operator;
- `Ctrl+Up/Down` — select previous/next operator;
- `Alt+Up/Down` — reorder selected operator;
- `X` — bypass selected operator;
- `L` — toggle selected operator mutation lock;
- `Ctrl+L` — toggle selected parameter mutation lock;
- `[` / `]` — small selected numeric-parameter nudge;
- `Shift+[` / `Shift+]` — large nudge;
- `Space` — enable/disable the whole active stack;
- `R` — deterministic root-seed reroll;
- `F5` — rerender;
- existing `F`, `1`, `B`, `P`, mouse wheel and canvas pan controls remain presentation-only.

Bindings may evolve later, but their semantics must retain the separation between manual edit history, mutation locks and canonical rendering.

## Verification

`editor_project_contracts` covers:

- descriptor enumeration and generic add/edit across FM-005/006/007;
- exact numeric entry and descriptor validation;
- duplicate/reorder stable IDs;
- coalesced nudge undo;
- mutation locks excluded from canonical identity/output;
- manual edits allowed despite mutation locks;
- exact project canonical round-trip;
- future project-version rejection;
- source missing/identical/changed classification;
- embedded FM-007 palette persistence;
- project reload reproducing canonical output and lock state;
- moved identical source relink and changed-source rejection.

All earlier deterministic suites and the real D3D11 smoke remain required in Debug and Release.
