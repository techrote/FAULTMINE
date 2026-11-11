# ART-TEMP-005 — Genuine native sequence / owner decision

**Draft PR #54: Seat Count: Minus One.** All artwork source, typed genome, project and archived native canonical PNG bytes remain unchanged. This is an optional presentation edit, not a revised canonical render or physical Windows acceptance.

## Open and compare

Extract the reviewer ZIP and open `OPEN-SEAT-REVIEW.html` locally. **Authored** is the default: all 32 original integer-indexed native CPU/WIC frames, in order, at 10 fps. **Editorial study** is an optional 19-keyframe selection with longer exposures at 0 and 31 and omission of slow/repetitive mid/late frames. This is not the engine's 10/1 semantics. Manual seek, step and all 32 contact tiles address exact original frames in both modes. Pause, mode switch, scrub, reset, Escape and hiding the page cancel playback.

To recreate using the previously qualified main four-family Windows archive and Python stdlib:

```powershell
python gallery/temporal/05-seat-count-minus-one/review_native.py .\FAULTMINE-2026-11-11-main-four-family-evaluation-win64.zip .\OPEN-SEAT-REVIEW.html
python gallery/temporal/05-seat-count-minus-one/test_review_native.py .\FAULTMINE-2026-11-11-main-four-family-evaluation-win64.zip
```

The generator pins native receipt SHA-256 `1ee290b15ffb8a07d4416b51185d223935c321bfea356ddc62e750ab12f4b590`, verifies all 32 encoded PNG identities and ordered frame numbers, and refuses corrupted or missing data. Native source ID `cb8df081f6f79c6ec75e974dc0f20cb227aa79e97c17dcaeadf554b311eda342`, genome ID `e8f70bc1b738b5557d2c0e20beb8d1af49b4c1c0894c4d8bbe1758a11acc3850`, seed `9a5d7e63c204b811`, size 336×224. First native output ID `9cdc62bb711170a1ae91a47fe7af40c1bc154ab90120c3ce23ffab602302178e`; last `789042dff8be2a9f488dc72ccf238d9838d2925fbb7d7e9b694ad80f0f359383`. Genuine Windows [native qualification #42969432120](https://github.com/techrote/FAULTMINE/actions/runs/42969432120), not a new render in this correction. Every original decoded identity is distinct.

## Art critique and exact decision

I inspected genuine native frames for **Seat** and [#50 The Angle of Departure](https://github.com/techrote/FAULTMINE/pull/50) from the four-family archive. Angle has broad interlocking cyan/coral cartographic planes and a geometric void; Seat has four luminous chair bodies around a large crossed-out hollow seat, displaced later into chromatic trailing registers. Their principal silhouettes and negative-space roles differ materially. Seat's weakness is the repetitive later shear: transitions 25→26, 26→27 and 27→28 have little structural development, punctuated by 28→29. Editorial mode condenses these stretches without rewriting the authored sequence.

**Owner:** watch both modes and decide **admit unchanged / request a separate native temporal revision / reject**. **Integration:** physically open the portable project, confirm source relink and exercise FM-012 full and cancelled frame-sequence export on Windows. Neither CI-native CLI evidence nor browser playback establishes GUI or aesthetic acceptance. Keep Draft.


## Direct one-file animated canonical-pixel comparison (11 Nov)

This Draft also contains `make_native_apng.py` and `test_native_apng.py`. With the genuine pinned four-family evaluation ZIP (or the smaller seat editorial-review ZIP) in the working directory, run:

```powershell
python .\gallery\temporal\05-seat-count-minus-one\make_native_apng.py .\FAULTMINE-2026-11-11-four-family-owner-review-v2-win64.zip .\SEAT-32-FRAME-NATIVE-PRESENTATION.png
python .\gallery\temporal\05-seat-count-minus-one\test_native_apng.py .\FAULTMINE-2026-11-11-four-family-owner-review-v2-win64.zip
```

Open the produced **animated PNG (APNG)** in Firefox or Chrome. It is a **presentation-only** 32-frame 10 fps loop, generated from each archived native Windows CPU PNG's original compressed IDAT scanlines; no re-render, fabricated intermediate frames or palette reduction. The generator independently decodes all 32 original PNGs, checks source/genome/seed/ordered output identities against the pinned Windows receipt, preserves decoded RGBA8 pixel values in the APNG, and refuses to overwrite a prior file. The test reconstructs all 32 embedded frames and verifies every decoded byte against the native original; also rejects corrupt receipt/image and output collision. A separate Pillow 12.3.0 decode independently confirmed all 32 APNG frames pixel-for-pixel in the 11 Nov local run. Local generated APNG SHA-256: `f922fb740a255e10dd90c4fc8ef54f3845f174b8af3ea2c5ea9a06544ae62d29`.

**Observed motion/selection:** 32/32 frames have distinct decoded identities, but differences between frames 26→27 and 27→28 affect only 7.84% and 4.48% of pixels respectively (RGB comparisons on actual decoded Windows frames); the 28→29 transition changes 42.85%. This corroborates the optional HTML editorial cut rather than justifying a new numbered composition. The one-file APNG always shows the full unchanged authored order. Genuine chromium in-memory APNG playback changed rendered pixels after one second; managed test policy denied direct `file://` navigation, so physical Firefox playback and Win32 application export are **not** claimed.

**Decision:** retain Draft #54 for owner motion/pacing decision (**admit unchanged / request separately versioned temporal revision / reject**); Integration must independently exercise portable project loading and FM-012 full/cancelled export in the physical Windows GUI.