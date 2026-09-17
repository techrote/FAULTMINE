# FAULTMINE

FAULTMINE is a standalone Windows glitch-art synthesis instrument built around **controlled, deterministic mistakes** rather than a conventional filter stack.

The program deliberately misinterprets image memory, addressing, representation, bitplanes, channels, timing, reconstruction and feedback. Every canonical result must remain reproducible from explicit source/genome/seed/versioned semantics.

The core interaction is evolutionary exploration: generate a tray of related glitch specimens, retain interesting results, lock desirable genes, mutate the rest, breed compatible genomes and progressively reduce mutation radius to excavate a visual neighbourhood.

## Product principles

- **Reproducibility is a contract.** A saved genome is evidence sufficient to reconstruct the same canonical result.
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

FM-010 adds the durable evolutionary layer:

- deterministic typed crossover for 2–8 **ordered** parents, with the first parent as the primary topology scaffold;
- conservative operator alignment, typed gene inheritance and deterministic new instance IDs for inherited unmatched operators;
- a durable acyclic lineage graph keyed by canonical genome identity, distinct from manual editor undo/redo;
- explicit mutation/crossover provenance, source identity and parent edges for retained specimens;
- durable favourites that survive tray rerolls and project reload;
- lineage navigation back to retained ancestors/descendants plus provenance inspection;
- `.fmproj` project schema v2 with lineage/favourites and explicit history-free migration from project v1.

The byte-level deterministic rules are documented in [`docs/RAG_DETERMINISM.md`](docs/RAG_DETERMINISM.md), canonical image/operator semantics in [`docs/RAG_IMAGE_PIPELINE.md`](docs/RAG_IMAGE_PIPELINE.md), session/presentation boundaries in [`docs/RAG_SESSION_PRESENTATION.md`](docs/RAG_SESSION_PRESENTATION.md), fault-family contracts in the corresponding RAG documents, manual editor/project foundations in [`docs/RAG_PROJECT_EDITOR.md`](docs/RAG_PROJECT_EDITOR.md), mutation search in [`docs/RAG_MUTATION_SEARCH.md`](docs/RAG_MUTATION_SEARCH.md), and crossover/lineage/project-v2 semantics in [`docs/RAG_LINEAGE_CROSSOVER.md`](docs/RAG_LINEAGE_CROSSOVER.md).

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

Mutation locks do **not** block deliberate manual edits. They protect search operations and remain outside canonical genome identity. A primary whole-operator lock is also a crossover topology anchor. Manual edit history and specimen lineage remain intentionally separate.

## Project files and source relinking

`.fmproj` schema v2 is strict human-readable JSON. It embeds the active canonical genome and records source provenance, active-genome mutation locks, durable specimen lineage/favourites, proxy/session state, selected operator and separately identified non-semantic canvas view state.

Lineage records retain complete typed genomes plus canonical genome/source identities and explicit derivation metadata. Mutation nodes store parent identity, mutation-policy version, seed, descendant index and radius. Crossover nodes store ordered parent identities, crossover-policy version and crossover seed. Rendered thumbnail/full-resolution pixels are never lineage authority.

The v2 parser accepts historical project v1 and migrates it conservatively to exactly one `migrated-project` lineage root. The migration does not fabricate parents or historical seed/policy information. The next save writes schema v2. Unsupported future project versions remain rejected.

If the recorded source is moved, FAULTMINE accepts an explicitly relinked file only when its normalized content identity is identical. Missing sources and changed content are distinguished; changed content at the old path is never silently accepted as the same provenance.

FM-007 palettes/LUTs remain embedded canonical asset JSON inside the operator parameters that use them, so project persistence requires no second asset serialization system.

## Non-GUI canonical render hook

The narrow developer CLI remains available:

```powershell
.\build\Debug\FAULTMINE-render.exe input.png genome.json output.png
```

It validates the genome against the **default** registry, normalizes the input through WIC, executes the canonical CPU stack, writes a PNG, and prints normalized source/output identities. Example genomes under `examples/` cover the accepted addressing, representation/bit and colour families; `examples/fm007-rust.fmpal` demonstrates the canonical human-editable palette format.

## Authoritative development documentation

Start with [`docs/RAG_INDEX.md`](docs/RAG_INDEX.md). It indexes the product, architecture, deterministic contracts, image/pipeline contracts, session/presentation boundaries, fault-family contracts, mutation/search/crossover/lineage contracts, project semantics, roadmap, verification and autonomous-issue execution rules.

Implementation work is tracked as GitHub issues prefixed `FM-###`. Each implementation issue is intended to be executable autonomously: inspect current `main`, implement the stated scope, test it, reconcile documentation, open a PR, repair CI, merge after required checks pass, verify the merge landed on `main`, and only then close the issue when its acceptance criteria are satisfied.

## Planned stack

- C++20
- CMake + MSVC
- Win32 desktop shell and generic native stack/exploration editor
- pure-core deterministic genome/entropy/SHA-256 substrate
- canonical RGBA8 CPU fault pipeline
- application/session/editor/project/lineage models and deterministic proxy preprocessing
- D3D11 / DXGI presentation
- Windows Imaging Component (WIC) image I/O
- GitHub Actions on `windows-latest`

No third-party runtime dependency is part of the baseline design. Any future dependency must be justified against the standalone/distributable requirement and documented before adoption.
