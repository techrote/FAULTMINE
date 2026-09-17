# FAULTMINE

FAULTMINE is a standalone Windows x64 glitch-art synthesis instrument built around **controlled, deterministic mistakes** rather than a conventional filter stack. It deliberately misinterprets image memory, addressing, representation, bitplanes, channels, colour, timing, reconstruction and feedback while keeping canonical results reproducible from explicit source/genome/seed/versioned semantics.

The core interaction is evolutionary exploration: generate related specimens, retain interesting results, lock desirable genes, mutate the rest, breed compatible genomes, navigate durable lineage and reduce mutation radius to excavate a visual neighbourhood.

## v1.0.0 status

FM-001 through FM-015 form the v1 release campaign and all intended v1 subsystems are included:

- deterministic genome, stable instance IDs, named entropy streams and canonical identities;
- normalized straight-RGBA8 WIC source handling and the authoritative CPU fault pipeline;
- D3D11/DXGI presentation with deterministic non-canonical proxy previews;
- memory/addressing, representation/bit and colour/LUT/quantisation/dither fault families;
- descriptor-driven stack editing, typed parameters, mutation locks, undo/redo and project persistence;
- independently addressed mutation descendants, specimen trays, typed mutation radius and promotion;
- ordered-parent typed crossover, durable favourites and an acyclic provenance lineage graph;
- explicit integer-frame temporal replay, feedback/modulation and timeline controls;
- canonical still/contact-sheet/frame-sequence export with strict provenance manifests;
- a bounded separate-process malformed-codec laboratory plus frozen materialized source provenance;
- deterministic resumable headless batch mining, exact/near dedupe and descriptor-space diversity selection;
- v1 hardening, explicit resource ceilings, performance regression budgets, version/support diagnostics and a clean self-contained Windows x64 portable package.

The final release contract, package contents, resource ceilings, performance workloads and release checklist are in [`docs/RAG_RELEASE_V1.md`](docs/RAG_RELEASE_V1.md).

## Product contracts

- **Reproducibility is a contract.** A saved genome plus the normalized source and, for temporal work, explicit frame index is sufficient to reconstruct canonical output under the recorded engine/schema versions.
- **Faults are simulated safely.** Artistic corruption uses checked logical memory/address rules, never C/C++ undefined behaviour or unsafe host-memory access.
- **The CPU engine is authoritative.** D3D11 presents results. Proxy previews are deterministic but explicitly non-canonical final outputs.
- **Standalone means standalone.** The portable build needs normal Windows system components only; no Node.js, Python, package manager, web runtime, cloud account or installer service is required.
- **Projects remain inspectable.** `.fmproj` is strict versioned human-readable state with source/provenance, lineage/favourites, locks and separated session/view state.
- **External-decoder experiments stay contained.** Decoder-dependent recovery runs in a bounded worker; only validated materialized pixels become a stable downstream source.

## Run the portable package

The release artifact is `FAULTMINE-1.0.0-win64.zip`. Extract it and run `FAULTMINE.exe` directly, or use the optional `Run FAULTMINE.cmd` wrapper. The Release executables use the statically linked MSVC runtime (`/MT`), so a separately installed Visual C++ developer runtime is not part of the package contract.

`FAULTMINE.exe --diagnostics` shows the application and principal engine/schema versions without exposing sensitive machine data.

## Build prerequisites

Building from source requires:

- 64-bit Windows 10/11 or a compatible supported Windows development environment;
- Visual Studio 2022 or 2026 Build Tools/IDE with **Desktop development with C++**, the MSVC x64 toolchain and Windows SDK;
- CMake 3.25 or newer, with generator support for the installed Visual Studio version;
- PowerShell 7 or Windows PowerShell for convenience scripts.

No third-party library/package-manager runtime is required.

## Configure, build and test

From PowerShell at the repository root:

```powershell
.\scripts\configure.ps1
.\scripts\build.ps1 -Configuration Debug
.\scripts\test.ps1 -Configuration Debug
.\scripts\run.ps1 -Configuration Debug

.\scripts\build.ps1 -Configuration Release
.\scripts\test.ps1 -Configuration Release
.\scripts\run.ps1 -Configuration Release
```

Equivalent direct commands after configuration are:

```powershell
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Build outputs are under `build/`; the desktop executable is `build/Debug/FAULTMINE.exe` or `build/Release/FAULTMINE.exe`.

To build and verify the same portable layout used by CI:

```powershell
.\scripts\package.ps1 -Configuration Release -OutputDir artifacts
.\scripts\verify-package.ps1 -ArchivePath .\artifacts\FAULTMINE-1.0.0-win64.zip
```

The verifier extracts to an unrelated temporary directory, checks required/minimal contents and 1.0.0 version metadata, rejects build/source debris and dynamic MSVC C++ runtime imports, then runs packaged native smoke and the noninteractive open/edit/mutate/lineage/save/reopen/export/temporal workflow self-test.

## Interactive controls

The right-side native editor exposes registered fault operators and typed parameters. Baseline editor/canvas shortcuts include `Ctrl+O` open image, `Ctrl+Shift+O` open project, `Ctrl+S`/`Ctrl+Shift+S` save/save-as, `Ctrl+E` canonical PNG export, `Ctrl+Z`/`Ctrl+Y` undo/redo, `Ctrl+Insert` add operator, `Ctrl+D` duplicate, `Delete` remove, `Ctrl+Up`/`Ctrl+Down` select, `Alt+Up`/`Alt+Down` reorder, `X` bypass, `L` whole-operator mutation lock, `Ctrl+L` parameter lock, `[`/`]` numeric nudge (`Shift` for large), `Space` whole-stack enable/disable, `R` deterministic root-seed reroll, `F5` rerender, `F` fit, `1` 1:1, wheel zoom, drag/arrow pan, `B` before/after and `P` proxy-preview toggle.

Exploration adds `Ctrl+Alt+G` generate mutation tray, `Ctrl+Alt+R` reroll descendants, `Ctrl+Alt+Enter` promote selected specimen, `Ctrl+Alt+P` toggle durable Favourite, `Ctrl+Alt+Left`/`Right` change mutation radius, normal thumbnail click for comparison, `Ctrl+click` to assign ordered crossover ranks and `Ctrl+Alt+B` to breed 2–8 ordered parents. Lineage Activate navigates retained specimens; Provenance reports source/genome/parents and mutation/crossover address data.

The Timeline menu provides play/pause, exact frame stepping, reset/seek and preview-rate control. Playback speed is presentation state; the explicit semantic frame index remains canonical. Export provides atomic canonical stills, contact sheets and frame sequences with provenance manifests and explicit overwrite/cancellation policy.

Mutation locks protect search/crossover but do not block deliberate manual edits. A whole-operator lock is a topology anchor. Manual undo history and durable specimen lineage are intentionally distinct.

## Projects and provenance

`.fmproj` schema v2 embeds the active canonical genome and records normalized source provenance, active-genome mutation locks, durable lineage/favourites, proxy/session state, selected operator and separately identified non-semantic canvas state. The parser also accepts historical project v1 and migrates it conservatively to one `migrated-project` root without inventing historical parent/seed/policy information; the next save writes v2.

Lineage nodes retain complete typed genomes, canonical genome/source identities and explicit derivation metadata. Mutation nodes record parent, policy, seed, descendant index and radius. Crossover nodes record ordered parents, policy and crossover seed. Rendered thumbnails are disposable presentation products, never lineage authority.

A moved source may be explicitly relinked only when its normalized content identity matches. Changed content is never silently accepted as the same provenance. FM-013 laboratory projects may embed validated frozen normalized pixels/provenance so reopening does not rerun unstable external decoder recovery.

## Command-line tools

Canonical single render:

```powershell
.\build\Release\FAULTMINE-render.exe input.png genome.json output.png
```

It validates the genome, normalizes the source through WIC, executes the canonical CPU stack at frame 0, writes PNG and prints source/output identities.

Contained laboratory experiments use `FAULTMINE-lab.exe`; the helper worker `FAULTMINE-lab-worker.exe` is an implementation boundary rather than a normal interactive entry point.

A deterministic batch search can start from source+genome (or project input):

```powershell
.\build\Release\FAULTMINE-batch.exe `
  --source input.png `
  --genome genome.json `
  --out .\mine-001 `
  --seed 0123456789abcdef `
  --count 2048 `
  --threads 8
```

Useful mining options include `--begin`, `--radius low|medium|high`, explicit `--frame`, `--render canonical|proxy`, proxy bounds, inclusive `--near-threshold`, `--select`, verified `--resume`, `--selected-stills` and `--no-contact-sheet`. Ctrl+C writes a coherent partial manifest rather than fabricating completion. Selected results are emitted through the ordinary project lineage/export paths and reopen in the GUI.

`FAULTMINE-perf.exe` is a CI/development release probe rather than a shipped portable tool. It measures the representative workloads and enforces the broad regression ceilings defined in `RAG_RELEASE_V1.md`.

## Resource and safety boundaries

v1 rejects resource-limit violations explicitly rather than truncating state. Important release ceilings include a 512 MiB canonical RGBA8 image allocation, 64-operator mutation/genome topology contract, 4096 retained lineage specimens, the JSON parser's bounded nesting contract, and the laboratory worker's deadline/Job Object memory/validated-IPC boundaries. Subsystem-specific batch, temporal, proxy and arbitrary-binary limits remain defined in their RAG contracts.

## CI and release

Normal CI on `windows-latest` is the v1 release gate: Debug build/tests/smoke; Release build/tests/smoke; performance measurement; portable package construction; clean extracted-package dependency/content verification; packaged smoke/workflow self-test; release-candidate ZIP and performance JSON upload.

The separate Release workflow repeats those gates for manual candidates and tags. Tagged publication is allowed only for a tag whose commit is reachable from `main`; a successful compile alone never publishes a release.

The deterministic suites retain exact PRNG/serialization known-answer vectors, canonical image hashes, project migration fixtures, mutation/crossover/lineage contracts, temporal replay/export equality, worker crash/hang/IPC recovery and GUI/headless semantic equivalence. `/W4 /WX` remains mandatory; FM-015 introduces no warning/test suppression to ship.

## Authoritative documentation

Start with [`docs/RAG_INDEX.md`](docs/RAG_INDEX.md). It indexes product/architecture/determinism, every implemented fault and workflow subsystem, verification, roadmap, issue protocol and the final v1 release contract.

Implementation work is tracked as `FM-###` issues and follows the repository protocol: inspect accepted `main`, implement/test/reconcile docs, open a PR, repair required CI, merge only when green, verify the accepted tree actually landed on `main`, then close the issue only when its acceptance criteria are genuinely satisfied.

## Implemented stack

- C++20 + CMake + MSVC x64, strict `/W4 /WX`;
- Win32 native desktop shell;
- canonical CPU RGBA8 still/temporal fault engine;
- D3D11/DXGI presentation;
- WIC source/PNG I/O and isolated decoder worker;
- versioned project/genome/provenance/export/batch contracts;
- deterministic mutation/crossover/lineage and resumable batch mining;
- static MSVC runtime portable Release packaging;
- GitHub Actions release gating on Windows.

No third-party runtime dependency is part of the v1 baseline. Any future dependency must be justified against the standalone/distributable contract and documented before adoption.
