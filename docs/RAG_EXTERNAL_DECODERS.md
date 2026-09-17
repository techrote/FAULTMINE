# FAULTMINE external-decoder and laboratory boundary

This document extends `RAG_ARCHITECTURE.md` for malformed-codec and other external-decoder experiments. It is authoritative for FM-013 and any later feature that deliberately supplies unusual/corrupted encoded data to platform or third-party decoders.

## Core distinction

FAULTMINE has two different kinds of determinism and they must never be conflated.

### Canonical FAULTMINE transforms

The canonical CPU engine owns the algorithm, numeric rules, PRNG/stream derivation and logical memory semantics. For supported engine/schema/platform contracts, source + genome + seed + explicit tick determines canonical output.

### External-decoder experiments

A deterministic mutation may produce the exact same corrupted JPEG/PNG/etc. byte stream every time, but the response of an external/platform decoder to malformed data can vary by:

- Windows version;
- WIC codec implementation/version;
- installed codec component;
- security/robustness update;
- decoder error-recovery behaviour;
- implementation details outside FAULTMINE's contract.

Therefore, **deterministic corrupted input bytes do not imply canonical decoded pixels**.

FAULTMINE must report this truthfully rather than extending its reproducibility claim beyond code it controls.

## Isolation contract

Malformed encoded data is decoded only in a dedicated bounded worker process.

The host must provide process containment comparable to:

- Windows Job Object or equivalent lifecycle control;
- timeout/deadline;
- cancellation/termination;
- kill-on-host-close;
- practical memory/process limits;
- versioned, length-bounded, validated IPC;
- no raw pointers shared across the boundary.

Worker crash, hang, timeout and decode failure are structured experiment outcomes. They are never allowed to become editor crashes or silently corrupted host state.

## Freeze/materialize rule

When an external-decoder experiment produces useful pixels and the user wants to continue with them, FAULTMINE converts the validated returned pixels into a **materialized normalized source**.

The materialized source:

- uses the same validated canonical source-buffer representation as ordinary decoded sources;
- receives content identity from the normalized pixel bytes plus required dimensions/format metadata;
- becomes the source for downstream canonical FAULTMINE fault operators;
- is retained/embedded/referenced strongly enough that reopening the project does not need to re-run the unstable decoder path to claim reproducibility.

If the materialized pixels are not available, FAULTMINE may offer to rerun the external-decoder experiment, but it must describe the result as a new decoder experiment and compare the resulting normalized identity rather than assuming equality.

## Required provenance

A retained laboratory result should preserve, where available:

- identity/hash of the original encoded input;
- exact deterministic byte-mutation parameters/genome and root seed;
- identity/hash of the mutated encoded byte stream;
- worker protocol/application version;
- decoder family/identifier/version metadata that can be obtained safely;
- OS/build metadata relevant to decoder behaviour without collecting unnecessary personal machine data;
- outcome class: success, decode failure, crash, timeout, cancelled, rejected response;
- normalized materialized pixel identity when success is retained;
- timestamp only as non-semantic convenience metadata.

Export manifests must distinguish the laboratory derivation from the downstream canonical pipeline.

## UI contract

The UI must clearly distinguish:

- canonical FAULTMINE operator output;
- an external-decoder experiment in progress;
- crash/timeout/decode failure;
- a successfully decoded but not-yet-frozen laboratory result;
- a frozen/materialized source used by the canonical pipeline;
- rerunning the experiment versus reusing the frozen pixels.

A user should never have to infer whether a displayed result is expected to reproduce from canonical engine semantics or depends on an external decoder's recovery behaviour.

## Testing contract

CI must not depend on one malformed file decoding to exact pixels on every future Windows image.

Instead, test separately:

1. exact deterministic generation of corrupted encoded bytes;
2. worker containment and IPC using synthetic success/failure/crash/hang/malformed-response modes;
3. validation of any returned dimensions/payload before use;
4. materialization/freeze identity and project persistence;
5. provenance serialization;
6. canonical downstream rendering from frozen normalized pixels.

Real malformed-codec smoke tests may exist, but their decoder-dependent pixel output is not a canonical golden fixture unless a specific decoder/version is deliberately pinned as part of a narrower contract.

## Arbitrary binary input

Raw arbitrary-binary-to-image interpretation does **not** require the worker merely because the data is unusual. If FAULTMINE can interpret the bytes through its own bounded logical-buffer rules, that path belongs in the deterministic core and is canonical.

Use the worker only when an external decoder/parser or other risky subsystem genuinely creates a containment boundary.

## FM-013 implementation contract

FM-013 implements the boundary with two executables plus pure-core support:

- `FAULTMINE-lab-worker.exe` is the only new code path that feeds laboratory encoded bytes to WIC. The editor/host never performs that malformed decode in-process.
- `faultmine_laboratory_host` launches a fresh suspended worker for each experiment, assigns it to a one-process Windows Job Object before resuming it, applies kill-on-job-close and a practical job-memory limit, and terminates the job on cancellation or deadline expiry.
- IPC v1 is file-backed but structurally narrow: request/response control records have fixed magic, protocol version, message type and explicit little-endian lengths. Encoded bytes and returned RGBA payloads are bounded separately. No pointer, object address or native C++ struct representation crosses the process boundary.
- Host admission validates response control length, dimensions, tight RGBA8 stride, exact byte count and global pixel cap before constructing a canonical `ImageBuffer`. Crash, timeout, cancellation, decode failure and rejected-response remain distinct outcomes.
- Synthetic worker modes cover success, decode failure, crash, hang, oversized payload, malformed response, invalid dimensions, invalid stride and invalid byte count. They are the canonical CI containment fixtures; malformed real-codec pixels are deliberately not golden data.

### Deterministic encoded-byte mutation

The pure core owns the exact mutated encoded stream. `ByteMutationPlan` contains a 64-bit root seed, an explicit protected prefix and an ordered operation list. FM-013 supports deterministic bit flips plus explicit XOR, duplicate and drop ranges. Mutations are bounded to 64 MiB and reject any operation crossing the protected prefix or current stream extent. The canonical recipe, original encoded SHA-256 identity and mutated encoded SHA-256 identity are retained independently of decoder behaviour.

The protected-prefix setting is a user-controlled safety/artistic parameter, not a promise that the remaining bytes form a valid codec stream.

### Materialization and project persistence

A worker success is not canonical merely because its input mutation was deterministic. The host first validates and hashes the normalized returned pixels. Only then may it form a `MaterializedSource` whose provenance outcome is `success` and whose recorded materialized source identity exactly matches those pixels.

Project schema v2 gains an optional FM-013 `source.laboratory` extension. When present it embeds the frozen normalized RGBA8 pixels together with the external-decoder provenance. Ordinary v1/v2 projects remain accepted unchanged. Session save/load preserves the extension; downstream fault rendering continues to use the ordinary canonical source buffer. A project therefore does not need the original malformed encoded stream or another decoder run to substantiate its canonical downstream source identity.

The FM-012 export-manifest v1 grammar likewise has an optional FM-013 top-level `laboratory` extension. Exported pixels remain described by the existing canonical source/genome/frame fields; the laboratory block describes only how that normalized source was originally obtained. This is intentionally provenance, not part of genome identity.

### Advanced laboratory surface

`FAULTMINE-lab.exe` is the initial advanced/laboratory surface. It deliberately keeps risky work out of the normal editor path and prints the canonical/external distinction explicitly.

- `decode <encoded-input> <materialized.png> [seed] [flip-count] [protected-prefix]` deterministically mutates encoded bytes, runs the isolated worker, then on success writes the materialized PNG, an `.fmlabsource.json` frozen-source bundle and a provenance-bearing `.fmproj`.
- `reuse <bundle> <materialized.png>` reconstructs the normalized source from frozen pixels without launching the worker.
- `reuse-project <project.fmproj> <materialized.png>` reconstructs a project-embedded laboratory source without launching the worker.
- `raw ...` interprets arbitrary binary data through FAULTMINE's own checked logical rules and therefore remains a canonical in-process core operation.

A rerun is always a new external-decoder experiment even when the mutated encoded bytes are identical. Reuse means loading the frozen normalized pixels whose identity is already recorded.

### Canonical arbitrary-binary interpretation

`RawBinarySpec` explicitly controls width, optional/derived height, byte offset, stride, bytes-per-pixel format (`gray8`, `rgb8`, `rgba8`, `bgra8`) and `drop`, `wrap` or `fill` boundary policy. All row/pixel address arithmetic is checked before use, allocation goes through the canonical RGBA8 image constructor, and the laboratory pixel cap still applies. This path never calls WIC or the worker.
