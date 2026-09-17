# FAULTMINE verification strategy

## Verification goals

Testing must prove that FAULTMINE's artistic randomness is intentional while its engineering behaviour is not random.

The highest-risk contracts are:

1. deterministic output/provenance;
2. memory/address safety despite simulated corruption;
3. persistence/migration correctness;
4. isolation of malformed-codec experiments;
5. UI/export agreement on canonical semantics;
6. responsiveness without semantic shortcuts.

## Baseline CI

FM-001 should establish GitHub Actions on `windows-latest` with x64 MSVC and CMake.

At minimum, CI should converge on:

- configure + build Debug;
- configure + build Release;
- core/unit test execution;
- deterministic fixture execution;
- warning policy for project code;
- packaging/release checks once packaging exists.

UI automation may be limited on hosted CI. Core correctness must therefore remain independently testable without opening a window or creating a D3D device.

## Determinism tests

### PRNG and stream derivation

Store exact known-answer vectors for:

- root seed parsing;
- stream-key derivation;
- generator state transition/output;
- bounded integer mapping;
- per-gene/per-coordinate derivation where used.

A refactor is not allowed to change these values accidentally. An intentional algorithm change requires an explicit engine/schema version transition and migration/compatibility decision.

### Canonical serialization

Fixtures verify exact canonical bytes/text for representative genomes, including:

- key order;
- operator order and stable IDs;
- integer/fixed-point formatting;
- strings/enums/booleans;
- default/omission rules;
- UTF-8/line ending rules;
- unknown/unsupported version rejection.

Round-trip parsing must preserve canonical semantic state.

### Canonical image output

Use tiny deterministic images generated programmatically or stored as small fixtures. For representative pipelines, test exact output bytes and/or a collision-resistant hash of canonical output.

Fixtures should cover boundaries rather than only pretty examples:

- 1x1 and narrow images;
- odd row widths;
- minimum/maximum legal parameter values;
- large logical stride offsets mapped through safe policies;
- disabled operators;
- repeated operator types with different instance IDs;
- structured random faults;
- proxy versus full-resolution identity rules where applicable.

### Build-mode equivalence

Where practical, compare deterministic vectors/output hashes from Debug and Release CI artifacts. Optimisation must not change canonical semantics.

## Memory and arithmetic safety tests

Every operator that transforms logical addresses or dimensions needs boundary tests for:

- zero/invalid dimensions rejected before execution;
- multiplication/addition overflow;
- stride underflow/overflow concepts;
- negative logical offsets where supported;
- wrap/clamp/drop/fill boundaries;
- block coordinates at image edges;
- huge parameter values rejected/normalized according to contract.

The test oracle should reason about logical-address mapping, not merely assert that the process did not crash.

Sanitizer support on MSVC/clang-cl may be added when it is stable for the target configuration, but sanitizers supplement rather than replace explicit checked arithmetic and tests.

## Persistence and migration tests

For every persisted schema version retained by the project:

- parse valid fixture;
- reject malformed required fields;
- reject unsupported future versions clearly;
- preserve unknown optional data only if the schema explicitly promises such preservation;
- migrate old -> current through explicit migration steps;
- canonicalize current -> current without semantic drift;
- verify source identity mismatch detection;
- verify lineage/locks/favourites survive save/load once those features exist.

Never test migrations only against freshly generated files from the same parser. Retain representative historical fixtures once formats are released.

## Operator contract tests

Each operator should have:

- parameter validation tests;
- at least one exact known-output test;
- deterministic-random test where entropy is used;
- disabled/no-op behaviour test;
- edge/bounds tests;
- serialization round-trip test through the registry;
- mutation-descriptor validity test once mutation metadata exists.

Fault-family issues should also include cross-operator pipeline tests because glitches are intentionally compositional.

## Mutation and evolutionary search tests

Tests must prove:

- descendant N is independent of whether descendants 0..N-1 were generated/rendered;
- parallel execution order does not alter genomes;
- locked genes never mutate;
- low-radius mutations remain within their documented locality rules;
- high-radius topology mutation always produces a registry-valid pipeline;
- same parent + policy + seed + descendant index yields the same child;
- crossover is deterministic for the same parents/seed/policy;
- lineage links survive project reload.

Avoid asserting subjective “quality”. The system can test diversity/novelty descriptors mathematically without claiming aesthetic preference.

## Temporal tests

Temporal state is tested using explicit frame/tick sequences.

Verify:

- frame N from a known initial state is stable;
- playback speed/UI paint rate does not alter semantic frame state;
- pause/resume does not insert hidden ticks;
- feedback state is project/model-owned;
- seeded modulators produce exact known vectors;
- sequence export matches canonical per-frame rendering;
- checkpoint/replay, if introduced, matches replay from the defined initial state.

## D3D11/UI verification

The presentation layer should be thin enough that most semantics are testable below it.

Useful automated/manual checks include:

- D3D device/window creation smoke test where CI permits;
- device-loss/recreation path does not mutate project/genome state;
- canvas fit/zoom/pan does not affect canonical pixels;
- proxy/non-canonical preview state is visibly indicated;
- before/after uses the same normalized source identity;
- file-dialog cancellation is harmless;
- error surfaces include actionable context.

If a future GPU operator claims canonical equivalence, it requires explicit CPU-vs-GPU conformance fixtures across supported hardware/driver environments before becoming authoritative.

## Worker-process verification

For FM-013 and later, test the isolation boundary with synthetic worker behaviours:

- normal completion;
- structured decoder failure;
- crash;
- hang/timeout;
- oversized response;
- malformed IPC message;
- memory pressure where practical;
- host shutdown while worker is active.

The host must remain usable and must never trust unchecked dimensions/byte counts returned by the worker.

## Export verification

Exports must record enough provenance to identify:

- source identity;
- genome identity/canonical representation;
- root seed;
- engine/schema/operator versions;
- explicit temporal range for sequences;
- canonical/proxy status;
- output dimensions/format.

Tests verify that final still/sequence export renders from canonical full-resolution semantics by default rather than serializing a thumbnail/proxy buffer.

## Performance verification

Performance is important, but no optimisation may redefine canonical output.

Track at least:

- canonical CPU render time for representative image sizes/stacks;
- D3D upload/present cost;
- specimen proxy generation throughput;
- memory consumption for active source/result/tray caches;
- project load/save latency;
- temporal playback headroom once temporal features exist.

Do not lock premature numeric pass/fail budgets into early issues. FM-015 should establish release budgets from measured representative workloads and the final architecture. Until then, regressions should be measured and reported rather than hidden behind lower-quality semantics.

## Headless/batch verification

The headless tool must invoke the same canonical core as the GUI, not a reimplemented approximation.

For the same source/genome/seed/tick, GUI and headless canonical hashes must match.

Batch ordering, thread count and resume/restart must not change generated specimen identities.

## Packaging/release verification

The v1 portable package should be buildable on CI from a clean checkout and should include only required redistributable artefacts/documentation.

Release checks should include:

- clean Release build;
- full deterministic test suite;
- portable archive construction;
- executable/version metadata;
- launch from an extracted directory with no development tree present;
- open/edit/save/reopen/export smoke workflow;
- no mandatory package manager/web runtime/external service;
- documentation/build/run instructions match the shipped artefacts.

## Evidence in PRs

Every implementation PR should state:

- tests added/changed;
- commands/checks run;
- deterministic fixtures affected and why;
- persisted-format changes/migrations;
- known limits or follow-up work;
- CI result before merge.

A changed golden hash is evidence requiring explanation, not something to update mechanically until tests pass.

## FM-015 v1.0.0 resolution

FM-015 turns the earlier release-verification goals above into an executable gate. The authoritative numeric ceilings, resource limits, version matrix, portable-package contract and measured reference workload are frozen in [`RAG_RELEASE_V1.md`](RAG_RELEASE_V1.md).

The final pull-request gate now performs Debug build/tests/native smoke, Release build/tests/native smoke, the representative performance probe, portable archive construction, clean extracted-package validation, static-MSVC-runtime dependency validation, packaged native smoke, and a packaged `--release-self-test` covering open -> edit/mutate -> favourite lineage -> save -> parse/reopen -> canonical still export -> explicit temporal step/frame-sequence export. The ZIP and exact `performance.json` are retained together as CI evidence.

Release-hardening tests add explicit failure coverage for the 512 MiB canonical image allocation limit, 64-operator parsed/programmatic genome limit, 4096-specimen retained-lineage limit and the generic JSON nesting limit while retaining the worker deadline/memory-bound assertions. These limits reject work explicitly rather than truncating or silently changing canonical state.

The dedicated Release workflow repeats the test/performance/package gate for manual candidates and tags. Tagged publication is permitted only for commits reachable from `main`; package construction alone is not sufficient to publish an unstable release. `/W4 /WX`, existing deterministic known-answer vectors, canonical image goldens, migration fixtures, temporal/export equivalence, batch/session agreement and worker containment remain mandatory release evidence rather than being weakened for packaging.
