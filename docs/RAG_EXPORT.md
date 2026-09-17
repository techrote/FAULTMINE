# FAULTMINE canonical export and capture contract

Status: FM-012 export-manifest schema v1.

## Authority boundary

Export never treats presentation state as artwork authority. Canonical stills and temporal sequence frames are freshly rendered from the retained **full-resolution normalized source**, exact canonical genome and explicit semantic frame through the CPU reference pipeline.

A proxy preview, specimen thumbnail, D3D texture, stretched canvas, before/after view, playback timer or window paint result is never eligible to become a canonical still/sequence export merely because it is currently visible.

Contact sheets are intentionally different: they are deterministic presentation artefacts assembled from canonically rendered specimens. Their manifest therefore records `canonical_pixels:false` and `proxy_pixels:false`; each cell maps back to a canonical specimen genome rather than claiming the composed sheet itself is a canonical specimen output.

## Output baseline

FM-012 uses lossless PNG as the primary still/contact/sequence image format. Canonical image semantics remain straight RGBA8 UNORM under `RAG_IMAGE_PIPELINE.md`; WIC's BGRA encoder adapter is only an I/O boundary.

Still export captures the current explicit semantic frame. Temporal sequence export accepts an explicit half-open frame range `[start,end)` and renders every frame independently through `render_pipeline_at_frame`/`SessionModel::render_full_at_frame`.

No video encoder, FFmpeg dependency, cloud service or lossy format is introduced by FM-012.

## Export manifest schema v1

Companion manifests use `.fmmanifest.json`. Serialization is deterministic UTF-8 JSON with a final LF and `manifest_schema_version:1`.

Every manifest records:

- FAULTMINE application version;
- engine-contract and genome-schema versions;
- normalized source identity plus source path as non-authoritative convenience metadata;
- canonical genome identity, exact root seed and the complete embedded canonical genome JSON;
- ordered operator instance/type/type-version/enabled summary;
- output kind, dimensions, `rgba8-unorm`, PNG format and canonical/proxy classification;
- explicit semantic frame range and rational semantic timeline rate;
- active retained lineage derivation when one exists;
- completeness/cancellation state;
- per-frame audit records for still/sequence exports;
- deterministic contact-cell-to-specimen mapping for contact sheets.

The embedded genome is parsed against the current registry on manifest load and cross-checked against the recorded genome identity, root seed, engine/schema versions and ordered operator summary. Source/genome/image/frame identities are content/semantic identities; timestamps are deliberately absent from identity-bearing manifest state.

Unknown top-level/schema-owned fields and unsupported manifest schema versions are rejected rather than partially reinterpreted.

## Per-frame audit identity

Every canonical still/sequence frame record contains:

- explicit `frame_index`;
- deterministic filename;
- normalized canonical image identity;
- `temporal_frame_identity_hex`, which additionally binds source identity, genome identity and explicit frame index under the FM-011 temporal identity contract.

This makes a sequence auditable even when multiple semantic frames happen to have identical pixels.

## Deterministic names

Sequence output uses the chosen base stem followed by `-f` and a zero-padded decimal semantic frame number, then `.png`.

Padding is at least six digits and expands only when the largest requested frame requires more digits. Example for base `motion.png`:

```text
motion-f000002.png
motion-f000003.png
motion-f000004.png
```

The sequence companion manifest uses the base stem plus `.fmmanifest.json`.

No wall-clock timestamp, thread completion order or render duration participates in naming.

## Collision and safe-write rules

The export API has two explicit policies:

- `fail_if_exists` — preflight refuses any final path collision before a still/contact export and before any frame of a sequence is written;
- `replace_existing` — the caller has explicitly authorized replacement of colliding final outputs.

PNG and manifest bytes are first written to a temporary peer file in the destination directory. A successfully completed temporary file is then committed to the final name using Windows move/replace semantics. Encoder/write failure removes the temporary file and does not leave a final-looking truncated output.

The native UI uses save/overwrite confirmation and, for sequence-wide collisions, an additional explicit replace prompt before switching to `replace_existing`. If manifests are enabled, a collision with the companion manifest is also treated as an overwrite decision even when the image filename itself is new.

## Sequence cancellation and partial results

Sequence export is bounded to at most 100000 frames per invocation. A progress callback runs between atomic frame commits. Cancellation therefore means:

1. already committed PNG frames remain valid and are not deleted;
2. the frame at which cancellation is observed is not written;
3. a companion manifest is written when enabled with `complete:false`, `cancelled:true` and audit records for exactly the committed frames;
4. active source, genome, lineage, editor history and interactive semantic frame remain unchanged.

An encoder or destination failure follows the same no-truncated-final-file rule. Successfully committed earlier frames remain inspectable instead of being silently destroyed.

## Contact sheets

`export_contact_sheet` consumes an explicit ordered specimen list. Each specimen is freshly rendered from the full canonical source at the current explicit semantic frame, then deterministically nearest-neighbour reduced only for its contact-sheet cell.

The sheet:

- preserves caller order exactly;
- uses bounded deterministic cell geometry;
- includes a small deterministic label containing exported selection index and genome-identity prefix;
- records full mutation seed/descendant/genome mapping in the companion manifest when provenance output is enabled;
- never promotes or mutates any specimen/session state.

The native FM-012 command exports the user-retained mutation selection represented by lineage records, in creation order. This is the durable selection already created by promotion/favourite workflows rather than render-completion order. The lower-level API accepts an explicit current tray/selection directly, enabling exact tray/index contact sheets and later batch-miner reuse without coupling export semantics to native panel internals.

## Native controls

FM-012 adds an **Export** menu while retaining the established File-menu still command:

- `Ctrl+E` — canonical full-resolution still PNG;
- `Ctrl+Shift+E` — deterministic contact sheet of the retained mutation selection;
- `Ctrl+Alt+E` — temporal frame-sequence range dialog;
- **Write provenance manifest** — checked by default and may be toggled off for the current UI session;
- sequence export shows progress and exposes a Cancel button; cancellation takes effect between atomic frame commits.

The sequence dialog accepts explicit unsigned start-inclusive/end-exclusive frames. Destination dialogs control the base PNG name. The reusable export API exposes the same manifest-on/off choice through `ExportOptions::write_manifest`, with provenance enabled by default.

## State invariants

All public export entry points accept `const SessionModel&`. Export therefore cannot use ordinary editor/session mutators while writing output.

Tests additionally freeze that success, failure and cancellation preserve canonical genome identity and the interactive current frame. Export does not create manual edit-history entries or lineage nodes.

## Verification contract

`canonical_export_contracts` verifies at least:

- a deliberately tiny proxy preview still exports full-resolution canonical pixels;
- manifest schema/version, required fields and exact round-trip parsing;
- source identity, genome identity, root seed, engine/schema/application version and operator summary agreement;
- future manifest version rejection;
- default collision refusal and explicit replacement;
- invalid destination failure without a final-looking output;
- deterministic contact-sheet order and cell/specimen mapping;
- deterministic zero-padded sequence naming and explicit half-open range;
- every exported frame image/temporal identity matches direct canonical replay;
- cancellation preserves completed frames, writes truthful partial provenance and emits no cancelled-frame file;
- export paths do not mutate genome identity or interactive semantic frame.

CI runs these semantic export contracts in both Debug and Release, alongside the established real native window/D3D/session/mutation/crossover/lineage smoke lifecycle. The native smoke remains intentionally presentation-focused; export correctness is exercised directly through the reusable export API so failures are diagnosable as normal test assertions rather than hidden inside a window callback.
