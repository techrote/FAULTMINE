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
