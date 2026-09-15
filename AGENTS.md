# AGENTS.md — FAULTMINE autonomous implementation contract

This file is authoritative for implementation work in this repository unless a later, more specific repository document explicitly supersedes part of it.

## 1. Read before changing code

For every implementation issue, read:

1. `README.md`
2. `AGENTS.md`
3. `docs/RAG_INDEX.md`
4. every document referenced by `docs/RAG_INDEX.md`
5. the full issue body and all issue comments
6. current `main`, accepted prerequisite implementations, tests and workflows

The repository state on `main` is authoritative. If an issue description has become stale because prerequisite work changed the implementation, preserve the issue's intent and acceptance criteria while adapting the implementation to current `main`.

## 2. Autonomous execution protocol

Each `FM-###` implementation issue is intended to be completed autonomously.

- Inspect current `main` and verify prerequisites.
- Check whether equivalent work already exists before adding parallel mechanisms.
- Create a focused branch for the issue.
- Implement the complete issue scope, not merely scaffolding or a design note, unless the issue explicitly says otherwise.
- Add or update automated tests for behaviour introduced or changed.
- Update authoritative documentation when contracts, file formats, commands, architecture or user-visible behaviour change.
- Build and run the relevant local/CI-equivalent checks before opening the PR when practical.
- Open a PR that references the issue and explains implementation, tests, compatibility implications and known limitations.
- Repair failures until all required automated checks pass.
- Merge the PR after required checks pass. Repository agents are permitted to merge their own PRs for these issues.
- Verify from repository state that the merge actually landed on `main`.
- Close the issue only after its acceptance criteria are genuinely satisfied on `main`.

Do not claim completion from a green branch alone. Completion means verified code on `main`.

## 3. Architectural invariants

These are non-negotiable unless explicitly changed through a documented architecture decision.

### Determinism

Canonical output is a pure function of the normalized source content/identity, canonical genome, root seed, engine/schema version and explicit time/tick input when temporal operators are involved.

Do not use implementation-defined random facilities such as `std::uniform_*` as the canonical random contract. PRNG algorithms, seed derivation, stream derivation and integer-to-range mapping must be specified and tested.

Prefer deterministic integer/fixed-point arithmetic for canonical operators. Floating-point use in canonical semantics requires explicit justification, bounded/quantized behaviour and deterministic tests on supported targets.

### Safe simulated corruption

Never produce artistic corruption by performing real out-of-bounds access, use-after-free, uninitialised reads, invalid pointer arithmetic, data races or other C/C++ undefined behaviour.

Memory/address faults operate on explicit logical buffers with documented wrap/clamp/drop/fill semantics.

Potentially hostile malformed-codec experiments belong in an isolated worker process with resource/time limits and validated IPC boundaries.

### Authoritative CPU reference path

The canonical reference engine is CPU-side and host-testable without a D3D device. D3D11 initially presents canonical output rather than defining it.

A future accelerated implementation may be accepted only if it is proven equivalent for operators that claim canonical equivalence, or is explicitly labelled preview-only/non-authoritative.

Exports default to the canonical path.

### Standalone Windows application

The v1 baseline is Windows x64, C++20, Win32, D3D11/DXGI and WIC. Avoid external runtimes, Electron/web shells, mandatory package managers and large frameworks.

A small source-vendored dependency may be proposed only where the repository documents why the maintenance/security/size cost is lower than an in-tree implementation. Do not add one casually.

### Versioned project/genome formats

Persisted formats are explicit, versioned, migration-aware and test-covered. Unknown required fields or unsupported future versions must fail clearly rather than silently changing meaning.

Human readability is preferred. Canonical serialization used for identity/hashing must define ordering, number formatting and normalization independent of incidental container iteration order.

## 4. Engineering quality bar

- Compile warning-clean at the repository's configured warning level.
- Treat compiler warnings in project code as failures in CI once the bootstrap issue enables this policy.
- Keep platform-specific Win32/D3D/WIC code out of the pure core where practical.
- Avoid hidden global mutable state.
- Own resources with RAII.
- Use bounded arithmetic and validate dimensions, byte counts, strides and offsets before allocation/indexing.
- Errors visible to users must contain actionable context.
- Never silently discard or reinterpret project state during migration.
- Tests must be deterministic and must not depend on wall-clock timing, GPU vendor behaviour, locale or filesystem enumeration order unless explicitly testing those boundaries.

## 5. Verification expectations

The baseline CI should converge toward:

- configure/build Debug and Release on `windows-latest`;
- core unit tests;
- deterministic golden/hash fixtures generated from tiny in-repo or programmatic sources;
- strict genome/project parse/round-trip tests;
- malformed-input and overflow tests for fault operators;
- smoke tests for the executable where automation permits;
- package/archive construction checks before release work.

Tests should prove contracts, not freeze incidental implementation details.

## 6. Scope discipline

Do not redesign unrelated accepted components while completing an issue. If a discovered defect blocks the issue, fix it in scope when small and clearly required; otherwise document it and open a separate issue if repository policy/workflow permits.

Do not introduce speculative abstraction layers. Prefer explicit data contracts and composable operators over framework-building.

## 7. Documentation discipline

If implementation changes any of the following, update the corresponding RAG documentation in the same PR:

- architecture or source-tree boundaries;
- genome/project schema;
- determinism guarantees;
- fault semantics;
- commands/build/run instructions;
- user-visible interaction model;
- CI/release process;
- issue dependency assumptions.

`docs/RAG_INDEX.md` is the retrieval entry point and must remain accurate.
