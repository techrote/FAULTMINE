# FAULTMINE v1 release contract

This document freezes the release-readiness contract established by FM-015. It complements the subsystem RAG documents; it does not redefine their canonical semantics.

## Shipped v1 scope

FAULTMINE v1.0.0 includes all accepted FM-001 through FM-014 functionality: the deterministic genome/entropy substrate, canonical RGBA8 CPU pipeline, native Win32/D3D11 presentation, memory/addressing and representation fault families, colour/LUT work, generic project editor, deterministic mutation search, crossover/lineage/favourites, explicit temporal replay, provenance-rich export, the isolated external-decoder laboratory and the resumable headless batch miner. No accepted FM-001..FM-014 feature is intentionally deferred from v1.

The v1 canonical authority remains the CPU reference implementation. D3D11 presents pixels and does not redefine them. Proxy renders are deterministic exploration aids but non-canonical final outputs. External-decoder recovery is environment-dependent until validated normalized pixels are frozen/materialized; downstream work from those pixels is canonical under the ordinary source-identity contract.

## Version matrix

- application/product: `1.0.0`;
- engine contract: `1`;
- genome schema: `1`;
- project schema: `2`, with explicit project-v1 migration retained;
- entropy contract: `1`;
- mutation policy: `1`;
- crossover policy: `1`;
- proxy method: `1`;
- export manifest: `1`;
- laboratory IPC/provenance contracts: as frozen by `RAG_EXTERNAL_DECODERS.md`;
- batch manifest: `1`;
- visual descriptor: `1`.

`FAULTMINE.exe --diagnostics` displays the application and the principal engine/schema versions without collecting or exposing machine-identifying support data. Windows executable version resources also report product/file version 1.0.0.

## Release audit findings

The FM-015 audit retained the accepted architecture rather than introducing parallel persistence/rendering paths. The release gate exercises the existing deterministic known-answer and visual-hash tests, project-v1 migration, lineage/mutation/crossover provenance, temporal replay/export equivalence, worker crash/hang/malformed-IPC containment, and GUI/headless canonical equivalence already established by their subsystem suites.

The hardening pass additionally makes the following release boundaries explicit:

- canonical image buffers are limited to 512 MiB per RGBA8 image allocation and fail explicitly instead of attempting an unbounded allocation;
- mutation/genome topology uses a 64-operator v1 ceiling; mutation topology cannot exceed that contract;
- retained lineage state is limited to 4096 specimens and rejects further unique retention with an actionable export/prune message;
- generic structured JSON parsing retains its bounded 64-level nesting contract, duplicate-key rejection, UTF-8 validation and strict schema checking;
- the laboratory worker retains a 5 s default deadline and 512 MiB Job Object memory ceiling plus bounded/versioned IPC validation;
- batch ranges, worker counts, proxy extents, descriptor dimensions, temporal parameter ranges and arbitrary-binary interpretation remain explicitly bounded by their accepted subsystem contracts.

Resource-limit failure is an error, never silent truncation or semantic alteration.

## Portable Windows x64 package

The supported release artefact is `FAULTMINE-1.0.0-win64.zip`. It contains one top-level directory with:

- `FAULTMINE.exe`;
- `FAULTMINE-render.exe`;
- `FAULTMINE-batch.exe`;
- `FAULTMINE-lab.exe`;
- `FAULTMINE-lab-worker.exe`;
- `Run FAULTMINE.cmd` as an optional convenience launcher;
- `PORTABLE.txt`, `README.md` and RAG documentation.

The v1 CMake configuration statically links the MSVC C/C++ runtime using `/MT` for Release (`/MTd` for Debug). The archive therefore does not require a separately installed Visual C++ redistributable or development toolchain. Runtime dependencies are normal supported Windows system components used by the project, including Win32, D3D11/DXGI and WIC. No Node.js, Python, package manager, browser runtime, cloud account or installer service is required.

`scripts/package.ps1` stages the install tree and creates the archive. `scripts/verify-package.ps1` extracts it to an unrelated temporary directory, rejects source/build debris, verifies version resources and absence of dynamic MSVC runtime imports, then runs both the native window smoke test and the release workflow self-test from that extracted layout.

## Packaged workflow self-test

`FAULTMINE.exe --release-self-test` is a noninteractive support/release hook. It creates and opens a deterministic PNG through WIC, edits and mutates a project, retains a favourite lineage specimen with exact provenance, saves/parses/reopens project state, exports a canonical still plus manifest, adds a temporal operator, advances an explicit frame, and exports an explicit frame sequence. It uses only the packaged executables/libraries and Windows system components; it does not depend on the source or build tree.

This hook is not a substitute for the subsystem tests. Its purpose is to prove that the packaged composition still supports the principal open -> edit/mutate -> lineage -> save -> reopen -> canonical export -> temporal export path.

## Performance probe and budgets

`FAULTMINE-perf.exe` is a CI-only measurement executable and is intentionally not shipped in the portable archive. The v1 release gate measures representative workloads on the GitHub-hosted `windows-latest` x64 runner:

- median canonical 1920x1080 render of a four-operator deterministic stack;
- deterministic nearest-neighbour proxy generation to 640x360;
- D3D11 upload/draw/present of a 1280x720 proxy, recording whether WARP was used;
- canonical project serialize+parse round-trip;
- explicit temporal frame 8 replay;
- 32-descendant deterministic canonical batch mining on a 320x180 source;
- a conservative active-pipeline buffer estimate.

The release regression ceilings are deliberately looser than expected measurements because hosted-runner hardware is variable. They are tripwires, not performance claims:

| Workload | v1 CI ceiling |
| --- | ---: |
| 1920x1080 canonical render | 2500 ms |
| 640x360 proxy generation | 250 ms |
| 1280x720 D3D upload/present | 250 ms |
| project serialize+parse round-trip | 100 ms |
| temporal replay through frame 8 | 15000 ms |
| canonical batch of 32 descendants | 15000 ms |
| estimated active pipeline buffers | 64 MiB |

The accepted release-candidate workflow stores the exact measured JSON alongside the portable ZIP as CI evidence. These ceilings may only be tightened or changed from subsequent measurement; an optimisation may not alter canonical bytes to satisfy them. Approximate future accelerations must remain explicitly preview-only until exact equivalence is proven.

## Release CI

Every pull request and push to `main` runs the Windows x64 release gate:

1. configure from a clean checkout;
2. build and run all tests in Debug;
3. run native Debug window smoke;
4. build and run all tests in Release;
5. run native Release window smoke;
6. measure/enforce v1 performance budgets;
7. build the portable ZIP;
8. extract and verify package contents/runtime dependencies;
9. run packaged native smoke and the packaged release workflow self-test;
10. upload the ZIP and performance JSON as release-candidate evidence.

The separate `Release` workflow repeats the gated Debug/Release/test/performance/package verification path for manual candidates and tags. Only a tag whose commit is reachable from `main` is eligible for automatic GitHub Release publication; building alone never publishes an unstable commit.

## Reproducibility release gate

The existing deterministic tests remain authoritative. In particular, FM-015 does not intentionally revise any known-answer entropy/serialization vectors, canonical output hashes, headless-vs-session equivalence, temporal frame identities or historical project migration semantics. A future change to any of those requires an explicit versioned contract decision rather than mechanically updating a golden.

Release verification also requires no warning suppression or weakening of `/W4 /WX`. Static-runtime packaging is a distribution choice only and is not a canonical semantic input.

## Support diagnostics and recovery

Actionable failure surfaces should distinguish malformed input, unsupported versions, source-identity mismatch, resource ceilings, worker timeout/crash, collision policy and filesystem/export failure. Users should preserve the project/genome, source identity, manifest and application/schema versions when reporting a reproducibility problem. Machine-specific identifiers are not needed.

For a moved source, relink only to normalized content with the recorded source identity. For a materialized laboratory source, retain the frozen pixels/provenance rather than rerunning an unstable decoder experiment. For resource-limit failures, reduce source dimensions/use proxy exploration, reduce operator history, or export/prune retained lineage as appropriate; limits never silently rewrite state.

## Post-v1 work

Post-v1 changes are intentionally not folded into this release-hardening issue. Candidate future work includes measured semantics-neutral performance improvements, deterministic cache/pooling refinements, deeper UI automation, optional additional fault families, and any separately justified canonical GPU acceleration with exact conformance evidence. Cross-platform UI, cloud sync, plugin marketplaces and learned aesthetic models remain outside the v1 release contract.

## Release checklist

A v1 candidate is acceptable only when all of the following are true:

- all intended FM-001..FM-014 functionality is present on `main`;
- Debug and Release test suites are green with `/W4 /WX` intact;
- deterministic/golden, migration, headless, temporal, export and worker containment suites are green;
- release-hardening/resource-limit tests are green;
- the performance probe is within documented ceilings and its JSON is retained;
- the portable archive is created from a clean build and contains no source/build debris;
- extracted-package version/runtime-dependency checks pass;
- packaged native smoke and `--release-self-test` pass without the development tree;
- README and RAG documentation describe the shipped implementation;
- the implementation PR is merged only after required checks pass and the merged tree is verified on `main`.
