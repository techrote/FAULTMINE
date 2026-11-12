# The Wound That Counts Backwards — Reversible Wound

**ART-TEMP-003 · independent temporal study · DRAFT / canonical Windows frame rendering pending.**

A pair of torn woven registers hangs on opposite sides of an irregular black void. Warp and weft are deliberately mismatched: the left plate is teal and faded brass, the right violet and bone. Six punched holes, unmoored thread ends and intermittent pink stitches cross the fracture. This is an original, author-drawn **320 × 192, 24-bit BMP**, without photography, borrowed imagery, fonts, fetched textures, AI-generated proxy frames or third-party assets.

The motion hypothesis is a reversible repair that becomes its own damage. A small permanent red-channel misregistration precedes 7-pixel band tearing. The band phase follows an explicit 30-frame saw, while a displaced, decaying temporal trail leaves fragments of prior stitching behind. A separately seeded, five-frame sample/hold blue-channel offset introduces longer beats; a previous-frame feedback blend slows recovery. Finally 24-level colour quantisation locks the palette. Neither a palette flicker nor the circular signal aperture of ART-TEMP-001 nor the elevator of ART-TEMP-002. **The actual post-fault motion has not yet been inspected.** Structural coherence, pacing and artistic quality are admission questions, not claims established by the source.

## Exact immutable inputs

| Field | Identity |
| --- | --- |
| Encoded source BMP SHA-256 | `d8bc0cef014b6f7d92b6a53b436c3aa4859eb5b44b8fdb6b52259a812264a6b8` |
| Normalized source RGBA8 SHA-256 | `ba533af14e6dc02861ab0d8e55239edcc2358f554e07f460aa4fde9cd590619c` |
| Compact LF genome SHA-256 | `0de88b0fdfbd357ce09761200d2b96b90c8aa2977a9887a3dd7011315751cba2` |
| Root seed | `e30bd49c7a612f08` |
| Contract versions | genome 1 / engine 1 / project 2 / artwork manifest 1 |
| Canonical semantic frames | `[0,30)` (30 explicit unsigned integers) |
| Rational timeline rate | `12/1`; provenance/presentation metadata, not frame input |
| Suggested presentation frame indices | `0,4,8,12,16,20,24,27,29` |
| Nonchronological replay probes | `29,0,17,6,24` |

Project `the-wound-that-counts-backwards.fmproj` has a strict schema-v2 manual-root lineage carrying this exact embedded genome and normalized source identity. A moved source may be relinked **only** when decoded RGBA identity matches.

## Reproduce, verify, present

Source-only input preflight (cross-platform, Python standard library):

```sh
python gallery/temporal/03-the-wound-that-counts-backwards/verify_artwork.py
```

On supported Windows x64 with current native Debug and Release builds, run from repository root using *distinct empty output directories*:

```powershell
.\gallery\temporal\03-the-wound-that-counts-backwards\render-sequence-win.ps1 -Configuration Release -OutputDirectory .\artifacts\wound-release-01
.\gallery\temporal\03-the-wound-that-counts-backwards\render-sequence-win.ps1 -Configuration Debug -OutputDirectory .\artifacts\wound-debug-01
python .\gallery\temporal\03-the-wound-that-counts-backwards\verify_native_frames.py .\artifacts\wound-release-01 --contact .\artifacts\wound-contact-PRESENTATION.png
python .\gallery\temporal\03-the-wound-that-counts-backwards\verify_native_frames.py .\artifacts\wound-debug-01
```

The PowerShell tool renders genuine full-resolution CPU PNGs for each explicit frame, validates the WIC normalized source identity, captures PNG transport hashes and canonical output identities, then **replays the same PNGs read-only in deliberately nonchronological order**. The separate stdlib Python tool checks full PNG CRCs, unfilters actual decoded RGBA8 pixels, independently recomputes every frame's normalized RGBA identity and, only after successful verification, can produce a labelled **presentation-only** 3×3 contact sheet. A partial/interrupted author run writes `native-cli-replay-PARTIAL.json` rather than a false completed manifest. An existing output path is never overwritten.

Compare the ordered output identities in the Debug and Release audit JSON files; they must match exactly. Inspect native frames `0,4,8,12,16,20,24,27,29` in chronological order and compare the contact sheet with the complete sequence. Reject if the fabric simply dissolves, repeats visually or fails to recover a legible structure. A contact sheet is **not** a canonical frame or an FM-012 manifest.

### Independent integration gates

- Native debug/release full-frame qualification, output IDs and the actual sequence visual inspection; authored source was inspected, *no canonical post-fault image is yet claimed*.
- In FAULTMINE Win32 load/relink project with exact source fingerprint, export true FM-012 complete `[0,30)` sequence and validate its frame/temporal identity manifest.
- Cancel a fresh FM-012 sequence midway: require valid previously written frames, `complete=false`, `cancelled=true`, exact completed records and no next-frame final PNG.
- Independent artistic admission and guarded integration; leave this author PR Draft. None of the above requires changing the engine, shared workflow or any old artwork.
