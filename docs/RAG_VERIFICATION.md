# FAULTMINE verification strategy

## Verification goals

Testing proves that FAULTMINE's artistic randomness is intentional while its engineering behaviour is not random. The highest-risk contracts are deterministic output/provenance, memory/address safety despite simulated corruption, persistence/migration correctness, isolation of malformed-codec experiments, UI/export/headless agreement on canonical semantics, and responsiveness without semantic shortcuts.

## v1 CI baseline

GitHub Actions on `windows-latest` configures x64 MSVC/CMake and uses `/W4 /WX` for project code. The v1 gate performs Debug build/tests/native smoke, Release build/tests/native smoke, representative performance measurement, portable package construction, extracted-package verification and packaged workflow smoke. Release-candidate CI retains the portable ZIP and measured performance JSON as artifacts.

UI automation remains deliberately thin on hosted CI. Core correctness is independently testable without making window paint or D3D timing semantic; the native smoke exists to verify composition and lifecycle rather than to replace model tests.

## Determinism tests

### PRNG and stream derivation

Store exact known-answer vectors for root-seed parsing, stream-key derivation, generator transitions/output, bounded integer mapping and per-gene/per-coordinate derivation. Refactors may not change these accidentally. An intentional algorithm change requires an explicit engine/schema version transition and migration/compatibility decision.

### Canonical serialization

Fixtures verify exact canonical bytes/text for representative genomes: key/operator order, stable IDs, integer formatting, strings/enums/booleans, omission rules, UTF-8/line endings and unsupported-version rejection. Round-trip parsing must preserve semantic state.

### Canonical image output

Programmatic tiny-image fixtures exercise exact bytes/hashes across boundaries including 1x1/narrow/odd widths, parameter boundaries, logical addressing, disabled/repeated operators, deterministic structured faults and proxy/full-resolution identity distinctions. The historical canonical goldens remain release contracts; FM-015 does not alter them.

### Build-mode and surface equivalence

Debug/Release must preserve the same canonical contracts. The headless batch path invokes the same core as the GUI/session, and accepted tests verify equivalent canonical pixels/hashes for the same source/genome/frame. Temporal export likewise matches explicit per-frame canonical rendering. Optimisation is never permission to update a golden mechanically.

## Memory, resource and arithmetic safety

Address/dimension operators test invalid dimensions, multiplication/addition overflow, logical stride extremes, signed offsets, wrap/clamp/fill boundaries, image edges and extreme parameters. The oracle reasons about bounded logical mapping rather than merely asserting non-crash.

FM-015 adds release-level allocation/state ceilings described in `RAG_RELEASE_V1.md`, including the 512 MiB canonical image-buffer limit and 4096 retained-lineage limit. Mutation/genome topology remains capped at 64 operators. Limit failures are explicit errors and must never silently truncate or alter semantic state. The generic JSON parser retains bounded nesting, duplicate-key rejection and strict UTF-8/schema handling.

Sanitizers may supplement these contracts when stable for the supported toolchain but do not replace checked arithmetic and explicit tests.

## Persistence and migration tests

For every retained persisted schema: parse valid fixture; reject malformed required fields and future versions; migrate historical versions explicitly; canonicalize current state without semantic drift; verify source-identity mismatch handling; and verify locks/favourites/lineage survive save/load.

Project schema v2 retains a real project-v1 migration fixture rather than testing migration only against files emitted by the current parser. FM-015's packaged self-test additionally performs save/parse/reopen on a project containing a promoted favourite mutation specimen.

## Operator contracts

Each operator family covers parameter validation, exact known output, deterministic random behaviour when applicable, disabled/no-op behaviour, edge bounds, serialization/registry agreement and mutation-descriptor validity. Cross-operator pipeline tests remain important because glitches are intentionally compositional.

## Mutation, crossover and evolutionary search

Tests prove independently addressed descendants, scheduling independence, lock protection, typed locality/topology policy, registry-valid high-radius results, exact parent/seed/index reproducibility, deterministic crossover for ordered parents and durable lineage across project reload. Diversity/novelty is tested mathematically without claiming subjective visual quality.

## Temporal tests

Temporal tests use explicit frame/tick sequences and verify stable frame N, independence from playback/paint rate, no hidden pause/resume ticks, model-owned feedback state, exact seeded modulators and equality between sequence export and canonical frame rendering. Future checkpoint acceleration must match replay from the defined initial state byte-for-byte.

## D3D11/UI verification

Presentation tests/smoke verify native device/window creation where CI permits, canvas state remaining non-semantic, deterministic proxy distinction, source-consistent before/after and harmless cancellation/error paths. The v1 performance probe separately records D3D11 upload/draw/present overhead; this is presentation measurement, not canonical input.

A future GPU operator claiming canonical authority requires explicit CPU-vs-GPU conformance evidence across supported hardware/driver environments before it can replace the CPU reference.

## Worker-process verification

The FM-013 isolation suite covers normal completion, structured decode failure, crash, hang/timeout, oversized/malformed responses and invalid returned dimensions/stride/byte counts. Job Object cleanup, deadline/cancellation and validated IPC keep malformed external-decoder behaviour outside the editor process. Materialized decoder-dependent pixels retain truthful provenance.

## Export verification

Export manifests bind source identity, canonical genome/identity, root seed, engine/schema/operator versions, canonical/proxy status, output dimensions/format and explicit temporal range when applicable. Tests prove still/sequence export uses full-resolution canonical execution rather than thumbnail/proxy state and that partial/cancelled output remains truthful.

## Performance verification

FM-015 establishes the v1 measured workload and numeric regression ceilings in `RAG_RELEASE_V1.md`. The release probe covers representative 1920x1080 canonical render, proxy generation, D3D upload/present, project serialize+parse, temporal frame replay, headless batch throughput and a conservative active-pipeline memory estimate.

Hosted runner variance means these ceilings are tripwires rather than latency guarantees. Exact measurements from the accepted candidate are retained as `performance.json` beside the portable archive. A performance change may reuse buffers, parallelize independently addressed work or introduce deterministic caches only if canonical equality remains proven. Approximate paths remain visibly preview-only.

## Headless/batch verification

The batch tool invokes the shared canonical core. Same source/genome/frame semantics agree with GUI/session canonical output; thread count, ordering and resume do not change candidate identities. Resume cache entries are revalidated rather than trusted by filename/existence.

## Packaging/release verification

The supported v1 artifact is `FAULTMINE-1.0.0-win64.zip`. CMake uses the static MSVC runtime strategy documented in `RAG_RELEASE_V1.md`. CI stages only runtime executables plus owned documentation/convenience files, then extracts the archive outside the development tree and verifies:

- required/minimal contents and absence of source/build debris;
- executable 1.0.0 version resources;
- no dynamic MSVC C++ runtime dependency;
- native application window smoke;
- open -> edit/mutate -> favourite lineage -> save -> parse/reopen -> canonical still export -> temporal step/sequence export through `--release-self-test`.

The dedicated Release workflow repeats Debug/Release tests, performance and package verification. Tagged GitHub Release publication is permitted only for a tag reachable from `main`; successful compilation alone cannot publish an unverified candidate.

## Evidence in PRs

Every implementation/release PR records tests added/changed, commands/checks run, deterministic fixtures affected and why, persistence/version changes, known limits/follow-up work and final CI result. FM-015 additionally records measured performance, package artifact evidence and exact tested-head/merged-tree verification before issue closure.
