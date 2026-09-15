# FAULTMINE reviewed implementation roadmap

## Initial plan

The first concept divided development into these broad phases:

1. deterministic core;
2. fault primitives;
3. specimen explorer;
4. genetic operations;
5. instrument-quality controls;
6. dynamic/temporal glitches;
7. codec/raw-data corruption laboratory;
8. capture and batch mining;
9. unusual simulated-hardware and recursive faults.

That ordering correctly identified determinism and evolutionary search as the differentiators, but it was too feature-sequential. It risked spending substantial time on an invisible engine and operator catalogue before delivering a genuinely usable application.

## Review findings

### 1. Reproducibility must be architectural, not bolted on

A deterministic seed alone is insufficient. Reproducibility can still be destroyed by implementation-defined random distributions, mutable global RNG consumption order, floating-point differences, unordered serialization, GPU-specific execution or accidental dependence on thread scheduling.

**Improvement:** specify canonical serialization, named random streams, CPU reference semantics, explicit numeric rules and content identity before adding many operators.

### 2. Deliver an end-to-end vertical slice early

The original plan separated engine work, primitives and UI too sharply.

**Improvement:** after bootstrap and deterministic contracts, build a minimal path that can load an image, apply a small useful starter operator set, show the canonical result and export/save it. This becomes the integration substrate for later work.

### 3. Simulated memory corruption must never use real memory unsafety

Glitch software can be tempted to rely on invalid reads or accidental decoder behaviour.

**Improvement:** model a bounded virtual address space with explicit wrap/clamp/drop/fill semantics. Put experiments with genuinely malformed external codec data in a sacrificial worker process.

### 4. GPU preview must not become an accidental second truth

Implementing faults directly as shaders would be fast, but vendor/compiler differences and approximate math could undermine provenance.

**Improvement:** make the CPU engine authoritative. D3D11 first presents CPU results. Later GPU acceleration either proves canonical equivalence per operator or remains explicitly preview-only.

### 5. Random exploration needs stable local entropy

One global PRNG makes descendants fragile: adding one random call to operator A changes unrelated operator B and every later specimen.

**Improvement:** derive named streams from root seed + stable operator/gene/purpose/coordinate identity. Descendant N remains independent of whether descendants 0..N-1 were rendered.

### 6. The project format and lineage model need to precede crossover

Without stable operator instance IDs and versioned persistence, locks, breeding and ancestry become ambiguous.

**Improvement:** establish schema, IDs, migrations and session/project distinctions before advanced evolutionary features.

### 7. Performance should use deterministic proxies, not semantic shortcuts

A tray of many full-resolution specimens can become expensive.

**Improvement:** support explicit deterministic proxy sources for exploration while preserving full-resolution canonical render for active/export results. Proxy status must be visible.

### 8. Fault discovery and user control should be data-driven

Hard-coding bespoke UI for every operator would slow catalogue growth and make mutation logic inconsistent.

**Improvement:** operators expose typed parameter descriptors, validation and mutation metadata. UI and evolutionary tooling consume the same descriptors.

### 9. Risky/large features belong after the core artistic loop is good

Codec fuzzing, arbitrary binary interpretation and overnight mining are compelling but not required to validate the central interaction.

**Improvement:** delay them until deterministic stack editing, specimen mutation, locks, breeding and export are solid.

## Improved dependency-ordered plan

The implementation is split into `FM-###` issues. Issue numbers are sequential, but dependencies—not numbering alone—define execution order.

### FM-001 — Repository/bootstrap, build, CI and Win32 shell

Establish CMake/MSVC, warning policy, directory boundaries, core/app targets, a minimal native Win32 executable, unit-test harness, GitHub Actions and documented build/run commands.

**Exit condition:** fresh checkout builds/tests in Debug and Release on CI; executable launches a minimal native window; no third-party runtime is required.

### FM-002 — Deterministic substrate: genome, seed streams, canonical serialization and identities

Implement foundational core types, strict versioned genome representation, stable operator instance IDs, explicit deterministic integer PRNG/stream derivation, canonical serialization, content/genome hashing and round-trip/golden tests.

**Dependencies:** FM-001.

**Exit condition:** identical fixtures produce identical canonical serialization/random vectors/hash identities across repeated CI runs, with no UI or GPU requirement.

### FM-003 — WIC source normalization and first canonical fault pipeline vertical slice

Add WIC image loading/normalization, checked image buffers, a serial deterministic CPU pipeline, a small starter fault set spanning addressing/bit/channel/scan concepts, still export, and CLI/test hooks sufficient to prove source + genome + seed -> output.

**Dependencies:** FM-002.

**Exit condition:** a source can be loaded, glitched deterministically, saved/reloaded by genome and exported with byte-stable test fixtures.

### FM-004 — D3D11 interactive preview and application session shell

Add D3D11/DXGI presentation of canonical CPU results, file-open workflow, canvas zoom/pan/fit, before/after, proxy-state infrastructure, status/error surfaces and basic session orchestration. Do not duplicate canonical fault semantics in shaders.

**Dependencies:** FM-003.

**Exit condition:** a user can open an image and interactively view deterministic starter faults in the native application.

### FM-005 — Memory/addressing fault family

Implement the first serious fault catalogue: logical stride mismatch, offsets, address transforms, block/tile remap, wrap/clamp/drop/fill policies, line/region duplication/skip and selected Z/Morton-style addressing. Add malformed/overflow tests proving no actual out-of-bounds host access.

**Dependencies:** FM-003; integrates with FM-004 UI if already merged.

### FM-006 — Representation and bit fault family

Implement channel/byte order reinterpretation, packed logical formats where justified, planar/interleaved concepts, signed/unsigned transformations, bit masks/XOR/shifts/rotates, nibble and bitplane operations, deterministic structured burst faults.

**Dependencies:** FM-003.

### FM-007 — Colour/LUT/quantisation/dither subsystem

Implement deterministic palette/LUT mapping, palette import/generation where in-scope, quantisation and deterministic dithering. Define LUT file/project representation and mutation descriptors.

**Dependencies:** FM-003.

### FM-008 — Generic fault-stack editor, parameter controls, locks and edit history

Build data-driven UI from operator descriptors. Support add/remove/duplicate/reorder/bypass, typed parameter editing, exact numeric entry/nudging, per-gene/operator locks, undo/redo and project/session save/load.

**Dependencies:** FM-004 and enough operators from FM-005/006/007 to exercise genericity.

**Exit condition:** FAULTMINE is independently useful as a deterministic manual glitch instrument.

### FM-009 — Specimen tray and deterministic mutation search

Implement population generation, thumbnails, promote/select/pin, deterministic descendant seeds, mutation radius, mutation of unlocked genes and deterministic proxy rendering. Rendering order/parallel scheduling must not alter descendant genomes or pixels.

**Dependencies:** FM-008.

**Exit condition:** the core “give me more mistakes like this” loop works for mutation without crossover.

### FM-010 — Crossover, lineage graph, favourites and project provenance

Add multi-parent selection, conservative typed crossover, explicit lineage records, favourites/pins independent of undo, project persistence/migration and UI navigation for ancestors/descendants.

**Dependencies:** FM-009.

**Exit condition:** selected specimens can be bred, ancestry survives save/reload and any specimen can report how it was derived.

### FM-011 — Deterministic temporal engine, feedback and modulation

Add explicit integer-frame/tick execution, model-owned previous-frame buffers, feedback operators, envelopes/LFOs/seeded modulators and timeline playback. Keep time semantic state independent of wall clock/display refresh.

**Dependencies:** FM-008; mutation integration should work with FM-009/010 if merged.

### FM-012 — Export, manifests, contact sheets and reproducible capture

Expand export to provenance manifests, contact sheets, image sequences for temporal work, deterministic naming and robust overwrite/error handling. Ensure exports default to canonical full-resolution execution rather than proxy preview.

**Dependencies:** FM-008; temporal sequence support additionally depends on FM-011.

### FM-013 — Isolated codec and arbitrary-binary corruption laboratory

Add a worker executable and validated IPC. Support bounded experiments that mutate encoded inputs or interpret arbitrary binary data without risking editor stability. Enforce time/memory/process limits and treat crash/timeout as structured outcomes.

**Dependencies:** FM-003 architecture; preferably after FM-012 so outputs/provenance already exist.

### FM-014 — Headless batch miner, descriptors and novelty clustering

Expose the deterministic core through a headless executable. Generate large descendant sets, calculate simple deterministic visual descriptors, remove near-duplicates, cluster/rank by novelty/diversity (not subjective quality), write manifests and contact sheets, and permit later loading of mined specimens in the GUI.

**Dependencies:** FM-009 and FM-012.

### FM-015 — v1 hardening, performance, packaging and release readiness

Profile real workloads; add deterministic caching/proxy improvements where needed; harden parsing/allocation/error paths; verify worker containment; build a clean portable Windows x64 archive; add version/about metadata; reconcile documentation; run full regression/golden suites; establish release checklist.

**Dependencies:** all intended v1 features.

## Milestones

### Milestone A — Deterministic vertical slice

FM-001 through FM-004.

Outcome: open -> deterministic fault -> preview -> save/export works end to end.

### Milestone B — Useful manual instrument

FM-005 through FM-008.

Outcome: substantial fault vocabulary plus a generic editable stack and persistent projects.

### Milestone C — FAULTMINE's distinctive search loop

FM-009 and FM-010.

Outcome: mutation, locks, breeding and lineage make the application materially different from ordinary glitch-filter tools.

### Milestone D — Dynamic work and provenance-rich export

FM-011 and FM-012.

Outcome: deterministic animated glitches and robust capture/manifests.

### Milestone E — Laboratory/mining extensions

FM-013 and FM-014.

Outcome: bounded malformed-codec/raw-data experiments and large-scale headless exploration.

### Milestone F — v1 release

FM-015.

## Dependency graph

```text
FM-001
  -> FM-002
      -> FM-003
          -> FM-004
          -> FM-005
          -> FM-006
          -> FM-007
              \   |   /
               -> FM-008
                    -> FM-009
                        -> FM-010
                    -> FM-011
                    -> FM-012
                         -> FM-014 (also needs FM-009)
          -> FM-013 (prefer after FM-012)

all intended v1 work -> FM-015
```

## Scope guidance

Issues should be implemented serially when they modify the same foundational contracts. FM-005, FM-006 and FM-007 may be parallelised after FM-003 if coordination preserves the shared operator registry contract. FM-011 and FM-012 can partially overlap after FM-008, but temporal export is only complete once both are merged.

The roadmap intentionally avoids a numerical operator-count target as a proxy for quality. Each fault-family issue should implement a coherent useful baseline with tests and documented semantics rather than padding the catalogue with superficial variants.
