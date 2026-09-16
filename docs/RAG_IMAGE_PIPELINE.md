# FAULTMINE canonical image and starter-pipeline contract

This document records the visual execution contracts established by FM-003. It is authoritative for canonical still-image normalization, source identity, serial CPU pipeline execution, the starter fault operators and WIC still I/O until a later explicit versioned change supersedes them.

## Canonical still-image representation

FM-003 canonical still images use one format only:

- format: `RGBA8 UNORM`;
- channel byte order in memory: `R, G, B, A`;
- alpha: straight/unassociated alpha;
- bytes per pixel: `4`;
- canonical row stride: exactly `width * 4` bytes;
- rows are contiguous with no padding;
- width and height are non-zero unsigned 32-bit values;
- the complete canonical byte count must fit the checked signed/host-size contract before allocation.

The canonical core never type-puns these bytes into native pixel structs. Structural faults address explicit byte or pixel indices and resolve them through bounded policies before touching host memory.

## Normalized source identity

A source path is convenience metadata only. The canonical normalized source identity is SHA-256 of:

```text
ASCII bytes: FAULTMINE-SOURCE-RGBA8-v1
uint32 little-endian width
uint32 little-endian height
raw canonical RGBA8 bytes, row-major
```

There is no delimiter or NUL byte after the ASCII domain string; dimensions are fixed-width binary words.

The FM-003 representative `4x3` source fixture has identity:

```text
c428aed50243cec1d9847a3d677577caba1a4999068cfa6f22e91c178a04a6b8
```

Moving/copying an encoded source without changing its normalized pixels therefore preserves source identity. Different normalized pixels at the same path produce a different identity.

## WIC decode boundary

`faultmine_wic` is a platform I/O adapter, not part of the pure deterministic core.

For normal still-image loading it:

1. initializes/uses COM without changing an already-established compatible apartment;
2. opens the first WIC image frame;
3. obtains explicit dimensions;
4. rejects dimensions that cannot be copied through bounded WIC row APIs or the canonical allocation contract;
5. converts the frame through `IWICFormatConverter` to `GUID_WICPixelFormat32bppRGBA` with no dithering;
6. copies one row at a time into the canonical buffer;
7. computes source identity only after normalization.

No path, timestamp, EXIF field or decoder metadata participates in source identity.

Malformed-codec experiments remain governed by `RAG_EXTERNAL_DECODERS.md` and are **not** brought into the editor process by this ordinary loader.

## WIC PNG export

Canonical still export currently writes PNG through WIC using `GUID_WICPixelFormat32bppRGBA` and one-row writes.

The semantic contract is the decoded canonical pixel result, not byte-for-byte PNG container identity. WIC encoder implementation details or metadata may change encoded PNG bytes without changing the canonical pixels. Tests therefore export and decode again and compare canonical pixels/identity.

## Serial CPU pipeline

The canonical pipeline is an ordered serial transform over `ImageBuffer` values.

Before execution:

- the source buffer is validated;
- the genome is validated through the FM-002 `OperatorRegistry` contract;
- each enabled operator must have a registered CPU executor.

Execution rules:

- disabled operators are exact no-ops and do not consume entropy;
- every enabled starter operator receives immutable input and produces a new canonical output buffer;
- starter operators preserve width, height and canonical pixel format;
- execution errors return structured stack index/type/message data rather than partially accepting a buffer;
- no window, D3D device, wall clock or global mutable RNG participates.

`FaultRegistry` layers executor functions on top of the existing FM-002 descriptor registry rather than replacing the persisted operator/schema model.

## Boundary policy

Starter addressing operators use explicit read-boundary policies:

- `wrap` — modulo the logical extent;
- `clamp` — select the nearest valid first/last element;
- `fill` — an out-of-range read produces zero bytes (`0,0,0,0` for a whole missing pixel).

There is no native out-of-bounds read. Later catalogues may add richer policies without changing these v1 meanings.

## Starter operators

All starter operator type versions are `1`.

### `fault.row-offset`

Parameters:

- `amount` — `i64` pixel displacement;
- `boundary` — `string`: `wrap`, `clamp`, or `fill`.

Positive displacement moves visible content to the right: destination `x` reads logical source `x - amount`.

### `fault.stride-delta`

Parameters:

- `delta_bytes` — `i64` added to the canonical physical row stride to form a logical row stride;
- `boundary` — `string`.

Each destination byte reads from:

```text
logical_row_origin + x * 4 + channel
```

where each next logical row origin advances by `canonical_stride + delta_bytes`. All signed additions are checked; overflow is an execution error.

### `fault.address-xor`

Parameters:

- `mask` — `u64`;
- `boundary` — `string`.

For each destination pixel index `p`, the logical source pixel index is `p XOR mask`, then resolved through the selected boundary policy.

### `fault.channel-permute`

Parameter:

- `order` — four-character `string` containing each of `r`, `g`, `b`, `a` exactly once.

Output channel positions read source channels in the supplied order. Example `bgra` swaps red and blue while preserving green/alpha.

### `fault.byte-xor`

Parameters:

- `mask` — `u64` restricted to `0..255`;
- `channels` — non-empty unique subset of `rgba`.

Selected channel bytes are XORed by the mask independently.

### `fault.bit-rotate`

Parameters:

- `amount` — `u64` restricted to `0..7`;
- `channels` — non-empty unique subset of `rgba`.

Selected bytes are rotated left within eight bits by the requested amount.

### `fault.scanline-jitter`

Parameters:

- `max_shift` — `u64` not exceeding the signed range contract;
- `boundary` — `string`.

Each row `y` independently derives the named stream:

```text
root seed
+ operator instance ID
+ purpose tag "scanline-jitter-row"
+ identity word [y]
```

A shift is selected uniformly from the inclusive range `[-max_shift, +max_shift]`. Positive shift moves visible row content right.

Because row entropy is derived from row identity, row execution order or unrelated stream consumption cannot change the result.

## Representative golden pipeline

The FM-003 multi-operator fixture applies, in order:

1. row offset `+1`, wrap;
2. channel order `bgra`;
3. byte XOR `0x0f` on `r` and `b`;
4. pixel-address XOR mask `1`, wrap;
5. scanline jitter `max_shift=2`, wrap.

With root seed `0123456789abcdef` and the fixture instance identities retained in the automated test, the resulting canonical image identity is:

```text
348ac78d545e84a596a7721483dee33eca27be6ca4221170d8741d07424d5dd2
```

Debug and Release builds must retain this result unless an explicit versioned semantic correction is accepted.

## Non-GUI render hook

`FAULTMINE-render.exe` is the FM-003 developer/test render hook:

```text
FAULTMINE-render <input-image> <genome.json> <output.png>
```

It:

1. loads the starter registry;
2. parses/validates the genome using the FM-002 canonical schema;
3. normalizes the source through WIC;
4. executes the canonical CPU pipeline;
5. exports canonical PNG;
6. prints normalized source and output image identities.

This is intentionally narrow. FM-014 later owns the full headless batch/mining CLI.

## Compatibility rule

Changes that alter canonical pixel format, source identity bytes, starter operator type/version semantics, boundary meanings, named jitter purpose/identity words, or the representative golden output are contract changes and require explicit compatibility/version reasoning rather than a mechanical fixture update.
