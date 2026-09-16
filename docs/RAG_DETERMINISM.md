# FAULTMINE deterministic substrate contract

This document records the byte-level contracts established by FM-002. It is authoritative for genomes, root seeds, operator instance identities, named deterministic entropy and canonical genome identity until an explicit versioned migration supersedes a rule.

## Contract versions

- genome schema version: `1`
- engine contract version: `1`
- entropy contract version: `1`

Changing a rule below in a way that changes persisted meaning, deterministic random vectors, canonical bytes or genome identity requires an explicit version transition and compatibility decision. Refactors must not silently move the known-answer vectors.

## Root seed

A root seed is an unsigned 64-bit integer. Persisted text uses exactly 16 hexadecimal digits. Parsing accepts upper- or lower-case hexadecimal; canonical formatting is lower-case with leading zeros.

Example:

```text
0123456789abcdef
```

No root seed is generated implicitly while loading a genome.

## Operator instance identity

An operator instance ID is 128 bits represented as two unsigned 64-bit words (`high`, `low`). Persisted text uses exactly 32 hexadecimal digits (`high` then `low`), canonically lower-case with leading zeros.

Instance identity is distinct from operator list position. Reordering an operator does not regenerate its ID. Loaded IDs are data and are never replaced implicitly.

Future topology mutation may create child IDs through the deterministic derivation function documented below.

## Integer mixer and purpose tags

### `mix64`

`mix64` is the SplitMix64 finalizer, operating with unsigned 64-bit wraparound:

```text
x ^= x >> 30
x *= 0xbf58476d1ce4e5b9
x ^= x >> 27
x *= 0x94d049bb133111eb
x ^= x >> 31
```

Known answers:

```text
mix64(0) = 0000000000000000
mix64(1) = 5692161d100b05e5
```

### purpose-tag hash

Purpose strings are hashed byte-for-byte as UTF-8 with FNV-1a-64:

```text
offset basis = 14695981039346656037
prime        = 1099511628211
```

For `scanline` the result is `f162a4628b7e1992`.

## Named stream derivation

Named entropy is derived from stable semantic identity rather than from consumption of one process-global generator.

Given:

- root seed `R`;
- operator instance words `H`, `L`;
- purpose tag `P`;
- zero or more ordered 64-bit semantic identity words `W[i]`;

compute:

```text
x = mix64(R xor 0x4641554c544d494e)   // ASCII-domain constant "FAULTMIN"
x = mix64(x xor H)
x = mix64(x xor L)
x = mix64(x xor FNV1a64(P))
for each W[i] in semantic order:
    x = mix64(x xor W[i])
```

The final `x` is the named stream seed.

Known-answer tuple:

```text
root       = 0123456789abcdef
instance   = 00112233445566778899aabbccddeeff
purpose    = scanline
words      = [7, 11]
streamSeed = e878b6e8cc098808
```

Unrelated named streams are instantiated independently. Consuming values from one stream cannot perturb another stream.

## SplitMix64 stream

A local deterministic stream stores one unsigned 64-bit state. Every output performs:

```text
state += 0x9e3779b97f4a7c15
output = mix64(state)
```

For the named stream seed `e878b6e8cc098808`, the first five outputs are:

```text
90c745c6e92dd49d
1be54033185387d8
242ab2c766145b60
33477970e976f62f
70a847fcd8f86b2d
```

`std::random_device`, `std::uniform_*` and implementation-defined distributions are not part of the canonical contract.

## Bounded integers

`uniform_below(n)` requires `n > 0` and uses rejection sampling, not modulo-only bias:

```text
threshold = (0 - n) mod n       // unsigned 64-bit arithmetic
repeat:
    r = next_u64()
until r >= threshold
return r mod n
```

For the known stream above and `n = 10`, the first ten results are:

```text
7, 4, 4, 9, 9, 1, 5, 8, 9, 7
```

Closed ranges use this mapping except the full `[0, UINT64_MAX]` range, which consumes one raw stream value directly.

## Deterministic child instance IDs

`derive_instance_id(root, parent, purpose, ordinal)` derives two named seeds using identity words:

```text
high: [ordinal, 0x49445f48495f3031]  // "ID_HI_01"
low:  [ordinal, 0x49445f4c4f5f3031]  // "ID_LO_01"
```

Known answer:

```text
root    = 0123456789abcdef
parent  = 00112233445566778899aabbccddeeff
purpose = child-instance
ordinal = 42
child   = b42a435c13b5b6269cba56a7fd72f5e6
```

Later mutation policies must define what semantic value becomes `ordinal`; they must not substitute execution order or container address.

## Genome v1 model

A genome contains only canonical rendering semantics:

```text
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

Locks, favourites, lineage, window state and other exploration/project state are not genome fields.

### Parameter values

FM-002 canonical parameter primitives are:

- `bool`
- signed 64-bit integer (`i64`)
- unsigned 64-bit integer (`u64`)
- UTF-8 string (`string`)

Canonical fractional operator semantics should normally be defined later as explicitly scaled/fixed-point integers. Adding another canonical primitive requires a schema/contract decision.

Values are persisted with an explicit kind tag so `i64(7)` and `u64(7)` cannot silently collapse into one JSON numeric type:

```json
"amount":{"kind":"i64","value":-7}
"mask":{"kind":"u64","value":255}
"active":{"kind":"bool","value":true}
"mode":{"kind":"string","value":"wrap"}
```

## Operator registry foundation

Every operator descriptor declares:

- stable type ID;
- minimum supported operator schema version;
- current operator schema version;
- typed parameter descriptors;
- required/optional status;
- mutation metadata with its own non-zero policy version.

Genome validation rejects unknown operator types, unsupported operator versions, undeclared parameters, missing required parameters and parameter-kind mismatches.

The registry deliberately contains no artistic operator implementation in FM-002. Later issues attach execution, richer mutation metadata and migration behaviour to this stable identity/descriptor boundary.

## Canonical JSON v1

Genome files are UTF-8 JSON. Input parsing is strict:

- duplicate object keys are rejected;
- invalid UTF-8 is rejected;
- JSON Unicode escapes and surrogate pairs are decoded correctly;
- malformed JSON numbers are rejected;
- numeric overflow is rejected;
- unknown fields in schema-owned objects are rejected;
- missing required fields are rejected;
- unsupported future schema/engine/operator versions are rejected rather than partially interpreted.

Input object field order is not semantically significant. Serialization emits one canonical representation.

### Canonical top-level key order

```text
schema_version
engine_contract_version
root_seed
operators
```

### Canonical operator key order

```text
instance_id
type_id
type_version
enabled
parameters
```

Parameter names are emitted in bytewise lexicographic order by the deterministic map representation. Parameter objects always emit `kind` then `value`.

Canonical JSON is compact (no insignificant spaces), uses JSON escapes only when required, and ends with exactly one LF (`0x0a`). That final LF is part of the canonical bytes and therefore part of genome identity.

## Genome identity

Genome identity is lower-case hexadecimal SHA-256 of the complete canonical genome bytes, including the final LF.

SHA-256 is implemented in the pure core; no platform crypto API, locale, filesystem or GPU state participates.

Representative canonical fixture identity:

```text
c2b815d1b94e4485a9a3d7f58152f53546809d2a2e2775b30aa9d0e0bb818e92
```

The standard SHA-256 vectors for empty input and `abc` are retained in the automated suite.

## Compatibility rule

A change that alters any known-answer entropy vector, canonical serialized bytes, schema meaning or genome identity is not a harmless refactor. It requires an explicit versioned contract change, updated migration/compatibility policy and an explanation in the implementing PR.
