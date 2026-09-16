# FAULTMINE

FAULTMINE is a standalone Windows glitch-art synthesis instrument built around **controlled, deterministic mistakes** rather than a conventional filter stack.

The program deliberately misinterprets image memory, addressing, representation, bitplanes, channels, timing, reconstruction and feedback. Every result must remain reproducible from its source identity, genome, seed and engine version.

The core interaction is evolutionary exploration: generate a tray of related glitch specimens, select interesting results, lock desirable genes, mutate the rest, breed compatible genomes and progressively reduce mutation radius to excavate a visual neighbourhood.

## Product principles

- **Reproducibility is a contract.** A saved genome is evidence sufficient to reconstruct the same canonical result.
- **Faults are simulated safely.** Artistic memory corruption must never rely on C/C++ undefined behaviour or unsafe host memory access.
- **The CPU reference engine is authoritative.** GPU rendering may accelerate presentation, but must not silently change canonical output.
- **Standalone means standalone.** v1 targets Windows x64 using C++20, Win32, D3D11/DXGI and WIC, with no required package manager, web runtime or external application framework.
- **Exploration beats presets.** Seeds, mutation, locks, crossover and lineage are first-class concepts.
- **Projects stay inspectable.** State is stored in versioned human-readable files with explicit source/provenance metadata.
- **Fast path to usefulness.** The roadmap establishes a usable end-to-end still-image glitch workflow before expanding the fault catalogue.

## Development status

FM-001 established the native project substrate:

- C++20/CMake project targeting Windows x64 with MSVC;
- dependency-free `faultmine_core` static library separated from Win32 code;
- minimal Unicode Win32 `FAULTMINE.exe` shell;
- zero-dependency CTest target;
- strict project warning/conformance policy (`/W4 /WX /permissive-`);
- Debug and Release Windows CI with real Win32 window-lifecycle smoke checks.

FM-002 established the deterministic substrate:

- versioned genome and operator-descriptor model;
- fixed-width 64-bit root seeds and stable 128-bit operator instance IDs;
- named deterministic SplitMix64 streams derived independently from semantic identity;
- unbiased bounded-integer mapping;
- strict UTF-8 JSON parsing and canonical genome serialization;
- pure-core SHA-256 genome identity;
- exact known-answer tests for entropy, serialization and hashes.

FM-003 establishes the first complete visual CPU path:

- canonical straight RGBA8 image buffers with checked allocation/stride rules;
- normalized source identity independent of pathname;
- WIC still-image decode and PNG export adapters;
- serial canonical CPU pipeline execution over FM-002 genomes;
- starter row/stride/address/channel/bit/random-scanline fault operators;
- exact per-operator and multi-stack golden tests;
- `FAULTMINE-render.exe` for non-GUI source + genome -> canonical PNG execution.

The byte-level deterministic rules are documented in [`docs/RAG_DETERMINISM.md`](docs/RAG_DETERMINISM.md); visual/source/operator semantics are documented in [`docs/RAG_IMAGE_PIPELINE.md`](docs/RAG_IMAGE_PIPELINE.md). D3D11 presentation begins with FM-004.

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

`configure.ps1` uses Visual Studio Installer's `vswhere.exe` to locate an MSVC x64 toolchain and currently selects either the Visual Studio 2026 or Visual Studio 2022 CMake generator. This keeps `windows-latest` CI and normal VS 2022 development machines on the same source/build contract without pinning the repository to one runner image.

Release uses the same configured build tree:

```powershell
.\scripts\build.ps1 -Configuration Release
.\scripts\test.ps1 -Configuration Release
.\scripts\run.ps1 -Configuration Release
```

The wrappers are deliberately thin. After configuration, their direct equivalents are:

```powershell
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

A direct configure command is also possible when choosing the generator manually, for example:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
```

Use `Visual Studio 18 2026` instead when building with Visual Studio 2026 and a compatible CMake version.

Build outputs are generated under `build/`; with a Visual Studio multi-configuration generator the desktop executable is `build/Debug/FAULTMINE.exe` or `build/Release/FAULTMINE.exe`.

## Non-GUI canonical render hook

FM-003 adds a deliberately narrow developer CLI:

```powershell
.\build\Debug\FAULTMINE-render.exe input.png genome.json output.png
```

It validates the genome against the starter registry, normalizes the input through WIC, executes the canonical CPU stack, writes a PNG, and prints normalized source/output identities. This is not the later batch miner; it exists so the visual engine can be exercised without the GUI.

## Authoritative development documentation

Start with [`docs/RAG_INDEX.md`](docs/RAG_INDEX.md). It indexes the product, architecture, deterministic byte-level rules, canonical image/pipeline rules, roadmap, verification and autonomous-issue execution contracts.

Implementation work is tracked as GitHub issues prefixed `FM-###`. Each implementation issue is intended to be executable autonomously: inspect current `main`, implement the stated scope, test it, reconcile documentation, open a PR, repair CI, merge after required checks pass, verify the merge landed on `main`, and only then close the issue when its acceptance criteria are satisfied.

## Planned stack

- C++20
- CMake + MSVC
- Win32 desktop shell
- pure-core deterministic genome/entropy/SHA-256 substrate
- canonical RGBA8 CPU fault pipeline
- D3D11 / DXGI presentation
- Windows Imaging Component (WIC) image I/O
- GitHub Actions on `windows-latest`

No third-party runtime dependency is part of the baseline design. Any future dependency must be justified against the standalone/distributable requirement and documented before adoption.
