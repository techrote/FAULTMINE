# FAULTMINE architecture

## Architectural objective

FAULTMINE separates canonical glitch semantics from presentation, storage and unsafe external-decoder experiments. The core must remain deterministic, bounded and host-testable.

The intended v1 dependency direction is:

```text
Win32 UI  -> application/session layer -> deterministic core
D3D11 UI  -> presentation only        -> canonical image buffers
WIC I/O   -> normalized sources       -> deterministic core
worker    -> isolated risky decoding  -> validated image/result messages
CLI/tools -> deterministic core       -> tests/batch/export
```

Platform layers may depend on the core. The core must not require Win32 window handles, a D3D device, wall-clock time, user input timing or process-global mutable state.

## Proposed source tree

```text
/CMakeLists.txt
/src/app/                 application/session orchestration
/src/core/                deterministic engine, genomes, operators, mutation
/src/core/operators/      fault implementations and registry
/src/io/                  project/genome serialization and WIC adapters
/src/platform/win32/      windowing, dialogs, input, shell integration
/src/render/d3d11/        canvas/thumbnails/presentation
/src/worker/              isolated malformed-codec/raw-data worker executable
/src/headless/            CLI/batch miner entry point
/tests/                   deterministic/unit/integration tests
/tools/                   developer utilities if required
/docs/                    authoritative RAG documentation
```

Names may evolve, but dependency boundaries should not.

## Canonical image model

The reference engine operates on explicit owned image buffers with checked metadata:

- width and height;
- pixel/logical format identifier;
- row stride in bytes/elements;
- byte length;
- colour/alpha interpretation metadata when semantically relevant.

The initial normalized source format should be a simple fixed representation such as RGBA8 with a precisely documented byte/channel order. Reinterpretation operators may expose alternative *logical* views without invoking native type punning or unaligned/undefined access.

All size calculations use checked arithmetic before allocation or indexing.

## Logical memory faults

Memory faults address a virtual logical buffer rather than native memory. Every address transform resolves through an explicit policy:

- wrap modulo logical extent;
- clamp;
- drop/read fill value;
- mirror/other later documented policies.

An operator may simulate a nonsensical stride, offset, block address or pitch, but the implementation always maps the resulting logical address safely before touching host memory.

This separation is essential: the art may depict a buffer overrun while the program never performs one.

## Operator contract

Every canonical operator should expose at least:

- stable operator type ID;
- operator schema/version;
- typed parameter descriptors;
- deterministic parameter serialization;
- validation/normalization rules;
- capability flags (still/stateful, topology-safe, proxy-safe, etc.);
- execution function against explicit input/output/context;
- mutation descriptors for each mutable gene;
- optional migration logic from older operator versions.

Operators must not query global time or hidden RNG state. Any entropy is obtained from the supplied deterministic random context.

## Deterministic entropy

### Root seed

Each genome has an explicit root seed represented in a fixed-width textual/numeric form.

### Named streams

Avoid one mutable global PRNG sequence whose consumption count changes whenever an unrelated implementation detail is added. Derive random streams from stable identities such as:

```text
root seed
+ operator stable instance id
+ purpose tag
+ coordinate/block/frame/gene identity where applicable
```

A specified integer mixing/PRNG algorithm turns that tuple into deterministic values. The exact algorithm and range-mapping rules become part of the file/engine contract and are golden-tested.

For sequential local randomness, a named stream may initialize a specified generator such as PCG-family/xoshiro-style logic, but the algorithm, state transition and output mapping must be implemented explicitly rather than delegated to implementation-defined C++ distributions.

### Mutation entropy

Descendant generation derives a stream from parent genome identity + mutation seed + descendant index + gene/operator identity. Generating specimen 5 must not depend on whether specimens 0-4 were actually rendered.

This supports parallelism without changing results.

## Canonical serialization and identity

Genomes/projects use versioned human-readable structured text, initially JSON unless implementation evidence justifies another inspectable format.

Canonical genome serialization defines:

- stable key ordering;
- integer representation;
- normalized booleans/enums/strings;
- fixed policy for decimal/fixed-point values;
- stable operator instance IDs;
- omission/default rules;
- line ending/encoding rules for hashed canonical form.

Do not rely on map/hash-container iteration order.

A content identity is computed from canonical serialized state and, where needed, normalized source bytes. Windows built-in cryptographic APIs may be used for SHA-256 or equivalent content identity; cryptographic security is not required for artistic randomness but collision-resistant provenance is useful.

## Numeric determinism

Canonical structural operators should prefer integer or fixed-point arithmetic. Where interpolation, colour conversion or temporal modulation benefits from fractional values, define explicit fixed-point precision/rounding when practical.

If floating point is used canonically:

- do not depend on fast-math reassociation;
- specify clamping/rounding boundaries;
- avoid denormal/NaN-driven semantics unless explicitly normalized;
- add golden tests on supported CI targets;
- document the support boundary.

UI-only layout and non-authoritative display transforms may use ordinary floating point freely.

## Pipeline representation

v1 may execute a serial stack, but the persisted model should distinguish operator instance identity from list position. This supports later insertion, locking, lineage and possible typed graph expansion without corrupting identity.

Suggested conceptual structure:

```text
Genome
  schema_version
  engine_contract_version
  root_seed
  operators[]
    instance_id
    type_id
    type_version
    enabled
    parameters{}
```

Locks belong to exploration/session state rather than changing an operator's rendering semantics.

## Mutation model

Each parameter descriptor declares mutation metadata, for example:

- continuous/fixed-point range and scale;
- integer range;
- enum alternatives;
- bitmask mutation strategy;
- probability of mutation at radius bands;
- whether a parameter may be structurally regenerated.

Mutation radius is mapped through a documented policy to value perturbation and, at high levels, topology mutations.

Topology mutations must preserve valid pipelines. Insertion/removal/reorder chooses from compatible registered operators, validates the result, and remains deterministic.

## Crossover model

Crossover operates on stable operator/gene identities where possible instead of blindly cutting serialized text. It records both parents and the crossover seed/policy in lineage metadata.

Early v1 can use conservative crossover rules: inherit aligned operators by stable identity/type, choose compatible unmatched spans deterministically, then validate/migrate parameters.

## Lineage and history

Distinguish:

- **edit history** — reversible UI edits to the active project;
- **specimen lineage** — artistic parent/child relationships;
- **saved favourites/pins** — retained nodes independent of linear undo.

A descendant must not disappear merely because the user navigates undo history.

Lineage records IDs/provenance, not duplicate full-resolution pixels unless caching policy chooses to.

## Source identity

When loading an image:

1. decode with WIC into normalized canonical pixels;
2. validate dimensions/byte counts;
3. record source metadata/path as convenience;
4. compute identity from normalized bytes plus required dimensions/format metadata.

A moved source with identical normalized content can still satisfy provenance. A same-path source with different content must be detected.

## D3D11 presentation

D3D11/DXGI handles efficient display, scaling, thumbnails and UI compositing. Initially it uploads canonical CPU output.

If GPU execution is later added:

- canonical-equivalent operators need conformance tests against the CPU reference;
- non-equivalent GPU approximations must be visibly marked preview-only;
- export remains CPU canonical unless the user explicitly chooses a documented non-canonical acceleration mode in a future design.

## Proxy rendering

Large sources and specimen trays may use deterministic proxies to maintain interactivity. Proxy generation is an explicit preprocessing function keyed by source identity and proxy dimensions/method.

The UI must indicate proxy state. Full-resolution canonical render is performed for final export and optionally on active-specimen settle.

## Temporal/stateful engine

Temporal rendering advances in explicit integer ticks/frames. A stateful operator receives model-owned previous state and produces next state.

Do not tie semantic advancement to paint events, monitor refresh, `GetTickCount`, high-resolution timers or frame-delivery jitter.

For random access to long timelines, later work may add deterministic checkpoints. The initial correct implementation may replay from frame zero/checkpoint rather than compromise determinism.

## Persistence

Project state includes:

- project/schema version;
- source references and identities;
- active genome(s);
- specimen lineage and parent links;
- favourites/pins;
- gene/operator locks;
- relevant mutation policy/state;
- non-semantic UI state in a separately identifiable section.

Genomes are exportable separately from full projects.

Migrations are explicit functions between known schema versions. Unsupported future versions fail read-only/clearly rather than being partially interpreted.

## Risky codec/raw worker boundary

Actual malformed encoded data is processed outside the editor in a dedicated worker process.

The host should launch the worker under a Windows Job Object or equivalent bounded process-control scheme with:

- timeout/deadline;
- memory limit where practical;
- process termination on host close;
- small validated IPC messages;
- no shared raw pointers or in-process decoder fuzzing.

The worker returns only validated decoded pixel buffers/metadata, structured failure, or bounded diagnostics. A crash/timeout is an artistic outcome/failure mode, not an editor crash.

## Headless/batch architecture

The deterministic core must be callable without UI. A later headless executable can:

- load source + genome;
- render canonical output;
- generate deterministic descendants;
- calculate visual descriptors;
- write manifests/contact sheets;
- mine a search region.

This requirement is why application/session concerns must not leak into core execution.

## Concurrency

Parallel rendering is permitted only when task ordering cannot alter canonical bytes. Jobs should be independent by explicit seed/descendant/frame identity.

Caches are performance optimizations only. Cache presence/eviction must never change semantics.

## Failure model

Malformed projects, impossible dimensions, allocation failures, unsupported versions and worker failures return structured errors with context. They do not silently substitute different semantics.

An artistic fault can intentionally destroy image structure; an engineering fault must still be observable as an error.
