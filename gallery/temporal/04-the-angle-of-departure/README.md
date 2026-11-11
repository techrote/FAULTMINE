# The Angle of Departure — Misfolded Cartography

**ART-TEMP-004 · independent authored temporal artwork · DRAFT; native Windows frame qualification pending.**

A hinge of negative space has slipped out of an angular field map. Three opaque paper-like leaves—pale ruled metal, rust-coloured recording foil and teal grid stock—overlap around a skewed black diamond. Detached registration slivers accumulate on opposing edges. The source is an original **320×192, 24-bit BMP**, generated using only integer geometry and the Python standard library. No third-party photography, fonts, image downloads or borrowed assets. The committed `source.bmp` is authoritative; `generate_source.py` reproduces its exact bytes and refuses to overwrite a different image.

The intended temporal conflict is not a looped colour change. The map repeatedly slips sideways along its implied fold (36-tick triangular phase). Smaller independently phased band tears interrupt that motion. A displaced, decaying trail retains ghosts of previous alignments, while partial-refresh holds one register against the next. A seeded six-frame sample/hold moves only the blue channel; a low feedback blend slows the new fold's appearance. Quantisation makes the paper colours less continuous. The expected shape trajectory is departure → interference → accumulation → attempted realignment. Whether the **actual CPU output** delivers that trajectory remains to be established through native rendering. Aesthetic claims based on the unprocessed source are deliberately limited.

Unlike ART-TEMP-001's circular signal aperture, ART-TEMP-002's mechanical lift, and ART-TEMP-003's woven seam, this work is a set of nested angular surfaces and competing flat graphic registrations. All fault calculations use supported version-1 typed operators; no engine semantics were modified.

## Immutable inputs

| Field | Identity |
| --- | --- |
| Source, original BMP SHA-256 | `7fdf2e80da19a28fe41b15d25599e790aee6ca231ae0fd580df87ad66daecd52` |
| Normalized RGBA8 source identity | `332cf105a6625bf25949b68b053f4c8ac748b16a9aa9b86b734ee683c459b497` |
| Canonical LF genome identity (SHA-256) | `851824bb7e806b75f0e819ddb9b1f902e763e5a2700421fdee9cc5951a55b680` |
| Root seed | `b64a7c291d0ee305` |
| Versions | genome 1, engine 1, project 2, artwork note 1 |
| Typed stack | 9 operators; distinct 128-bit instance IDs |
| Semantic frames | `[0,36)` (36 explicitly addressed frames) |
| Rational timeline rate | `12/1` (presentation/provenance metadata, never inferred frame input) |
| Contact-sheet samples | `0,4,8,12,16,20,24,28,35` |
| Independent read-only probes | `35,0,19,7,28` |

The project carries one exact `manual-root` lineage specimen, the full canonical genome, source identity and saved view state. Its `source.path` is repository-root-relative. When launching the packaged Windows executable outside this checkout, relink to the included source **only if normalized decoded identity matches**.

## Source preflight and canonical Windows reproduction

From the repository root on Python 3 (stdlib only), run:

```sh
python gallery/temporal/04-the-angle-of-departure/verify_artwork.py
python gallery/temporal/04-the-angle-of-departure/generate_source.py
```

To obtain genuine CPU canonical frames on a supported Windows x64 build, use two new empty paths:

```powershell
.\gallery\temporal\04-the-angle-of-departure\render-sequence-win.ps1 -Configuration Release -OutputDirectory .\artifacts\angle-release-01
.\gallery\temporal\04-the-angle-of-departure\render-sequence-win.ps1 -Configuration Debug -OutputDirectory .\artifacts\angle-debug-01

python .\gallery\temporal\04-the-angle-of-departure\verify_native_frames.py .\artifacts\angle-release-01 --contact .\artifacts\angle-PRESENTATION.png
python .\gallery\temporal\04-the-angle-of-departure\verify_native_frames.py .\artifacts\angle-debug-01
```

The PowerShell driver invokes `FAULTMINE-render.exe --frame N` for every integer `N` in `[0,36)`, records the native output identities, the actual PNG transport hashes and five intentionally nonchronological `--verify` replays. The independent stdlib PNG auditor checks transport CRC, decodes full straight RGBA8 pixels, recomputes every identity and optionally makes a clearly **noncanonical presentation-only** contact sheet from nine verified frames. Its contact PNG is not a canonical FAULTMINE frame nor an FM-012 manifest.

Both Debug and Release must return identical ordered decoded canonical output IDs. Source-only verification is not a substitute. An aborted native run writes `native-cli-replay-PARTIAL.json` and must never be described as a complete sequence. Existing outputs are not overwritten.

## Review and acceptance

1. Run the native author driver in Debug and Release; compare all 36 output identities, inspect the nine actual frame samples and evaluate the complete chronological motion. Reject mere trivial flicker or irreversible visual sludge.
2. Import the `.fmproj` in the native GUI; test moved-source relink against exact normalized source ID.
3. Separately test the **real FM-012 UI** full export and cancellation/partial manifest: correct complete flag, cancellation state, committed record list and no fabricated final frame.
4. Integration decides artistic admission, exact-head Windows CI and guarded merge. This author submission remains Draft.

**Observed in author environment:** the original decoded source was enlarged and visually inspected; encoded BMP, normalized source hash, canonical genome hash and project-lineage consistency were checked. **Not observed:** any native post-fault frame, native frame ID, Windows sequence export, GUI cancellation or canonical contact sheet. No proxy was substituted for those missing results.
