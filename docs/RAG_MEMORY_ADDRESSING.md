# FAULTMINE memory and addressing fault contract

This document records the canonical memory/addressing semantics established by FM-005. It complements `RAG_IMAGE_PIPELINE.md` and is authoritative for the shared logical-address boundary contract and the FM-005 operator family until an explicit versioned change supersedes it.

## Safety model

FAULTMINE never performs real invalid memory access to create glitch art. Memory faults are simulations over bounded logical indices. Every logical address is resolved through an explicit policy before any host-memory access occurs.

The canonical image allocation contract already limits total RGBA8 bytes to the signed/host-size range. FM-005 additionally uses checked signed arithmetic wherever a persisted signed displacement participates in an address calculation.

## Shared boundary policies

`faultmine/logical_address.hpp` defines the shared v1 policies used by FM-005 and the refactored FM-003 addressing operators:

- `wrap` — modulo the logical extent;
- `clamp` — select the first or last valid element;
- `fill` — the logical read is absent and the operator leaves/emits zero RGBA bytes as documented.

`resolve_displaced_index(base, displacement, extent, policy)` means:

```text
logical source = base - displacement
```

Positive displacement therefore moves visible content toward larger destination indices. The helper handles `INT64_MIN` without negating it and has exact/property tests against a slow safe reference.

No implicit mirror policy exists in v1.

## Registry composition

FM-003's `make_starter_fault_registry()` remains available so its original golden harness is compatibility-focused and unchanged.

`make_default_fault_registry()` composes:

1. FM-003 starter faults;
2. the FM-005 memory/addressing family.

The non-GUI `FAULTMINE-render.exe` uses the default registry, so FM-005 genomes can be rendered without D3D11 or the interactive application.

## Mutation descriptor hints

FM-005 extends non-persisted operator descriptor metadata with `MutationDomain`:

- `opaque`;
- `toggle`;
- `signed_range`;
- `unsigned_range`;
- `choice`;
- `bitmask`.

The metadata also carries bounded range/step hints and canonical choice lists where appropriate. It is **advisory search metadata**, not render semantics and not part of canonical genome JSON or genome identity. FM-009 may use these hints to implement deterministic radius-aware mutation; executors remain responsible for validating actual persisted parameter values.

All FM-005 parameters have explicit policy version `1` and non-opaque mutation domains.

## Existing stride primitive

`fault.stride-delta` remains the canonical wrong-pitch primitive from FM-003. FM-005 does not duplicate it under a new type ID.

Its semantics remain:

```text
logical stride = canonical byte stride + delta_bytes
```

Each destination byte reads from the corresponding logical row origin, resolved through `wrap`, `clamp`, or `fill`. Signed additions are checked. FM-005 adds focused smaller/equal/larger pitch regression coverage while preserving the FM-003 goldens.

## `fault.address-offset` v1

Parameters:

- `offset_pixels` — `i64`;
- `boundary` — `wrap|clamp|fill`.

For destination linear pixel index `p`:

```text
source = p - offset_pixels
```

The displacement helper resolves the result safely. This is deliberately linear across row boundaries and therefore differs from `fault.row-offset`.

## `fault.address-mask` v1

Parameters:

- `xor_mask` — `u64`;
- `and_mask` — `u64`;
- `or_mask` — `u64`;
- `boundary` — `wrap|clamp|fill`.

For destination linear pixel index `p`:

```text
logical = ((p XOR xor_mask) AND and_mask) OR or_mask
```

The resulting unsigned logical address is then resolved through the selected boundary policy. This supports adjacent-address swaps, stuck address bits, forced-high bits and combined bit faults without native pointer manipulation.

## `fault.coordinate-remap` v1

Parameters:

- `x_offset`, `y_offset` — `i64`;
- `x_xor_mask`, `y_xor_mask` — `u64`, execution-valid only when each fits `uint32`;
- `swap_xy` — `bool`;
- `boundary` — `wrap|clamp|fill`.

For each destination coordinate, the optional X/Y swap happens first, then coordinate XOR masks, then signed displacements. X and Y are independently resolved against source width/height.

On non-square images, `swap_xy` therefore has explicit boundary-dependent crop/wrap/fill behavior rather than pretending to be a geometric transpose that changes dimensions.

## `fault.tile-permute` v1

Parameters:

- `tile_width`, `tile_height` — non-zero `u64` values that must fit `uint32`;
- `boundary` — `wrap|clamp|fill`.

The source is divided into a ceil-rounded tile grid. A named stream is derived from:

```text
root seed
+ operator instance ID
+ purpose "tile-affine-permutation"
+ [tile_width, tile_height, tiles_x, tiles_y]
```

The stream deterministically chooses an affine permutation of tile indices:

```text
sourceTile = (a * destinationTile + b) mod tileCount
```

where `a` is advanced deterministically until it is coprime with `tileCount`. Modular multiplication is overflow-safe and requires no shuffle table allocation.

Pixel-local coordinates are retained inside the selected source tile. Right/bottom partial source tiles use the common boundary policy for local coordinates that land beyond canonical width/height. Destination partial tiles simply stop at the real destination edge.

## `fault.band-repeat` v1

Parameters:

- `band_height` — non-zero `u64` fitting `uint32`;
- `every` — non-zero `u64`;
- `source_delta_bands` — `i64`;
- `boundary` — `wrap|clamp|fill`.

Every `every`-th destination band reads from:

```text
sourceBand = destinationBand - source_delta_bands
```

Non-selected bands pass through unchanged. Local row position within the band is retained. A positive delta therefore repeats an earlier band; negative deltas can pull later bands forward. Partial final bands use the common boundary policy.

## `fault.address-burst` v1

Parameters:

- `burst_count` — `u64`, maximum `65536`;
- `burst_length_pixels` — `u64`, not greater than the current canonical pixel count;
- `max_offset_pixels` — `u64`, at most `INT64_MAX/2`;
- `boundary` — `wrap|clamp|fill`.

Each burst index independently derives:

```text
root seed
+ operator instance ID
+ purpose "address-burst"
+ [burst_index]
```

That stream selects one start pixel and one signed displacement uniformly from `[-max_offset_pixels,+max_offset_pixels]`. The burst is truncated at the end of the destination image. Overlapping bursts are applied in increasing burst-index order; each reads from the immutable operator input, so later bursts deterministically win at overlapping destination pixels.

The `65536` burst cap is part of v1's bounded-work safety contract.

## Determinism and scheduling

Random tile/burst behavior is derived only from FM-002 named streams. Unrelated random consumption, render scheduling and UI repaint order cannot change these outputs.

All FM-005 operators remain pure CPU-reference transforms and are testable without Win32, WIC or D3D11.

## Compatibility

Changing any type ID, type version, transform order, boundary meaning, purpose tag, identity-word sequence, tile-affine derivation, burst overlap order or bounded-work limit is a semantic contract change. It requires explicit version/compatibility reasoning rather than a mechanical golden update.
