# FAULTMINE representation and bit-fault contract

This document records the representation/bit semantics established by FM-006. It extends the canonical CPU fault catalogue without changing the FM-002 entropy/genome contract, FM-003 RGBA8 canonical representation, or FM-005 bounded logical-address rules.

## Principle

These operators model **data being interpreted under the wrong representation**. They never use native type punning, alignment assumptions, host byte order, signed overflow, or undefined behavior as an artistic mechanism.

All operator type versions introduced here are `1`.

## Catalogue composition

`make_default_fault_registry()` now composes:

1. FM-003 starter faults;
2. FM-005 memory/addressing faults;
3. FM-006 representation/bit faults.

The compatibility-focused `make_starter_fault_registry()` remains unchanged, so FM-003 golden fixtures still test their original accepted catalogue boundary.

## Channel representation

### `fault.channel-route`

Parameter:

- `routes` — four symbols, one for each output `R,G,B,A` position.

Each symbol is one of:

- `r`, `g`, `b`, `a` — copy that source channel;
- `0` — constant zero;
- `1` — constant 255.

Duplicates are intentionally allowed. Therefore this one primitive covers all channel permutations, duplication, drop/fill and alpha replacement.

Examples:

- `bgra` swaps red/blue;
- `rr01` emits source red into R/G, zero into B, and 255 into A.

### `fault.channel-offset`

Parameters:

- `channels` — non-empty unique subset of `rgba`;
- `dx`, `dy` — signed pixel displacement;
- `boundary` — FM-005 `wrap`, `clamp`, or `fill`.

For selected channels, destination `(x,y)` reads the same channel from logical source `(x-dx, y-dy)`. Unselected channels remain at their original destination pixel. Fill reads produce zero only in selected channels.

Displacements use the shared overflow-safe FM-005 resolver, including `INT64_MIN`.

## Logical word byte lanes

### `fault.word-lanes`

Parameters:

- `word_bytes` — exactly `2` or `4`;
- `mode` — `reverse`, `rotate-left`, or `rotate-right`.

The canonical byte stream is divided into consecutive logical words. Bytes are rearranged by index only; no native integer loads/stores occur.

Examples:

```text
2-byte reverse:  [01 02] -> [02 01]
4-byte reverse:  [01 02 03 04] -> [04 03 02 01]
4-byte rotate-L: [01 02 03 04] -> [02 03 04 01]
```

Because canonical RGBA8 byte length is a multiple of four, no partial word exists for the accepted word widths.

## Explicit 16-bit packed reinterpretation

### `fault.packed-reinterpret`

Parameters:

- `source_format` — `rgb565`, `bgr565`, `rgba4444`, or `argb1555`;
- `source_endian` — `little` or `big`;
- `interpret_format` — same format set;
- `interpret_endian` — `little` or `big`.

Each canonical pixel is processed independently:

1. its RGBA bytes are quantized/packed into the selected 16-bit **source layout**;
2. that logical word is materialized as two explicit bytes using `source_endian`;
3. those two bytes are assembled into a new logical 16-bit word using `interpret_endian`;
4. the new word is decoded using `interpret_format` into canonical RGBA8.

No host-endian or alignment behavior participates.

### Layouts

```text
rgb565:   RRRRR GGGGGG BBBBB
bgr565:   BBBBB GGGGGG RRRRR
rgba4444: RRRR GGGG BBBB AAAA
argb1555: A RRRRR GGGGG BBBBB
```

Packing truncates low bits:

- 8 -> 5 bits: `v >> 3`;
- 8 -> 6 bits: `v >> 2`;
- 8 -> 4 bits: `v >> 4`;
- ARGB1555 alpha: `A >= 128` becomes `1`, otherwise `0`.

Expansion is exact integer bit replication:

```text
4 -> 8: (v << 4) | v
5 -> 8: (v << 3) | (v >> 2)
6 -> 8: (v << 2) | (v >> 4)
1 -> 8: 0 or 255
```

Representative endian mismatch:

```text
RGBA = ff 00 ff ff
pack rgb565 little -> word f81f -> bytes 1f f8
read those bytes as big-endian rgb565 -> word 1ff8
output = 18 ff c6 ff
```

## Planar/interleaved disagreement

### `fault.planar-layout`

Parameter `mode`:

- `interleaved-as-planar`;
- `planar-as-interleaved`.

For `N` pixels, a planar stream is defined as `N` R bytes, then `N` G bytes, then B, then A.

`interleaved-as-planar` treats the existing canonical raw byte stream as though it already had that four-plane layout.

`planar-as-interleaved` first defines the conceptual plane stream from canonical source channels, then consumes those bytes four-at-a-time as RGBA pixels.

No padding or implicit stride is inserted.

## Signed-byte misunderstandings

### `fault.signed-byte`

Parameters:

- `channels` — selected channels;
- `mode` — `bias-flip`, `absolute-signed`, or `clamp-negative`.

Two's-complement signed interpretation is computed explicitly:

```text
s = byte < 128 ? byte : byte - 256
```

Modes:

- `bias-flip`: `byte XOR 0x80`;
- `absolute-signed`: output `abs(s)` (`-128 -> 128`);
- `clamp-negative`: negative signed values become zero; non-negative values retain magnitude.

No conversion to implementation-defined native `signed char` occurs.

## Bit faults

FM-003 already provides:

- `fault.byte-xor` for selected-channel XOR masks;
- `fault.bit-rotate` for exact 8-bit rotate-left.

FM-006 adds the following complementary primitives.

### `fault.bit-shift`

Parameters:

- `channels`;
- `direction` — `left` or `right`;
- `amount` — `0..8`.

Shifts use zero fill. Shift by exactly eight yields zero. Left shifts use a wider unsigned intermediate then truncate to the low eight bits.

### `fault.nibble-swap`

Selected byte:

```text
abcd efgh -> efgh abcd
```

Equivalent integer rule: `(v << 4) | (v >> 4)`, restricted to one byte.

### `fault.bitplane-swap`

Parameters:

- `channels`;
- `plane_a`, `plane_b` — each `0..7`, where zero is the least-significant bit.

The two selected bits exchange positions independently in every selected byte. Equal plane indices are a defined no-op.

### `fault.stuck-bits`

Parameters:

- `channels`;
- `zero_mask` — byte mask for bits forced to zero;
- `one_mask` — byte mask for bits forced to one.

Masks must be `0..255` and must not overlap. Result:

```text
(value & ~zero_mask) | one_mask
```

Together with FM-003 `byte-xor`, this provides explicit AND/OR/stuck/XOR-style mask behavior without one ambiguous mega-operator.

## Structured deterministic bit bursts

### `fault.bit-burst`

Parameters:

- `block_width`, `block_height` — positive logical pixel dimensions;
- `burst_count` — `0..65536` executor limit;
- `xor_mask` — `0..255`;
- `channels` — selected channels.

The image is partitioned into a logical grid of blocks. Right/bottom edge blocks are clipped to the real image dimensions.

Each burst `i` independently derives:

```text
root seed
+ operator instance ID
+ purpose "bit-burst-block"
+ [i, grid_width, grid_height, block_width, block_height]
```

The stream selects one block uniformly. Every selected channel byte in that entire block is XORed by `xor_mask`. Repeated selection of the same block is intentionally cumulative, so an even number of identical XOR bursts may cancel.

Burst `i` does not depend on generation/execution of bursts `0..i-1` through shared RNG state; each burst has its own named stream derivation.

### Bounded work

Persisted hostile parameters are bounded before loops execute:

- block dimensions must fit positive `uint32` range;
- `burst_count <= 65536`;
- total block pixel visits must not exceed 64 Mi pixel visits for one operator execution.

These are safety/resource limits, not artistic host-memory shortcuts.

## Mutation metadata

Every FM-006 parameter has a non-opaque typed mutation domain:

- integer offsets/counts/indices -> signed or unsigned range;
- enumerated layouts/modes/endian/channel presets -> choice domains;
- stuck/XOR masks -> bitmask domains.

Mutation descriptors are registry metadata. They are not serialized into genomes and therefore do not change canonical output identity by themselves.

The channel-route mutation domain contains all 24 pure RGBA permutations plus representative duplicate/drop/fill routes. The executor accepts every syntactically valid four-symbol route even when it is not part of that curated mutation set.

## Compatibility and verification

FM-006 must not alter accepted FM-002/FM-003/FM-004/FM-005 output semantics. Existing golden tests remain authoritative.

The dedicated `representation_bit_contracts` suite covers:

- all 24 channel permutations plus duplication/drop/fill;
- selected-channel spatial offsets and extreme signed displacement;
- 2/4-byte lane ordering;
- packed-format and endian known answers;
- both planar/interleaved disagreement directions;
- signed-byte endpoint mappings;
- shift amounts including 0/8/out-of-range;
- nibble/bitplane/stuck-bit vectors;
- deterministic structured bit-burst blocks, isolation and work caps;
- default-registry composition;
- canonical genome serialization/parse/rerender;
- disabled no-op semantics;
- typed mutation metadata.

Any future semantic change that alters these accepted outputs requires an explicit operator-version/compatibility decision rather than a mechanical golden update.
