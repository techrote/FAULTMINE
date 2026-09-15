# FAULTMINE product contract

## Purpose

FAULTMINE is a glitch-art synthesis instrument for deliberately exploring mistakes in how digital visual data is stored, addressed, interpreted, reconstructed and evolved.

It is not primarily a collection of decorative post-processing filters. Its distinctive capability is to make low-level fault semantics composable, deterministic, searchable and artistically navigable.

The essential loop is:

1. load or generate a source;
2. construct a fault genome;
3. render a deterministic specimen;
4. generate related descendants;
5. select interesting descendants;
6. lock useful structure and mutate the rest;
7. narrow mutation radius to excavate an aesthetic neighbourhood;
8. save exact provenance and export canonical output.

The user should be able to say **“I like this mistake; give me more mistakes like it”** and receive useful, reproducible answers rather than unrelated random presets.

## Primary user experience

### Main canvas

The centre of the application displays the active specimen at interactive resolution. The view supports fit, 1:1, integer zoom where useful, pan, before/after comparison and a clear indication when the visible preview is proxy-resolution or otherwise non-canonical.

### Fault stack

A dense side panel shows ordered operators and their most important parameters. Operators can be enabled/disabled, reordered, duplicated, removed and selected for deeper editing. Parameters expose lock state separately from operator bypass state.

Example conceptual stack:

```text
SOURCE
  -> reinterpret RGBA8 as BGRA8
  -> row stride +13 logical bytes
  -> address xor 0x0340
  -> blue channel delay 37 px
  -> swap bitplanes 2 and 5
  -> feedback previous frame x0.164
  -> LUT rust-terminal-04
OUTPUT
```

### Specimen tray

A tray presents a population of deterministic descendants derived from the active parent(s). Each thumbnail carries enough lightweight provenance to recover its genome and lineage.

Core actions:

- promote specimen to active parent;
- multi-select parents for crossover;
- favourite/pin a specimen;
- lock selected genes/operators;
- reroll descendants with the same parent and mutation policy;
- change mutation radius;
- move backward/forward through lineage/history;
- compare selected specimens at larger size.

### Mutation radius

Mutation radius is a first-class artistic control rather than a generic random-strength slider.

At low radius it changes parameter values slightly. At medium radius it can alter discrete modes and larger parameter ranges. At high radius it may insert/remove/reorder compatible operators or change operator types, subject to type/safety constraints.

The mutation algorithm is deterministic for a given parent genome, mutation policy, seed and descendant index.

### Keyboard-first interaction

The application should make repeated exploration fast without requiring precision mouse work. Exact bindings are implementation/UI decisions, but v1 should support efficient keyboard actions for reroll, promote, pin, lock, mutation-radius adjustment, history traversal, stack navigation and parameter nudging.

## Fault families

The family names describe semantics, not UI presets. Operators remain individually documented and testable.

### Memory and addressing faults

Examples include:

- logical row-stride mismatch;
- row/column offset;
- address XOR/mask/add/rotate transforms;
- wrap, clamp, drop and fill policies;
- block permutation;
- tile addressing mistakes;
- Morton/Z-order reinterpretation;
- duplicate/skip regions;
- dimension/pitch disagreement;
- neighbourhood reads from intentionally wrong logical coordinates.

All such faults are simulated against bounded logical buffers. They never perform an actual invalid host-memory read/write.

### Representation faults

Examples include:

- channel order reinterpretation;
- packed-format reinterpretation such as RGB565/RGBA8 concepts;
- endian/byte-order changes;
- planar versus interleaved reinterpretation;
- signed/unsigned value interpretation;
- premultiplied/unpremultiplied-alpha disagreement;
- linear/sRGB disagreement;
- indexed/palette/luminance reinterpretation;
- eventually selected YUV-style representation disagreements.

### Bit faults

Examples include:

- deterministic bit flips;
- stuck bits;
- masks and XOR patterns;
- shifts and rotates;
- nibble swaps;
- bitplane reorder/exchange;
- spatially structured burst errors;
- threshold/mask-controlled bit faults.

### Signal/display faults

Examples include:

- scanline displacement;
- horizontal phase errors;
- channel delays;
- tearing-like region phase changes;
- partial-frame refresh;
- ghost/hold behaviour;
- deterministic sync instability.

The implementation models these effects deterministically; it does not depend on actual unstable display timing.

### Transform/reconstruction/codec faults

Later work may include block coefficient mutation, damaged residual application, wrong-block reconstruction, motion-vector-like displacement and controlled mutation of actual encoded files. Actual malformed-codec work is isolated into a worker process so decoder failures cannot compromise the editor.

### Recursive/stateful faults

Temporal operators may use explicit previous-frame state, feedback buffers and derived masks. State is model-owned and advances only through explicit ticks/frames. Rendering frame N from an initial state and explicit timeline must be reproducible.

### Colour synthesis

Colour controls are the intentionally “correct” counterpart to structural corruption and include:

- LUT/gradient mapping;
- indexed palettes;
- palette generation/mutation;
- quantisation;
- ordered/noise dithering;
- channel curves where they remain deterministic.

Colour stages allow structurally wild faults to be constrained into coherent visual families.

## Source model

v1 prioritises still images loaded through WIC. Internally, sources are normalized into an explicit canonical pixel representation before fault processing unless an operator intentionally requests reinterpretation of another logical representation.

Future inputs may include:

- generated patterns;
- raw/arbitrary binary data interpreted as 2D memory;
- image sequences/video frames;
- framebuffer/screen captures;
- other deterministic generators.

Source identity is recorded so a genome cannot silently be applied to a different source while claiming to reproduce the same specimen.

## Genome and project concepts

A **genome** contains fault topology, parameter values, operator identities/versions and root seed. It excludes transient window placement, hover state and other irrelevant UI data.

A **project** contains source references/identity, one or more genomes/specimens, locks, lineage, favourites, view/editor state and export metadata needed for continuing a session.

Persisted formats are versioned, human-readable and migration-aware.

## Reproducibility contract

For supported v1 Windows x64 builds, canonical result bytes must be determined by:

- normalized source bytes and identity;
- canonical genome;
- root seed;
- engine/operator/schema versions;
- explicit timeline/tick input for temporal content.

Incidental factors such as thread scheduling, machine time, filesystem enumeration order, GPU vendor and UI interaction timing must not change canonical output.

## Proxy and full-resolution rendering

Interactive exploration may use a deterministic proxy representation for responsiveness. The UI must make this visible. Full-resolution export uses the canonical path and the same semantic parameters.

Proxy sampling is not allowed to alter the saved genome merely to make thumbnails look better.

## Export and provenance

Minimum useful exports include:

- canonical still image;
- genome/project file;
- human-readable manifest with source identity, seed, engine/schema version and operator stack;
- specimen contact sheet.

Temporal work later adds image sequences and practical lossless/high-quality animation/video paths where feasible without undermining the standalone baseline.

## Non-goals for early v1

- becoming a general-purpose raster editor;
- layers/brushes/text/layout competing with dedicated image editors;
- arbitrary third-party plugin ABI before the internal operator contract stabilises;
- cloud accounts or mandatory online services;
- nondeterministic “AI style” generation;
- authentic memory unsafety or deliberate process corruption;
- cross-platform UI before the Windows architecture and deterministic contracts are mature.

## Success criteria

FAULTMINE is successful when a user can discover a visually interesting accident, preserve exactly what caused it, explore nearby accidents systematically, return to an ancestor without loss, and reproduce/export the chosen result later on a supported machine without relying on a lucky unrecoverable random state.
