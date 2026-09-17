# FAULTMINE external-decoder and laboratory boundary

This document extends `RAG_ARCHITECTURE.md` for malformed-codec and other external-decoder experiments. It is authoritative for FM-013 and any later feature that deliberately supplies unusual/corrupted encoded data to platform or third-party decoders.

## Core distinction

FAULTMINE has two different kinds of determinism and they must never be conflated.

### Canonical FAULTMINE transforms

The canonical CPU engine owns the algorithm, numeric rules, PRNG/stream derivation and logical memory semantics. For supported engine/schema/platform contracts, source + genome + seed + explicit tick determines canonical output.

### External-decoder experiments

A deterministic mutation may produce the exact same corrupted JPEG/PNG/etc. byte stream every time, but the response of an external/platform decoder to malformed data can vary by Windows version, WIC codec implementation, installed codec components, security updates, recovery behaviour and other implementation details outside FAULTMINE's contract.

Therefore, **deterministic corrupted input bytes do not imply canonical decoded pixels**. FAULTMINE reports this truthfully rather than extending its reproducibility claim beyond code it controls.

## FM-013 implemented boundary

FM-013 implements three deliberately separate pieces:

- `faultmine_core` owns deterministic bounded mutation of encoded bytes and deterministic arbitrary-binary interpretation.
- `FAULTMINE-lab-worker.exe` is the only process in the FM-013 path that invokes WIC on deliberately mutated encoded data.
- `FAULTMINE-lab.exe` is the explicit laboratory user surface. Its `decode` command labels the external-decoder phase, validates the worker result, freezes the normalized RGBA8 pixels, writes the frozen PNG plus project, and labels the transition to a materialized canonical source. Its `raw` command stays entirely on the deterministic core path.

The ordinary editor executable does not decode malformed laboratory bytes in-process.

## Isolation and IPC contract

The host creates each worker suspended, places it into a fresh Windows Job Object, then resumes it. The job enables kill-on-close, permits one active process, and applies an explicit process-memory limit. The host applies a deadline and supports cancellation; either condition terminates the entire job before returning a structured outcome.

Protocol v1 uses temporary files with a small binary control message rather than shared pointers or mapped editor state. The request includes magic, protocol version, message type, worker mode and exact encoded byte count. The response includes magic, protocol version, message type, outcome, dimensions, row stride, pixel byte count and length-prefixed diagnostic/decoder/OS strings. The host rejects unsupported versions/types, truncated messages, oversized metadata, invalid dimensions/stride/counts and pixel files whose actual length differs from the validated declaration.

Current explicit bounds are:

- encoded input: 64 MiB;
- normalized worker pixel payload: 256 MiB;
- diagnostic text: 16 KiB;
- one child process per worker job;
- default worker deadline: 5 seconds;
- default worker process-memory limit: 256 MiB.

Worker crash/non-zero exit, decode failure, timeout, cancellation and rejected response are separate `LaboratoryOutcome` values. A failed worker cannot commit canonical source pixels.

## Deterministic encoded mutation policy v1

`EncodedMutationPlan` records all deterministic byte operations and the safety envelope. Policy v1 applies operations in this order:

1. bounded XOR range;
2. deterministic seeded bit flips in the mutable region;
3. bounded duplication;
4. bounded drop.

`protected_prefix_bytes` prevents all policy-v1 mutation ranges and random bit flips from touching an explicitly protected prefix. Random bit flips use a named deterministic stream derived from the root seed, a fixed laboratory mutation instance identity, the `encoded-bit-flip-v1` purpose tag, flip ordinal, current encoded length and protected-prefix length. Mutation rejects invalid ranges, arithmetic overflow, excessive flip work and output-size overflow before modifying host state.

The mutation result records SHA-256 identities of both exact original encoded bytes and exact mutated encoded bytes. `encoded_mutation_plan_text` provides the stable policy/parameter record used in retained laboratory provenance.

## Freeze/materialize rule

A successful external result is still decoder-dependent until FAULTMINE validates and freezes its normalized RGBA8 pixels. `MaterializedLaboratorySource` pairs those canonical pixels with `LaboratoryProvenance`; validation requires the provenance materialized identity to equal the canonical source identity calculated from the actual pixel buffer.

After materialization:

- downstream operators receive the same canonical RGBA8 source representation as ordinary sources;
- project schema v3 embeds the validated frozen RGBA8 pixels and laboratory provenance in `laboratory_source`;
- the companion frozen PNG is an ordinary stable source asset for the existing editor path;
- reopening a v3 project can recover exact embedded pixels without invoking the external decoder;
- export manifest v2 propagates laboratory provenance separately from downstream canonical genome/derivation provenance.

Rerunning a malformed decoder is a new experiment. It is never silently treated as equivalent to reusing frozen pixels.

## Required retained provenance

A materialized FM-013 source carries:

- SHA-256 identity of the original encoded input;
- deterministic mutation root seed;
- stable policy-v1 mutation parameter text;
- SHA-256 identity of the exact mutated encoded stream;
- worker protocol version;
- worker/application version;
- decoder identifier/family metadata;
- non-personal OS/build metadata available to the worker;
- structured outcome and diagnostic text;
- normalized materialized source identity.

Only `success` is valid for a materialized source. Failure outcomes remain experiment results and are not project sources.

## Project and export schemas

Project schema v3 adds one required top-level `laboratory_source` field. Ordinary projects serialize it as `null`. A laboratory project stores width, height, exact RGBA8 bytes encoded as hexadecimal, and the strict provenance object. Parsers still accept v1/v2 projects and migrate them in memory with no laboratory source. Embedded pixel identity must equal `source.source_identity` or parsing fails.

Export manifest schema v2 adds one required `laboratory_provenance` field. Ordinary-source exports use `null`; laboratory-source exports carry the validated provenance object. Manifest v1 remains parseable for compatibility. Laboratory provenance materialized identity must equal the manifest source identity.

## Arbitrary binary input

Raw arbitrary-binary-to-image interpretation does **not** use the worker merely because the data is unusual. `RawBinarySpec` defines width, optional/derived height, offset, optional stride, one-to-four source bytes per pixel, boundary mode and fill byte. Checked address arithmetic and explicit dimension bounds prevent unsafe reads. Interpretation produces canonical RGBA8 pixels in the pure deterministic core and therefore participates in the normal canonical source-identity contract.

Boundary modes are `wrap`, `fill` and `drop`; `drop` maps an unavailable source pixel to transparent black rather than performing an out-of-range read.

## User-visible laboratory workflow

The advanced FM-013 command surface is intentionally separate from normal editing:

```text
FAULTMINE-lab decode <encoded-input> <materialized.png> [seed-u64] [bit-flips] [protected-prefix]
FAULTMINE-lab raw <binary-input> <output.png> <width> <height-or-0> <bytes-per-pixel> [offset] [stride]
```

`decode` prints an explicit `EXTERNAL DECODER` state before worker execution and `FROZEN/MATERIALIZED` only after host validation and source-identity assignment. On success it writes `<materialized.png>` and `<materialized.png>.fmproj`. The project uses the frozen PNG path and also embeds exact v3 pixels/provenance, so continuing work does not depend on reproducing malformed decoder behaviour.

## Testing contract

CI does not assert one malformed image must decode to fixed future pixels. FM-013 instead verifies independently:

1. exact seeded encoded-byte mutation vectors and protected regions;
2. bounded deterministic raw-binary interpretation;
3. synthetic worker success, decode failure, crash, hang/timeout and cancellation;
4. malformed header, oversized metadata, invalid dimensions, invalid stride and invalid byte-count rejection;
5. successful fresh worker operation after earlier crash/timeout/cancellation;
6. a normal known-valid PNG through the real WIC worker path as a control fixture;
7. freeze/materialized identity, project v3 persistence and reopen without worker execution;
8. export manifest v2 laboratory-provenance round-trip;
9. canonical downstream rendering/export from frozen normalized pixels.

Real malformed-codec experiments may vary by decoder environment; their pixels are not canonical golden fixtures unless a future narrower contract pins a decoder implementation/version explicitly.
