# ART-TEMP-005 — Seat Count: Minus One

**The Parliament of Absence** · original temporal glitch study · authoring package v1 · 11 November 2026

## Premise and visual plan

A chamber contains five empty chairs. Four have luminous seats; the central, largest chair is an open copper-red outline marked with an X. The absence is the subject, not a generic distortion. Flat elevation and a measured floor register keep each empty place legible before any corruption. The governing motion idea is an incorrect head count: bands of chairs are recalled, displaced, held and superimposed as if a camera keeps trying to restore an audience that never arrived.

The **actual unprocessed** 336×224 source BMP was generated and inspected at 3× nearest-neighbour size. Its composition has a central negative-space anchor, four strong repeated chair silhouettes, cyan/ochre/cream accents and a subdued architectural grid. **No post-fault FAULTMINE frame was rendered or visually assessed by this author.** The intended spectral multiplication, cancellation and partial recovery are hypotheses to test on native output, not claims of observed animation.

The prior temporal works centre on a signal aperture (01), elevator registers (02), a ruptured textile (03) and folded geometry (04). This study substitutes **repeated occupied outlines surrounding a deliberately unfilled centre** and measures when temporal faults destroy versus clarify that legible contradiction.

## Reproducibility contract

| Input | Identity |
|---|---|
| Authored source | `source.bmp`, self-contained 24-bit RGB BMP, 336×224 |
| Encoded BMP SHA-256 | `efbb9b9f7fc73af41b5c10b6330424c5bd2f47794ab7a964318d149df613e4d0` |
| Normalized RGBA8 source identity | `cb8df081f6f79c6ec75e974dc0f20cb227aa79e97c17dcaeadf554b311eda342` |
| Genome | `genome.json`, schema 1, engine contract 1 |
| Canonical genome SHA-256 | `e8f70bc1b738b5557d2c0e20beb8d1af49b4c1c0894c4d8bbe1758a11acc3850` |
| Root seed | `9a5d7e63c204b811` |
| Loadable project | `seat-count-minus-one.fmproj`, schema 2, exact root lineage |
| Canonical frame address | `[0,32)`, explicit integer frames, semantic rate 10/1 |
| Keyframe review | 0, 4, 8, 12, 16, 20, 24, 28, 31 |
| Out-of-order native replays | 31, 0, 18, 5, 24 |

The nine ordered v1 operators use red-channel address misregistration, a 48-tick triangle global drift, independently addressed 19-tick saw-band tearing, model-owned eight-pixel partial refresh, displaced decaying frame memory, **seeded sample/hold blue-channel drift**, feedback blending, integer quantization and semantic rate metadata. The source generator contains no network calls or third-party images. Every encoded pixel is derived from fixed integer geometry and constants; no clock or nondeterministic RNG is used.

## Local authored-input checks

From the repository root, with Node and Python available:

```powershell
node .\gallery\temporal\05-seat-count-minus-one\generate_source.js
python .\gallery\temporal\05-seat-count-minus-one\verify_artwork.py
```

The generator refuses to overwrite a differing immutable BMP. Verification checks encoded bytes, normalized source pixels, compact LF-terminated genome, schema, parameter types, project/lineage roundtrip, and rejects truncated or modified input. These checks **do not** execute the canonical rendering engine.

## Genuine Windows canonical admission path

On supported Windows x64 after normal Debug and Release MSVC configuration/build/test:

```powershell
.\gallery\temporal\05-seat-count-minus-one\render-sequence-win.ps1 -Configuration Release -OutputDirectory .\artifacts\seat-count-release
.\gallery\temporal\05-seat-count-minus-one\render-sequence-win.ps1 -Configuration Debug -OutputDirectory .\artifacts\seat-count-debug
python .\gallery\temporal\05-seat-count-minus-one\verify_native_frames.py .\artifacts\seat-count-release --contact .\artifacts\seat-count-PRESENTATION.png
python .\gallery\temporal\05-seat-count-minus-one\verify_native_frames.py .\artifacts\seat-count-debug
```

The bounded PowerShell driver uses the **actual native** `FAULTMINE-render.exe --frame N` for every full-size frame, logs real source/output identities and PNG transport SHA-256, and uses the native **read-only** `--verify` command on five nonchronological frames. It rejects changed inputs and existing output directories. The independent Python auditor decodes PNG CRC, WIC RGBA8 pixel identity, order and distinct frame count before assembling a labelled **presentation-only** 3×3 contact sheet. Debug and Release arrays must be compared per frame; no canonical identity is predicted or invented here.

## Status and artistic selection

Authored source reproducibility and versioned genome/project are prepared. The Windows-native renderer, full frame sequence, frame-identity verification, contact sheet and human observation of temporal behaviour are **pending**. The original BMP and source preview are not proxies for canonical post-fault output.

Integration must (1) run the native driver and inspect real keyframes and playback; (2) reject the study if chair recognition collapses into unstructured noise or if all frames are only recolours; (3) compare all decoded Debug/Release frame IDs and randomized-request replay; (4) load/relink the project through the actual Win32 application and perform genuine FM-012 complete and cancelled sequence exports; (5) decide aesthetic admission. Keep this author PR Draft and unmerged until these gates are resolved.

**No shared engine/UI/tests/workflows, historical artwork or other worker branches are modified.**
