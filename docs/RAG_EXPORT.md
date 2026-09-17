# FAULTMINE canonical export and provenance contract

Status: FM-013 export contract v2, retaining FM-012 canonical image semantics.

## Authority boundary

Export is an output operation over an immutable semantic snapshot. It never promotes a preview buffer, canvas texture, tray thumbnail or contact-sheet cell into canonical specimen pixels.

- A still calls `SessionModel::render_full_at_frame` against the normalized full-resolution source and the active canonical genome.
- A temporal sequence calls the same full-resolution entry point independently for every explicit frame in `[begin,end)`.
- A contact sheet renders each explicitly ordered specimen genome canonically first, then creates a deterministic nearest-neighbour presentation thumbnail for layout.
- Export functions accept `const SessionModel&`; requesting, completing, cancelling or failing an export does not edit the genome, project, lineage, current timeline frame or preview state.

PNG through the accepted WIC RGBA/BGRA adapter remains the lossless portable image baseline. PNG file bytes are not themselves a canonical identity contract; the decoded canonical RGBA8 pixels and manifest identities are.

## Export manifest schema v2

Every export operation can emit a sidecar JSON manifest. Native UI defaults manifest output on. Manifest identity is not timestamp-based and deliberately stores no timestamp.

FM-013 advances the strict schema to `manifest_version: 2` by adding one required `laboratory_provenance` field. It is `null` for ordinary sources. For a frozen/materialized laboratory source it contains the validated external-decoder provenance defined by `RAG_EXTERNAL_DECODERS.md`, including original/mutated encoded identities, mutation seed and parameters, worker protocol/application version, decoder/OS metadata, outcome/diagnostic and the normalized materialized source identity. That materialized identity must equal the manifest `source_identity`.

The parser remains compatible with manifest v1. Unknown top-level/schema-owned fields, duplicate JSON fields, missing required fields, invalid canonical genomes, mismatched identities and unsupported manifest versions are rejected. V2 therefore extends provenance without weakening the FM-012 strictness contract.

Every manifest records:

- export kind: `still`, `contact-sheet` or `frame-sequence`;
- application version;
- engine-contract and genome-schema versions;
- normalized source identity and source path as convenience metadata;
- canonical genome identity, embedded canonical genome object and root seed;
- ordered operator summary with stable instance ID, type ID, type version and enabled state;
- explicit output dimensions and format;
- `canonical_full_resolution` truth marker;
- canonical decoded output-image identity when one final image exists;
- retained lineage derivation when the active genome is a retained specimen for the active source;
- explicit laboratory provenance or `null`.

Derivation data uses the FM-010 vocabulary and records kind, ordered parent identities, policy version and applicable seed/descendant/radius fields without inventing provenance for a detached manual state. Laboratory derivation is separate from this downstream genome derivation: a frozen source can have external-decoder provenance while its active specimen still has an ordinary manual/mutation/crossover lineage.

The manifest is canonical compact UTF-8 JSON with one final LF. It is human-inspectable and intentionally easy for later FM-014 headless tooling to parse strictly.

## Still export

`export_still` requires an explicit destination and collision policy. The default request policy is `fail_if_exists`; overwriting requires `CollisionPolicy::overwrite`.

A still records its explicit semantic frame. For non-temporal work this is normally frame 0. For temporal work a still may intentionally capture another explicit canonical frame.

The image is always rendered at the source's full canonical dimensions. Proxy-preview state is ignored. The manifest states `canonical_full_resolution: true` and records the decoded canonical output-image identity.

## Atomic final-file rule

PNG and manifest writers use an adjacent, collision-checked temporary path. Encoding/writing must complete before the temporary file is committed to the requested final path with a Win32 atomic move/replace operation.

Consequences:

- an encoder failure does not leave a final-looking incomplete PNG;
- a manifest write failure does not leave a final-looking incomplete manifest;
- `fail_if_exists` never deliberately deletes/replaces a user-existing final file;
- `overwrite` replaces only after a complete temporary output is available;
- a successfully committed image may remain if its subsequent optional manifest commit fails, and the returned structured error reports that partial operation truthfully.

FM-013's provenance augmentation follows the same atomic rule: after an FM-012 export succeeds, the v2 manifest is atomically replaced with the validated laboratory-provenance form. A failed v2 commit is reported as an export error rather than silently leaving provenance incomplete.

Temporary files are removed on handled encode/commit failure. Existing unrelated temporary-looking paths are not deleted; the exporter searches for an unused adjacent temporary name.

## Deterministic contact sheets

`ContactSheetRequest::specimens` is the semantic presentation order. Each item carries an explicit `order_key`, canonical genome and matching genome identity. The exporter never sorts by render completion, hash or worker order.

For each ordered specimen:

1. validate the canonical genome and supplied identity;
2. render the full-resolution specimen at the requested explicit frame;
3. compute its canonical rendered-image identity;
4. make the accepted deterministic nearest-neighbour proxy bounded by the requested cell size;
5. centre that proxy in its cell over a fixed opaque neutral background.

The final contact sheet is explicitly marked `canonical_full_resolution: false` and uses output format `png-rgba8-contact-sheet-presentation`. It is a review artefact, not a specimen image.

The companion manifest maps each cell to zero-based cell index, caller-provided order key, complete specimen genome identity, embedded canonical specimen genome and canonical full-resolution rendered-image identity used to create the presentation thumbnail.

The native FM-012 export menu reads the actual current `SpecimenTrayModel` through a read-only presentation accessor and exports its item vector in exact tray/index order. If no transient tray exists yet, it falls back to the retained lineage/selection in durable creation order, and finally to the active genome if nothing has been retained. The core export API also accepts any explicit caller-provided specimen/selection order.

## Canonical frame sequences

`FrameSequenceRequest` uses an explicit inclusive/exclusive range `[frame_begin, frame_end_exclusive)`. Empty/reversed ranges are rejected.

Each frame is rendered through `SessionModel::render_full_at_frame(frame)`, written as an independent atomically committed PNG, receives the canonical decoded-image identity, and receives the FM-011 `temporal_frame_identity_hex` binding source identity, genome identity, explicit frame index and canonical output image.

The sequence manifest also records the positive rational semantic timeline rate from `temporal.timeline-rate` or the accepted `30/1` default. Preview playback speed never enters export provenance.

### Stable naming

Frame filenames are:

```text
<stem>_<zero-padded frame index>.png
```

Padding is `max(requested minimum padding, decimal digits of the largest addressed frame)`. The native UI uses a minimum of six digits. Naming is a pure function of stem/range/frame and is independent of render timing.

## Collision and partial-output policy

For `fail_if_exists`, a sequence preflights all predictable frame destinations and the optional manifest before rendering. A known collision therefore fails before frame 0 is written.

For explicit `overwrite`, each individual frame is atomically replaced only after its new encoding succeeds.

If rendering/encoding fails after earlier frames have committed, those completed frame files remain; the `ExportResult` lists committed files and returns a structured operation/path/message error. The exporter never deletes pre-existing unrelated outputs as cleanup.

## Progress and cancellation

Contact-sheet and sequence APIs accept progress and cancellation callbacks.

Sequence cancellation is checked between frames, never by interrupting a WIC write halfway into a final path. On cancellation, already atomically committed frame files remain valid, no next-frame final file is created, result state sets `cancelled=true` without fabricating an encoder/render error, and when manifest output is enabled a partial manifest is committed with `complete=false`, `cancelled=true` and exactly the completed frame identity records.

The native sequence UI accepts explicit begin/end frames, displays completed/total progress in the status surface and uses Escape as the cancellation gesture. Timeline playback is paused before sequence export. Still/contact/sequence UI exposes manifest on/off and an explicit overwrite toggle.

## Verification contract

`canonical_export_contracts` continues to freeze at least full-resolution still pixels while proxy preview is active; source/genome/root-seed/operator/output manifest fields; strict manifest round-trip and schema rejection; collision/overwrite policy; contact ordering/mapping; sequence naming, frame pixels/hashes/identities and cancellation semantics; and project/genome/current-frame invariance.

FM-013's `isolated_laboratory_contracts` additionally proves a frozen source's manifest-v2 laboratory provenance round-trips exactly and remains bound to the frozen source identity. All earlier Debug/Release deterministic tests and the native Win32/D3D smoke remain required. A later format/video exporter may be added, but it must not weaken PNG/image-sequence canonical semantics or provenance requirements.
