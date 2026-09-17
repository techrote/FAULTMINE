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

FM-004 established the first interactively useful desktop shell:

- D3D11/DXGI presentation of already-rendered CPU pixels, with hardware device + WARP fallback;
- native WIC file-open and canonical PNG export;
- explicit application/session state separate from canonical genome state;
- fit, 1:1, zoom, pan and before/after canvas interaction;
- deterministic nearest-neighbour proxy previews for large sources, visibly marked `PROXY`;
- full-resolution canonical export even while a proxy is displayed;
- compact starter-fault controls without requiring JSON editing;
- semantic render scheduling outside `WM_PAINT`;
- D3D device-recreation handling that does not mutate canonical state.

FM-005 adds the first substantial memory/addressing family while preserving host-memory safety:

- one shared logical-address boundary abstraction (`wrap`, `clamp`, `fill`);
- linear address offset and combined XOR/AND/OR address-bit faults;
- coordinate swap/XOR/offset remapping;
- deterministic affine tile permutation with explicit partial-edge behavior;
- line/band repeat/remap faults;
- deterministic named-stream burst-address faults with bounded work;
- typed mutation-domain hints in operator descriptors for later FM-009 search;
- a composed default registry used by the non-GUI renderer while the original FM-003 starter registry remains compatibility-focused.

FM-006 adds a representation/bit family that models data being misunderstood rather than merely recoloured:

- arbitrary channel routing, duplication, drop/fill and selected-channel spatial delay;
- explicit 2/4-byte logical word lane reversal/rotation;
- host-independent RGB565/BGR565/RGBA4444/ARGB1555 packing and endian disagreement;
- planar/interleaved layout mismatch;
- explicit signed-byte reinterpretation modes;
- zero-fill shifts, nibble swaps, bitplane exchange and stuck-at masks;
- structured named-stream block bit bursts with bounded work;
- typed mutation domains for every new parameter.

FM-007 adds deterministic colour synthesis for constraining structural glitches into coherent visual families:

- versioned embedded RGBA palette and four-channel 256-entry LUT assets with canonical JSON and SHA-256 identity;
- nearest-palette and integer luminance-gradient mapping;
- exact per-channel LUT mapping and round-half-up channel quantisation;
- fixed Bayer 2x2/4x4/8x8 ordered dithering;
- independently named per-pixel/channel seeded-noise dithering;
- deterministic endpoint-ramp palette generation with seeded interior RGB jitter;
- structured `colour_rgba`, `palette`, and `lut` mutation domains for later FM-009 search.

The byte-level deterministic rules are documented in [`docs/RAG_DETERMINISM.md`](docs/RAG_DETERMINISM.md), canonical image/operator semantics in [`docs/RAG_IMAGE_PIPELINE.md`](docs/RAG_IMAGE_PIPELINE.md), interactive session/presentation boundaries in [`docs/RAG_SESSION_PRESENTATION.md`](docs/RAG_SESSION_PRESENTATION.md), memory/addressing semantics in [`docs/RAG_MEMORY_ADDRESSING.md`](docs/RAG_MEMORY_ADDRESSING.md), representation/bit semantics in [`docs/RAG_REPRESENTATION_BITS.md`](docs/RAG_REPRESENTATION_BITS.md), and colour semantics/assets in [`docs/RAG_COLOUR.md`](docs/RAG_COLOUR.md).

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

After launching `FAULTMINE.exe`:

- `Ctrl+O` — open a WIC-supported image;
- `Ctrl+E` — export the current genome against the **full-resolution canonical source** as PNG;
- `F` — fit image to the canvas;
- `1` — 1:1 display;
- mouse wheel — zoom relative to fit;
- left-button drag or arrow keys — pan;
- `B` — before/after;
- `P` — toggle deterministic proxy preview;
- `Space` — enable/disable the starter fault stack;
- `[` / `]` — row-offset -1 / +1;
- `-` / `=` — jitter magnitude -1 / +1;
- `R` — deterministically reroll the explicit root seed.

The bottom status line identifies full versus proxy preview, source/preview dimensions, before/result state, starter parameters, seed prefix and D3D11 hardware/WARP mode.

FM-004 intentionally keeps these controls compact. The generic data-driven stack editor is FM-008 work; FM-005/FM-006/FM-007 operators are currently exercised through canonical genomes/the CLI until that editor lands.

## Non-GUI canonical render hook

The narrow developer CLI remains available:

```powershell
.\build\Debug\FAULTMINE-render.exe input.png genome.json output.png
```

It validates the genome against the **default** registry (starter faults plus accepted memory/addressing, representation/bit and colour families), normalizes the input through WIC, executes the canonical CPU stack, writes a PNG, and prints normalized source/output identities.

Example genomes under `examples/` include the FM-003 starter stack, FM-005 addressing stack, FM-006 representation/bit stack and FM-007 colour stack. `examples/fm007-rust.fmpal` demonstrates the canonical human-editable FAULTMINE palette format.

## Authoritative development documentation

Start with [`docs/RAG_INDEX.md`](docs/RAG_INDEX.md). It indexes the product, architecture, deterministic contracts, image/pipeline contracts, session/presentation boundaries, fault-family contracts, roadmap, verification and autonomous-issue execution rules.

Implementation work is tracked as GitHub issues prefixed `FM-###`. Each implementation issue is intended to be executable autonomously: inspect current `main`, implement the stated scope, test it, reconcile documentation, open a PR, repair CI, merge after required checks pass, verify the merge landed on `main`, and only then close the issue when its acceptance criteria are satisfied.

## Planned stack

- C++20
- CMake + MSVC
- Win32 desktop shell
- pure-core deterministic genome/entropy/SHA-256 substrate
- canonical RGBA8 CPU fault pipeline
- application/session model and deterministic proxy preprocessing
- D3D11 / DXGI presentation
- Windows Imaging Component (WIC) image I/O
- GitHub Actions on `windows-latest`

No third-party runtime dependency is part of the baseline design. Any future dependency must be justified against the standalone/distributable requirement and documented before adoption.
