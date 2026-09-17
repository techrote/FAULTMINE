# FAULTMINE

FAULTMINE is a standalone Windows glitch-art synthesis instrument built around **controlled, deterministic mistakes** rather than a conventional filter stack.

The program deliberately misinterprets image memory, addressing, representation, bitplanes, channels, timing, reconstruction and feedback. Every canonical result must remain reproducible from explicit source/genome/seed/versioned semantics.

The core interaction is evolutionary exploration: generate a tray of related glitch specimens, retain interesting results, lock desirable genes, mutate the rest, breed compatible genomes and progressively reduce mutation radius to excavate a visual neighbourhood.

## Product principles

- **Reproducibility is a contract.** A saved genome is evidence sufficient to reconstruct the same canonical result when combined with the explicit source and, for temporal work, explicit frame index.
- **Faults are simulated safely.** Artistic memory corruption must never rely on C/C++ undefined behaviour or unsafe host memory access.
- **The CPU reference engine is authoritative.** GPU rendering accelerates presentation, but does not silently change canonical output.
- **Standalone means standalone.** v1 targets Windows x64 using C++20, Win32, D3D11/DXGI and WIC, with no required package manager, web runtime or external application framework.
- **Exploration beats presets.** Seeds, mutation, locks, crossover and lineage are first-class concepts.
- **Projects stay inspectable.** State is stored in versioned human-readable files with explicit source/provenance metadata.
- **Fast path to usefulness.** The roadmap establishes a usable end-to-end still-image glitch workflow before expanding the fault catalogue.

## Development status

FM-001 established the native C++20/CMake/MSVC substrate, strict warning policy, Win32 shell and Debug/Release CI. FM-002 established versioned genomes, stable operator identities, named deterministic entropy, canonical JSON and pure-core SHA-256 identities. FM-003 established canonical straight-RGBA8 image semantics, content-based source identity, WIC still I/O, the serial canonical pipeline, starter deterministic faults, visual goldens and `FAULTMINE-render.exe`. FM-004 added D3D11/DXGI presentation, deterministic proxy preview, native canvas controls and semantic render scheduling outside `WM_PAINT`.

FM-005 added bounded memory/addressing faults; FM-006 added host-independent representation/bit faults; FM-007 added deterministic palettes, LUTs, quantisation, Bayer/noise dither and generated palettes. FM-008 turned those catalogues into a descriptor-driven native stack editor with mutation locks, undo/redo, typed parameter editing, project save/load and source-identity-checked relinking.

FM-009 added deterministic independently addressed mutation descendants, typed low/medium/high mutation radius, topology-aware lock protection, proxy specimen thumbnails, reroll/pin/compare controls and canonical full-source promotion.

FM-010 added the durable evolutionary layer: deterministic typed ordered-parent crossover, a durable acyclic specimen lineage graph, explicit mutation/crossover provenance, durable favourites, lineage navigation/provenance inspection, and `.fmproj` project schema v2 with conservative history-free migration from project v1.

FM-011 adds deterministic temporal work without making wall clock part of the artwork:

- canonical rendering accepts an explicit unsigned frame/tick and defines frame N by deterministic replay from the frame-0 initial state;
- feedback state is pipeline/model-owned and isolated per operator rather than stored in Win32/D3D/global presentation state;
- exact integer triangle/saw/square/ramp, sample-and-hold and frame-keyed modulators;
- feedback blend/displacement, partial refresh, trail accumulation, phase drift, tearing-phase and channel-phase fault operators;
- typed temporal parameters participate in the existing FM-009 mutation and FM-010 crossover/lineage machinery;
- optional `temporal.timeline-rate` positive rational metadata persists semantic frame rate in ordinary canonical genome/project state;
- `SessionModel` exposes exact seek/step/reset and full render-at-frame while keeping preview playback rate separate from semantic frame/rate;
- the native Timeline transport provides play/pause, exact stepping, reset/seek, preview-rate control and a visible current-frame/rate/play-state display.

FM-012 turns capture into a provenance-bearing deterministic output pipeline:

- full-resolution canonical still PNG export is isolated from proxy/canvas/tray presentation buffers;
- strict export-manifest schema v1 embeds the canonical genome and binds source/genome/seed/operator/output identity plus retained lineage derivation where available;
- PNG and manifest outputs are written to adjacent temporary files and only committed to final paths after successful completion, with explicit fail-if-exists or overwrite policy;
- deterministic contact sheets preserve explicit specimen ordering and map every presentation cell back to complete specimen genome identity/canonical genome/render identity;
- temporal image-sequence export uses explicit `[begin,end)` frames, stable zero-padded naming, semantic rational rate metadata and per-frame canonical pixel/frame identities;
- cancellation occurs between atomic frame writes, preserving completed files and optionally committing a truthful partial manifest;
- the native Export menu exposes provenance on/off, explicit overwrite, canonical stills, retained-exploration contact sheets and explicit frame-range sequences with progress/Escape cancellation.

The byte-level deterministic rules are documented in [`docs/RAG_DETERMINISM.md`](docs/RAG_DETERMINISM.md), canonical image/operator semantics in [`docs/RAG_IMAGE_PIPELINE.md`](docs/RAG_IMAGE_PIPELINE.md), session/presentation boundaries in [`docs/RAG_SESSION_PRESENTATION.md`](docs/RAG_SESSION_PRESENTATION.md), fault-family contracts in the corresponding RAG documents, manual editor/project foundations in [`docs/RAG_PROJECT_EDITOR.md`](docs/RAG_PROJECT_EDITOR.md), mutation search in [`docs/RAG_MUTATION_SEARCH.md`](docs/RAG_MUTATION_SEARCH.md), crossover/lineage/project-v2 semantics in [`docs/RAG_LINEAGE_CROSSOVER.md`](docs/RAG_LINEAGE_CROSSOVER.md), temporal semantics in [`docs/RAG_TEMPORAL.md`](docs/RAG_TEMPORAL.md), and export/provenance semantics in [`docs/RAG_EXPORT.md`](docs/RAG_EXPORT.md).

## Prerequisites

- 64-bit Windows 10/11 or a compatible supported Windows development environment;
- Visual Studio 2022 or 2026 Build Tools/IDE with **Desktop development with C++**, the MSVC x64 toolchain and a Windows SDK;
- CMake 3.25 or newer, and a CMake version new enough to support the installed Visual Studio generator (VS 2026 requires VS 18 generator support);
- PowerShell 7 or Windows PowerShell for the convenience scripts.

No third-party runtime or package manager is required.

## Configure, build, test and run

From PowerShell at the repository root:

```powershell
.\scripts\configure.ps1
.\scripts\build.ps1 -Configuration Debug
.\scripts\test.ps1 -Configuration Debug
.\scripts\run.ps1 -Configuration Debug
```

`configure.ps1` uses Visual Studio Installer's `vswhere.exe` to locate an MSVC x64 toolchain and currently selects either the Visual Studio 2026 or Visual Studio 2022 CMake generator.

Release uses the same configured build tree:

```powershell
.\scripts\build.ps1 -Configuration Release
.\scripts\test.ps1 -Configuration Release
.\scripts\run.ps1 -Configuration Release
```

Equivalent direct build/test commands after configuration are:

```powershell
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Build outputs are generated under `build/`; the desktop executable is `build/Debug/FAULTMINE.exe` or `build/Release/FAULTMINE.exe`.

## Interactive controls

The native right-side editor exposes the registered fault catalogue and selected operator parameters directly. Baseline editor/canvas shortcuts include `Ctrl+O` open image, `Ctrl+Shift+O` open project, `Ctrl+S`/`Ctrl+Shift+S` save/save-as, `Ctrl+E` full-resolution canonical PNG export, `Ctrl+Z`/`Ctrl+Y` manual undo/redo, `Ctrl+Insert` add operator, `Ctrl+D` duplicate operator, `Delete` remove operator, `Ctrl+Up`/`Ctrl+Down` select operator, `Alt+Up`/`Alt+Down` reorder, `X` bypass, `L` whole-operator mutation lock, `Ctrl+L` selected-parameter mutation lock, `[`/`]` numeric nudge (`Shift` for large), `Space` whole-stack enable/disable, `R` deterministic root-seed reroll, `F5` rerender, `F` fit, `1` 1:1 display, wheel zoom, drag/arrow pan, `B` before/after and `P` deterministic proxy-preview toggle.

The Explore panel adds deterministic mutation/search controls:

- `Ctrl+Alt+G` — generate a mutation tray from the active genome;
- `Ctrl+Alt+R` — reroll descendants with the next deterministic mutation seed;
- `Ctrl+Alt+Enter` — promote the selected mutation specimen with exact provenance;
- `Ctrl+Alt+P` — toggle the selected specimen's durable Favourite state;
- `Ctrl+Alt+Left` / `Ctrl+Alt+Right` — reduce/increase mutation radius;
- normal thumbnail click — compare a tray specimen on the main canvas;
- `Ctrl+click` thumbnails — assign/remove ordered crossover ranks `P1`, `P2`, ...;
- `Ctrl+Alt+B` — breed the selected 2–8 ordered parents using typed crossover v1;
- Lineage **Activate** — navigate to any retained specimen without treating navigation as undo/redo;
- **Provenance** — report derivation kind, source/genome identity, parent identities and mutation/crossover address data.

The Timeline menu adds temporal presentation controls. These alter only which explicit semantic frame is requested and how quickly playback requests successive frames; they do not add wall-clock state to canonical pixels:

- `Ctrl+Alt+Space` — play/pause;
- `Ctrl+Alt+,` / `Ctrl+Alt+.` — exact one-frame backward/forward step;
- `Ctrl+Alt+Home` — reset to frame 0;
- `Ctrl+Alt+PageUp` / `Ctrl+Alt+PageDown` — seek -10/+10 frames;
- `Ctrl+Alt+Down` / `Ctrl+Alt+Up` — halve/double preview playback rate.

The window title shows current semantic frame, rational semantic rate, preview multiplier and play/pause state. Add/edit `temporal.timeline-rate` in the normal stack editor when a project needs explicit semantic rate metadata other than the default presentation assumption of 30/1.

The Export menu adds the FM-012 capture surface. `Ctrl+E` now routes through the atomic canonical-still exporter. **Write provenance manifest** is enabled by default. **Allow explicit overwrite** is opt-in; otherwise an existing target is rejected. Contact-sheet export uses retained exploration specimens in durable creation order. Frame-sequence export prompts for inclusive begin and exclusive end frames, then a destination/naming stem; the status bar reports progress and Escape requests cancellation between completed files.

Mutation locks do **not** block deliberate manual edits. They protect search operations and remain outside canonical genome identity. A primary whole-operator lock is also a crossover topology anchor. Manual edit history and specimen lineage remain intentionally separate.

## Project files and source relinking

`.fmproj` schema v2 is strict human-readable JSON. It embeds the active canonical genome and records source provenance, active-genome mutation locks, durable specimen lineage/favourites, proxy/session state, selected operator and separately identified non-semantic canvas view state.

FM-011 does not introduce a parallel temporal project schema. Temporal fault parameters and optional rational semantic-rate metadata are ordinary canonical genome parameters and therefore survive existing project/lineage persistence. Current playback frame and preview-speed multiplier remain session/presentation state rather than specimen identity.

FM-012 likewise does not change project schema. Export manifests are output-side audit records, not mutable project state. A manifest embeds the active canonical genome and identities needed to audit/reproduce the exported result without making the export operation part of specimen identity.

Lineage records retain complete typed genomes plus canonical genome/source identities and explicit derivation metadata. Mutation nodes store parent identity, mutation-policy version, seed, descendant index and radius. Crossover nodes store ordered parent identities, crossover-policy version and crossover seed. Rendered thumbnail/full-resolution pixels are never lineage authority.

The v2 parser accepts historical project v1 and migrates it conservatively to exactly one `migrated-project` lineage root. The migration does not fabricate parents or historical seed/policy information. The next save writes schema v2. Unsupported future project versions remain rejected.

If the recorded source is moved, FAULTMINE accepts an explicitly relinked file only when its normalized content identity is identical. Missing sources and changed content are distinguished; changed content at the old path is never silently accepted as the same provenance.

FM-007 palettes/LUTs remain embedded canonical asset JSON inside the operator parameters that use them, so project persistence requires no second asset serialization system.

## Non-GUI canonical render hook

The narrow developer CLI remains available:

```powershell
.\build\Debug\FAULTMINE-render.exe input.png genome.json output.png
```

It validates the genome against the **default** registry, normalizes the input through WIC, executes the canonical CPU stack at frame 0, writes a PNG, and prints normalized source/output identities. Provenance-rich still/contact/sequence capture is owned by the FM-012 export service used by the native application; FM-014 will reuse that service rather than inventing a second batch-export truth.

## Authoritative development documentation

Start with [`docs/RAG_INDEX.md`](docs/RAG_INDEX.md). It indexes the product, architecture, deterministic contracts, image/pipeline contracts, session/presentation boundaries, fault-family contracts, mutation/search/crossover/lineage/temporal/export contracts, project semantics, roadmap, verification and autonomous-issue execution rules.

Implementation work is tracked as GitHub issues prefixed `FM-###`. Each implementation issue is intended to be executable autonomously: inspect current `main`, implement the stated scope, test it, reconcile documentation, open a PR, repair CI, merge after required checks pass, verify the merge landed on `main`, and only then close the issue when its acceptance criteria are satisfied.

## Planned stack

- C++20
- CMake + MSVC
- Win32 desktop shell and generic native stack/exploration/timeline/export editor
- pure-core deterministic genome/entropy/SHA-256 substrate
- canonical RGBA8 CPU fault pipeline with explicit temporal replay
- application/session/editor/project/lineage models and deterministic proxy preprocessing
- provenance-rich canonical still/contact/image-sequence export service
- D3D11 / DXGI presentation
- Windows Imaging Component (WIC) image I/O
- GitHub Actions on `windows-latest`

No third-party runtime dependency is part of the baseline design. Any future dependency must be justified against the standalone/distributable requirement and documented before adoption.
