# FAULTMINE deterministic colour, palette, LUT, quantisation and dither contract

This document records the canonical colour semantics established by FM-007. It extends the default CPU fault catalogue without changing the FM-002 genome/entropy contract, FM-003 canonical RGBA8 representation, FM-005 addressing rules, or FM-006 representation/bit semantics.

## Principle

Colour stages are FAULTMINE's intentionally controlled counterpart to structural corruption. Their purpose is to constrain wild memory/representation faults into coherent visual families while preserving exact reproducibility.

Canonical colour execution is CPU-side integer arithmetic. D3D sampler filtering, monitor profiles, ICC state, OS colour management and GPU shader precision are not part of the result.

All operator type versions introduced here are `1`.

## Palette asset v1

`Palette` is a versioned ordered list of 1..256 straight RGBA8 entries.

Canonical UTF-8 JSON is:

```json
{"schema_version":1,"entries":["000000ff","ff0000ff","ffffffff"]}
```

followed by exactly one LF. Each entry is exactly eight lower-case hexadecimal digits in `rrggbbaa` order when serialized canonically. Parsing accepts upper/lower-case hexadecimal and JSON field order permitted by the strict shared JSON parser; reserialization emits the one canonical form.

Palette identity is lower-case SHA-256 of the complete canonical palette bytes including the final LF. Ordering is semantic: two palettes containing the same entries in another order have different identities.

FM-007 supports embedded palette assets in genome string parameters. Runtime external-path references are deliberately not introduced before the project/asset model in FM-008. A `.fmpal` file is simply the same canonical palette JSON text; import means parse + validate + canonicalize, and changed file content therefore yields a changed palette identity instead of silently preserving provenance.

## LUT asset v1

`Lut256` contains exactly four 256-byte lookup tables in canonical RGBA channel order.

Canonical JSON contains:

```text
schema_version = 1
channels.r = 512 hex digits
channels.g = 512 hex digits
channels.b = 512 hex digits
channels.a = 512 hex digits
```

The serialized key order is fixed `schema_version`, then `channels`, then `r,g,b,a`, followed by one LF. LUT identity is SHA-256 of those canonical bytes.

The LUT is embedded in a genome string parameter in FM-007. Like palettes, it has no runtime path dependency yet.

## RGBA colour literals

Generator endpoints use exactly eight hexadecimal digits `rrggbbaa`. Canonical formatting is lower-case.

## Nearest-palette metric

### `colour.palette-nearest`

Parameters:

- `palette` — embedded palette JSON;
- `alpha_mode` — `preserve` or `map`.

With `preserve`, distance is squared Euclidean RGB:

```text
(R-Pr)^2 + (G-Pg)^2 + (B-Pb)^2
```

and source alpha is retained. With `map`, squared alpha distance is added and the chosen palette alpha is emitted.

Distances are exact integers. Equal-distance ties choose the lowest palette index. No perceptual/display-profile conversion is performed.

## Luminance gradient

### `colour.gradient-map`

Parameters:

- `palette` — ordered control colours;
- `alpha_mode` — `preserve` or `map`.

Canonical luminance is:

```text
Y = (54*R + 183*G + 19*B + 128) >> 8
```

The coefficients sum to 256 and approximate Rec.709 weighting while remaining exact integer arithmetic.

For `N > 1` control colours:

```text
scaled    = Y * (N-1)
segment   = scaled / 255
remainder = scaled % 255
```

Adjacent control bytes are interpolated using:

```text
(left*(255-remainder) + right*remainder + 127) / 255
```

which is integer round-half-up at the 255 denominator. `Y=0` and `Y=255` hit the first and last entries exactly. A one-entry palette emits that entry. Preserve/map alpha follows the same rule as nearest-palette mapping.

## Per-channel LUT

### `colour.lut`

Parameter:

- `lut` — embedded canonical LUT asset.

Each canonical RGBA byte is replaced by `lut[channel][inputByte]`. Alpha participates as the fourth explicit table.

## Channel quantisation

### `colour.quantize`

Parameters:

- `levels` — `2..256`;
- `channels` — non-empty unique subset of `rgba`.

For a selected byte `v`:

```text
maximum = levels - 1
index   = (v*maximum + 127) / 255
output  = (index*255 + maximum/2) / maximum
```

Both divisions are integer round-half-up under the stated positive denominators. Unselected channels pass through exactly. `levels=256` is therefore identity.

## Ordered Bayer dither

### `colour.dither-ordered`

Parameters:

- `levels` — `2..256`;
- `channels` — selected channels;
- `matrix` — `bayer2`, `bayer4`, or `bayer8`.

The accepted Bayer rank matrices are fixed in code/tests. For rank `b` in an `N x N` matrix:

```text
threshold = ((2*b + 1) * 255) / (2*N*N)
scaled    = v * (levels-1)
base      = scaled / 255
remainder = scaled % 255
```

If `remainder > threshold` and `base` is not already the highest level, the index increments once. The final level is expanded using the same exact quantised-value formula above.

Matrix lookup is by canonical destination `(x mod N, y mod N)`. No display scaling or thread scheduling participates.

## Named-noise dither

### `colour.dither-noise`

Parameters:

- `levels` — `2..256`;
- `channels` — selected channels.

Each selected pixel/channel derives its own FM-002 stream from:

```text
root seed
+ operator instance ID
+ purpose "colour-noise-dither"
+ [linear_pixel_index, channel_index, levels]
```

One unbiased `uniform_below(255)` value gives threshold `0..254`; the same remainder/threshold rule as ordered dither selects the output level. Consequently execution order, thread scheduling, and unrelated stream consumption cannot change output.

## Deterministic palette generator

`generate_palette_ramp` accepts:

- start/end RGBA8 control colours;
- `count` in `1..256`;
- RGB jitter in `0..255`;
- root seed and stable operator/owner instance ID.

The base ramp interpolates each RGBA channel with integer round-half-up across `count-1` intervals. Start/end entries are exact and never jittered. For every interior RGB channel, a separate stream is derived from:

```text
root seed
+ instance ID
+ purpose "palette-ramp-jitter"
+ [entry_index, rgb_channel_index, count, jitter]
```

The signed offset is chosen uniformly from `[-jitter,+jitter]` and clamped to `0..255`. Alpha is interpolated but not jittered.

### `colour.generated-palette-map`

Parameters:

- `start`, `end` — `rrggbbaa`;
- `count` — `1..256`;
- `jitter` — `0..255`;
- `alpha_mode` — `preserve|map`.

It generates the palette using the operator's root-seed/instance identity, then applies the exact nearest-palette metric above.

## Mutation metadata

FM-007 extends non-persisted `MutationDomain` with:

- `colour_rgba` — structured RGBA colour values;
- `palette` — ordered palette assets;
- `lut` — per-channel 256-entry LUT assets.

Numeric counts/levels use bounded integer ranges and enumerated modes/channel presets use choice domains. These descriptors are search/editor metadata only; they are not serialized into genome identity.

## Catalogue composition

`make_default_fault_registry()` composes:

1. FM-003 starter faults;
2. FM-005 memory/addressing faults;
3. FM-006 representation/bit faults;
4. FM-007 colour/LUT faults.

`make_starter_fault_registry()` remains frozen for FM-003 compatibility tests.

## Human-editable asset ergonomics

`examples/fm007-rust.fmpal` is an owned FAULTMINE palette file using the canonical palette JSON format. The public pure-core parse/serialize/identity functions are the import/export boundary; FM-008 may expose those functions through the generic editor without changing the file format.

## Compatibility and verification

The `colour_lut_contracts` suite freezes:

- palette/LUT parse, canonical serialization, and SHA-256 identities;
- changed palette-content identity;
- malformed asset rejection;
- palette-generator known-answer values;
- nearest-colour tie-breaking;
- luminance/gradient endpoints and alpha behavior;
- per-channel LUT application;
- quantisation boundary values;
- Bayer matrix positions;
- named-noise dither exact output and stream isolation;
- generated-palette mapping;
- FM-005 structural + FM-007 colour composition;
- canonical genome save/parse/rerender;
- disabled no-op semantics;
- typed mutation domains.

FM-007 does not alter any accepted FM-002 through FM-006 operator semantics or golden fixture. A future change to asset bytes, identity, luminance weights, interpolation/quantisation rounding, Bayer matrices, purpose tags, identity words, palette-generator jitter rules, or operator type/version semantics requires explicit compatibility/version reasoning rather than a mechanical golden update.
