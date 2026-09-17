# FAULTMINE deterministic temporal engine contract

Status: FM-011 temporal contract v1.

## Semantic time

Canonical time is an explicit unsigned 64-bit `frame_index`. The CPU render entry point is `render_pipeline_at_frame(source, genome, registry, frame_index)`. Wall clock, paint frequency, timer jitter, monitor refresh and thread scheduling are not semantic inputs.

`render_pipeline()` remains the accepted still-image compatibility entry point and is exactly frame 0. Existing non-temporal genomes therefore retain their accepted output bytes.

Frame N is defined by deterministic replay of ticks `0..N` from the initial empty temporal state. The reference implementation deliberately prefers correctness over checkpoint complexity. A later checkpoint/cache implementation is permitted only if it is byte-equivalent to replay.

## Model-owned state

Each temporal operator receives only:

- its current frame input image;
- root seed and stable operator instance ID;
- explicit frame index;
- its own previous `OperatorTemporalState`, if any.

The v1 state payload is an optional previous canonical operator output image. It is owned by pipeline execution, never by Win32/D3D presentation code and never stored in a process-global feedback buffer. Operators cannot observe another session or an unrelated operator's state.

Stateful operators commit their current canonical output as the next tick state. Signal-only operators leave the state empty.

## Deterministic modulators

`evaluate_temporal_modulator` is the exact integer v1 modulation primitive. Supported shapes are:

- `triangle` — periodic integer triangle;
- `saw` — periodic integer saw/ramp;
- `square` — exact two-level periodic signal;
- `ramp` — one-shot integer ramp that clamps at its endpoint;
- `sample-hold` — named deterministic entropy, held for `period_frames` ticks;
- `keyed-noise` — independent named deterministic entropy addressed by explicit frame.

Amplitude is integral and bounded to 4096 for v1 consumers. Period is `1..65536`. Sample/hold and keyed noise derive streams from the normal FM-002 root seed + instance ID + operator-specific purpose + explicit frame/bucket address. No sequential/global RNG state participates.

## Temporal operator family

All FM-011 temporal visual operators are type version 1 and expose typed mutation metadata through the normal registry, so FM-009 mutation and FM-010 crossover operate on their parameters without a parallel evolutionary format.

### `temporal.feedback-blend`

Parameter `amount_256` is the previous-frame weight `0..256`. Frame 0 is source passthrough. Later frames compute exact byte-domain blend:

```text
(current * (256-a) + previous * a + 128) >> 8
```

### `temporal.feedback-displace`

Parameters: signed `dx`, `dy`; `amount_256`; boundary `wrap|clamp|fill`. The previous operator output is spatially displaced under the normal bounded logical-coordinate idea, then blended with current input using the exact rule above.

### `temporal.partial-refresh`

Parameters: `band_height`, `phase_bands`. Frame 0 is source passthrough. Later ticks retain previous output except for exactly one deterministic horizontal band refreshed from current input. The active band is addressed from explicit frame index.

### `temporal.trail`

Parameters: signed `dx`, `dy`; `decay_256`; boundary. Previous output is displaced, decayed with exact integer arithmetic and combined by per-byte maximum with current input to produce an accumulating ghost/trail fault.

### `temporal.phase-drift`

Uses a serialized modulator (`shape`, `period_frames`, `phase_frames`, `amplitude_pixels`) plus boundary policy to shift the current canonical frame horizontally.

### `temporal.tearing-phase`

Adds `band_height` to the phase-drift parameters. Each horizontal band evaluates an explicitly addressed phase and receives its own deterministic horizontal displacement.

### `temporal.channel-phase`

Adds a selected `channels` subset. Only those channels read from the modulated horizontal displacement; unselected channels pass through exactly.

## Explicit semantic frame-rate metadata

`temporal.timeline-rate` is a pixel-preserving ordinary genome operator containing positive rational `rate_num/rate_den` metadata. It does not determine which frame is rendered; frame index remains an explicit render argument. Its purpose is to persist semantic playback/export rate in normal genome/project identity instead of deriving rate from a UI timer.

Projects without this optional operator have a presentation default of `30/1`. Adding/editing the operator is available through the existing descriptor-driven generic editor and therefore remains human-visible canonical project state.

Preview/playback speed is separate session/presentation state expressed as a multiplier in thousandths (`125..8000`). Changing it changes only how quickly the UI requests successive explicit frames, not the pixels or identity of any frame N.

## Session and native transport

`SessionModel` owns an explicit `current_frame` and exposes seek, exact single-frame forward/backward step, reset, full-resolution render-at-frame and preview rate controls. `ensure_preview` and normal full render use the explicit current frame.

The native Timeline menu/hotkeys provide:

- play/pause — `Ctrl+Alt+Space`;
- step backward/forward — `Ctrl+Alt+,` / `Ctrl+Alt+.`;
- reset to frame 0 — `Ctrl+Alt+Home`;
- seek -10/+10 — `Ctrl+Alt+PageUp` / `Ctrl+Alt+PageDown`;
- preview rate /2 or x2 — `Ctrl+Alt+Down` / `Ctrl+Alt+Up`.

The title displays current semantic frame, semantic rational rate, preview multiplier and play/pause state. A Win32 timer is only a presentation scheduler: each timer delivery requests one explicit next frame. Pausing, repainting or timer jitter cannot change the definition of frame N.

## Persistence, lineage and evolutionary search

Temporal visual parameters and semantic rate metadata are ordinary canonical genome parameters. Consequently existing strict genome/project serialization, project schema v2 lineage records, mutation retention, crossover, favourites and provenance preserve them without introducing a second temporal schema.

Transient `current_frame`, play/pause state and preview-speed multiplier are not specimen identity and are not added to lineage. A retained specimen still names a canonical genome; a temporal output additionally names an explicit frame.

## Frame identity

`temporal_frame_identity_hex` domain-separates and hashes:

- normalized source identity;
- canonical genome identity;
- explicit frame index;
- canonical output image identity;
- temporal frame identity contract version.

This prevents frame provenance from being conflated with still genome identity.

## Verification obligations

`deterministic_temporal_contracts` freezes:

- exact integer vectors for periodic, ramp, sample/hold and frame-keyed modulators;
- frame-0 initial-state semantics and multi-frame feedback replay;
- identical frame requests producing identical bytes independent of earlier requests;
- explicit frame identity;
- default-registry exposure and typed mutation metadata for the temporal family;
- canonical temporal genome round-trip;
- session seek/step/reset and preview-rate separation;
- project save/reload reproducing an exact requested temporal frame and semantic rational rate.

All pre-FM-011 deterministic tests and the native D3D/session smoke remain required in Debug and Release. A future implementation may optimize replay or presentation, but changed accepted frame bytes require an explicit temporal/operator compatibility decision rather than a mechanical golden update.
