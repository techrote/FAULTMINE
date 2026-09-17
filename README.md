# FAULTMINE

FAULTMINE is a standalone Windows glitch-art synthesis instrument built around **controlled, deterministic mistakes** rather than a conventional filter stack.

The program deliberately misinterprets image memory, addressing, representation, bitplanes, channels, timing, reconstruction and feedback. Every result must remain reproducible from its source identity, genome, seed and engine version.

The core interaction is evolutionary exploration: generate a tray of related glitch specimens, select interesting results, lock desirable genes, mutate the rest, breed compatible genomes and progressively reduce mutation radius to excavate a visual neighbourhood.

## Product principles

- **Reproducibility is a contract.** A saved genome is evidence sufficient to reconstruct the same canonical result.
- **Faults are simulated safely.** Artistic memory corruption must never rely on C/C++ undefined behaviour or unsafe host memory access.
- **The CPU reference engine is authoritative.** GPU rendering accelerates presentation, but does not silently change canonical output.
- **Standalone means standalone.** v1 targets Windows x64 using C++20, Win32, D3D11/DXGI and WIC, with no required package manager, web runtime or external application framework.
- **Exploration beats presets.** Seeds, mutation, locks, crossover and lineage are first-class concepts.
- **Projects stay inspectable.** State is stored in versioned human-readable files with explicit source/provenance metadata.
- **Fast path to usefulness.** The roadmap establishes a usable end-to-end still-image glitch workflow before expanding the fault catalogue.

## Development status

FM-001 established the native C++20/CMake/MSVC substrate, strict warning policy, Win32 shell and Debug/Release CI.

FM-002 established versioned genomes, stable operator identities, named deterministic entropy, canonical JSON and pure-core SHA-256 identities.

FM-003 established the canonical straight-RGBA8 visual CPU path, content-based source identity, WIC still I/O, the serial canonical pipeline, starter deterministic faults, visual goldens and `FAULTMINE-render.exe`.

FM-004 established D3D11/DXGI presentation, WIC open/export, deterministic proxy preview, native canvas controls and semantic render scheduling outside `WM_PAINT`.

FM-005 adds the bounded memory/addressing family; FM-006 adds host-independent representation/bit faults; FM-007 adds deterministic palettes, LUTs, quantisation, Bayer/noise dither and generated palettes.

FM-008 turns those catalogues into a practical native manual instrument:

- a descriptor-driven right-side stack editor over the full default registry;
- add/remove/duplicate/reorder/bypass for registered operators;
- generic exact typed parameter editing, descriptor choices, numeric nudges and palette/LUT asset loading;
- separate whole-operator and per-parameter mutation locks;
- snapshot-based undo/redo with coalesced keyboard nudges and stable instance IDs;
- versioned `.fmproj` project save/load containing source provenance, genome, locks, proxy/session state and separately identified UI view state;
- explicit missing/moved/changed source handling with identity-checked relinking;
- keyboard-first stack navigation/edit controls.

The byte-level deterministic rules are documented in [`docs/RAG_DETERMINISM.md`](docs/RAG_DETERMINISM.md), canonical image/operator semantics in [`docs/RAG_IMAGE_PIPELINE.md`](docs/RAG_IMAGE_PIPELINE.md), interactive session/presentation boundaries in [`docs/RAG_SESSION_PRESENTATION.md`](docs/RAG_SESSION_PRESENTATION.md), memory/addressing semantics in [`docs/RAG_MEMORY_ADDRESSING.md`](docs/RAG_MEMORY_ADDRESSING.md), representation/bit semantics in [`docs/RAG_REPRESENTATION_BITS.md`](docs/RAG_REPRESENTATION_BITS.md), colour semantics/assets in [`docs/RAG_COLOUR.md`](docs/RAG_COLOUR.md), and editor/project semantics in [`docs/RAG_PROJECT_EDITOR.md`](docs/RAG_PROJECT_EDITOR.md).

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

The native right-side editor exposes the registered fault catalogue and selected operator parameters directly. Baseline shortcuts are:

- `Ctrl+O` — open a WIC-supported image;
- `Ctrl+Shift+O` — open a `.fmproj` project;
- `Ctrl+S` / `Ctrl+Shift+S` — save / save-as project;
- `Ctrl+E` — export the current genome against the **full-resolution canonical source** as PNG;
- `Ctrl+Z` / `Ctrl+Y` — undo / redo manual project edits;
- `Ctrl+Insert` — add the currently selected registered operator type;
- `Ctrl+D` — duplicate selected stack operator with a new stable instance ID;
- `Delete` — remove selected operator;
- `Ctrl+Up` / `Ctrl+Down` — select previous/next operator;
- `Alt+Up` / `Alt+Down` — reorder selected operator;
- `X` — bypass/enable selected operator;
- `L` — toggle selected operator mutation lock;
- `Ctrl+L` — toggle selected parameter mutation lock;
- `[` / `]` — small selected numeric-parameter nudge;
- `Shift+[` / `Shift+]` — large nudge;
- `Space` — enable/disable the whole stack;
- `R` — deterministically reroll the explicit root seed;
- `F5` — rerender;
- `F` — fit image to the canvas;
- `1` — 1:1 display;
- mouse wheel — zoom relative to fit;
- left-button drag or arrow keys — pan;
- `B` — before/after;
- `P` — toggle deterministic proxy preview.

Mutation locks do **not** block deliberate manual edits; they are protection state for the FM-009 descendant mutation engine. Locks, project UI state and edit history never change canonical genome identity by themselves.

## Project files and source relinking

`.fmproj` v1 is strict human-readable JSON. It embeds the canonical genome object and records:

- source path metadata plus the normalized FM-003 source identity;
- mutation locks;
- proxy/session state;
- selected operator;
- separately identified non-semantic canvas view state.

If the recorded source is moved, FAULTMINE accepts an explicitly relinked file only when its normalized content identity is identical. Missing sources and changed content are distinguished; changed content at the old path is never silently accepted as the same provenance.

FM-007 palettes/LUTs remain embedded canonical asset JSON inside the operator parameters that use them, so project v1 requires no second asset serialization system.

## Non-GUI canonical render hook

The narrow developer CLI remains available:

```powershell
.\build\Debug\FAULTMINE-render.exe input.png genome.json output.png
```

It validates the genome against the **default** registry (starter faults plus accepted memory/addressing, representation/bit and colour families), normalizes the input through WIC, executes the canonical CPU stack, writes a PNG, and prints normalized source/output identities.

Example genomes under `examples/` include the FM-003 starter stack, FM-005 addressing stack, FM-006 representation/bit stack and FM-007 colour stack. `examples/fm007-rust.fmpal` demonstrates the canonical human-editable FAULTMINE palette format.

## Authoritative development documentation

Start with [`docs/RAG_INDEX.md`](docs/RAG_INDEX.md). It indexes the product, architecture, deterministic contracts, image/pipeline contracts, session/presentation boundaries, fault-family contracts, project/editor contracts, roadmap, verification and autonomous-issue execution rules.

Implementation work is tracked as GitHub issues prefixed `FM-###`. Each implementation issue is intended to be executable autonomously: inspect current `main`, implement the stated scope, test it, reconcile documentation, open a PR, repair CI, merge after required checks pass, verify the merge landed on `main`, and only then close the issue when its acceptance criteria are satisfied.

## Planned stack

- C++20
- CMake + MSVC
- Win32 desktop shell and generic native stack editor
- pure-core deterministic genome/entropy/SHA-256 substrate
- canonical RGBA8 CPU fault pipeline
- application/session/editor/project models and deterministic proxy preprocessing
- D3D11 / DXGI presentation
- Windows Imaging Component (WIC) image I/O
- GitHub Actions on `windows-latest`

No third-party runtime dependency is part of the baseline design. Any future dependency must be justified against the standalone/distributable requirement and documented before adoption.
