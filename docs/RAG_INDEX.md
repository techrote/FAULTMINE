# FAULTMINE RAG index

This is the retrieval entry point for agents and maintainers. Read the documents below before implementation work; together they define the product and engineering contract.

## Authoritative documents

- [`../README.md`](../README.md) — project charter and top-level product principles.
- [`../AGENTS.md`](../AGENTS.md) — autonomous issue execution protocol and non-negotiable engineering invariants.
- [`RAG_PRODUCT.md`](RAG_PRODUCT.md) — product intent, workflows, terminology and non-goals.
- [`RAG_ARCHITECTURE.md`](RAG_ARCHITECTURE.md) — system decomposition, deterministic engine contract, data model and safety boundaries.
- [`RAG_DETERMINISM.md`](RAG_DETERMINISM.md) — accepted genome v1, seed/instance IDs, named entropy, canonical JSON and identity byte-level contract.
- [`RAG_IMAGE_PIPELINE.md`](RAG_IMAGE_PIPELINE.md) — canonical RGBA8/source identity, serial CPU pipeline, starter fault semantics, WIC I/O and visual golden contract.
- [`RAG_SESSION_PRESENTATION.md`](RAG_SESSION_PRESENTATION.md) — application/session model, deterministic proxy preview, D3D11 presentation, render scheduling and first native interaction contract.
- [`RAG_MEMORY_ADDRESSING.md`](RAG_MEMORY_ADDRESSING.md) — shared logical-address policies, FM-005 memory/addressing operators, bounded-work rules and mutation descriptor hints.
- [`RAG_REPRESENTATION_BITS.md`](RAG_REPRESENTATION_BITS.md) — FM-006 channel/word/packed/planar/signed/bit semantics, structured bit bursts and host-independent representation rules.
- [`RAG_COLOUR.md`](RAG_COLOUR.md) — FM-007 palette/LUT assets, colour mapping, integer quantisation/dither, seeded palette generation and colour mutation domains.
- [`RAG_PROJECT_EDITOR.md`](RAG_PROJECT_EDITOR.md) — FM-008 generic stack editor, mutation locks, edit history, project persistence/source relink contract; project schema v2 is extended by FM-010.
- [`RAG_MUTATION_SEARCH.md`](RAG_MUTATION_SEARCH.md) — FM-009 independently addressed descendants, typed mutation radius, topology/lock semantics, specimen tray and promotion contract.
- [`RAG_LINEAGE_CROSSOVER.md`](RAG_LINEAGE_CROSSOVER.md) — FM-010 typed multi-parent crossover, durable specimen DAG, favourites, provenance UI and explicit project-v1 → project-v2 migration.
- [`RAG_TEMPORAL.md`](RAG_TEMPORAL.md) — FM-011 explicit frame/tick semantics, model-owned feedback state, integer modulators, temporal faults, timeline-rate metadata and native transport controls.
- [`RAG_EXPORT.md`](RAG_EXPORT.md) — FM-012 canonical still/contact/sequence export, strict provenance manifests, atomic collision policy and cancellation/partial-output semantics.
- [`RAG_EXTERNAL_DECODERS.md`](RAG_EXTERNAL_DECODERS.md) — external-decoder isolation, non-canonical decoder behaviour, freeze/materialize rules and laboratory provenance.
- [`RAG_ROADMAP.md`](RAG_ROADMAP.md) — initial plan, review findings, improved dependency-ordered implementation plan and milestones.
- [`RAG_VERIFICATION.md`](RAG_VERIFICATION.md) — deterministic testing, CI, performance and release verification strategy.
- [`RAG_ISSUE_PROTOCOL.md`](RAG_ISSUE_PROTOCOL.md) — standard structure and completion protocol used by every `FM-###` GitHub implementation issue.

## Authority and conflict resolution

1. Current accepted code/tests on `main` define implemented reality.
2. Explicit contracts in `AGENTS.md` and these RAG documents define intended architecture.
3. A specific issue defines the scope of its change.
4. If an issue becomes stale relative to accepted prerequisite work, preserve intent and acceptance criteria while adapting to current `main`; document any material reconciliation in the PR.
5. Do not silently resolve genuine contract conflicts. Reconcile documentation in the same PR or create a focused follow-up issue where appropriate.

## Core vocabulary

- **source** — normalized canonical input pixels/data plus identity metadata; path is convenience metadata only.
- **canonical RGBA8** — FM-003 still-image representation: straight `R,G,B,A` bytes, tightly packed at `width * 4` bytes per row.
- **source identity** — SHA-256 identity of the normalized canonical representation defined in `RAG_IMAGE_PIPELINE.md`, independent of pathname.
- **fault operator** — a safe deterministic transformation that deliberately models a representational, addressing, bit, signal, reconstruction or state error.
- **logical address** — simulated signed/unsigned address or coordinate resolved against a bounded canonical extent before any host-memory access.
- **boundary policy** — canonical `wrap`, `clamp`, or `fill` resolution defined in `RAG_MEMORY_ADDRESSING.md`.
- **logical word** — an explicitly assembled sequence of bytes/bits with documented order; never a native type-punned host load.
- **packed reinterpretation** — pack canonical channels into an explicit logical bit layout/byte order, then decode those same logical bytes under another explicit layout/order.
- **palette asset** — an ordered, versioned list of canonical RGBA8 colours with content identity defined in `RAG_COLOUR.md`.
- **LUT asset** — four explicit 256-entry byte lookup tables for canonical RGBA channels, with versioned canonical serialization and content identity.
- **fault stack / pipeline** — ordered operator graph for a specimen; v1 begins as a serial stack but serialization must permit future typed expansion.
- **genome** — complete serializable description of pipeline topology and parameters, excluding incidental UI state. FM-011 temporal operator parameters and optional rational timeline-rate metadata remain ordinary genome state.
- **project** — versioned human-readable continuation state containing source provenance, active genome, mutation locks, durable specimen lineage/favourites and clearly separated session/UI state. FM-010 project schema v2 is defined by `RAG_LINEAGE_CROSSOVER.md` and migrates schema v1 explicitly.
- **edit history** — undo/redo snapshots of deliberate manual genome/lock edits, distinct from specimen lineage.
- **seed** — explicit 64-bit root deterministic entropy used to derive named random streams under `RAG_DETERMINISM.md`.
- **operator instance ID** — stable persisted 128-bit identity independent of list position.
- **mutation descriptor** — non-persisted typed operator/parameter metadata used by deterministic search; it never changes canonical render identity by itself.
- **session** — non-canonical application state connecting one normalized source, a genome, preview cache, proxy state, explicit current temporal frame, view state and durable project lineage state.
- **proxy preview** — deterministic nearest-neighbour preprocessing used only for interactive responsiveness; visibly non-canonical as a final-resolution result and never substituted for export.
- **specimen** — retained/searchable canonical genome for a particular source plus explicit derivation metadata; rendered pixels are disposable products, not specimen lineage authority.
- **gene** — mutable unit of genome state used by exploration controls.
- **lock** — mutation/crossover protection applied to genes/operators; locks are project state and do not block deliberate manual editing. Under FM-009 a whole-operator lock is a topology anchor during mutation; FM-010 preserves primary-parent anchors during crossover.
- **mutation radius** — categorical typed mutation locality/topology policy (`low`, `medium`, `high`) defined by `RAG_MUTATION_SEARCH.md`, not a generic scalar multiplier.
- **lineage** — durable acyclic parent/child/crossover provenance among retained canonical specimens, distinct from editor undo/redo; defined in `RAG_LINEAGE_CROSSOVER.md`.
- **favourite** — durable non-semantic retained-specimen flag persisted by project schema v2; tray pins are the interactive presentation of that state.
- **crossover** — deterministic typed multi-parent recombination under policy v1. Parent selection order is semantic; the first parent is the primary topology scaffold.
- **semantic frame/tick** — explicit unsigned frame index supplied to canonical temporal rendering; it is never inferred from wall clock, paint rate or display refresh.
- **temporal state** — pipeline-owned previous operator state used by feedback faults and deterministically rebuilt from the defined initial state during canonical replay.
- **semantic timeline rate** — optional positive rational `rate_num/rate_den` stored by `temporal.timeline-rate`; it is provenance/export metadata and does not choose frame N.
- **preview playback rate** — presentation-only multiplier controlling how often the native UI requests a next explicit frame; it is not canonical render input.
- **canonical output** — result of the authoritative deterministic CPU engine for explicit source/genome/version and, for temporal work, explicit frame index.
- **export manifest** — strict versioned FM-012 JSON sidecar binding an export to normalized source identity, embedded canonical genome/identity, seed, operator versions, output dimensions/format and applicable lineage/temporal provenance.
- **contact sheet** — deterministic presentation artefact made from ordered canonical specimen renders and explicitly marked non-canonical as a final specimen image; its manifest maps cells back to canonical specimen identities/genomes.
- **frame sequence** — dependency-free canonical animation baseline: atomically written PNGs over an explicit `[begin,end)` semantic-frame range with deterministic filenames and per-frame identities.
- **preview** — interactive presentation; it must not silently redefine canonical semantics.
- **materialized laboratory source** — validated normalized pixels retained from an external-decoder experiment so downstream canonical work does not depend on rerunning decoder-specific recovery behaviour.

## v1 target

A compact Windows x64 application capable of opening an image, constructing and editing a deterministic fault stack, exploring seeded descendants in a specimen tray, locking/mutating/breeding results, retaining favourites and navigating durable lineage, driving explicit deterministic temporal faults/feedback, saving projects/genomes, and exporting provenance-rich reproducible stills, deterministic contact sheets and canonical temporal image sequences. Advanced codec/binary corruption and batch mining are later v1 phases but are architected from the start as bounded extensions rather than unsafe shortcuts.
