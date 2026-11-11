# ART-TEMP-005 — Native sequence review

**Candidate:** *Seat Count: Minus One / The Parliament of Absence*, Draft PR #54. This is evidence and a playback handoff, **not a new artwork**, native render, or artistic acceptance.

## One-step owner review

Extract the supplied `FAULTMINE-ART-TEMP-005-seat-native-review.zip` and open `OPEN-SEAT-REVIEW.html` in Firefox or another local browser. It is an offline self-contained 32-frame viewer: play/pause, precise frame slider, ordered contact grid, loop toggle and keyboard stepping. The HTML embeds the original PNG bytes, not proxy frames. It is a **presentation** of native CPU output, not an FM-012 export.

For independent regeneration from the previously qualified Windows evaluation archive (Python standard library only):

```powershell
python gallery/temporal/05-seat-count-minus-one/review_native.py .\FAULTMINE-2026-11-11-main-four-family-evaluation-win64.zip .\OPEN-SEAT-REVIEW.html
python gallery/temporal/05-seat-count-minus-one/test_review_native.py .\FAULTMINE-2026-11-11-main-four-family-evaluation-win64.zip
```

The generator refuses any archive whose complete native replay receipt differs from SHA-256 `1ee290b15ffb8a07d4416b51185d223935c321bfea356ddc62e750ab12f4b590`, or whose 32 ordered original PNGs differ from the receipt. It checks the exact source/genome/seed, frame range and semantic rate before creating HTML. The regression embeds all images byte-for-byte and tests corruption refusal.

## Verified source and sequence

- Original source identity: `cb8df081f6f79c6ec75e974dc0f20cb227aa79e97c17dcaeadf554b311eda342`.
- Original genome identity: `e8f70bc1b738b5557d2c0e20beb8d1af49b4c1c0894c4d8bbe1758a11acc3850`.
- Original root seed: `9a5d7e63c204b811`.
- Native Windows Release CPU/WIC frames: **32/32**, 336×224, semantic frames 0–31 at authored 10/1 fps. All 32 encoded PNG SHA-256 checks passed; all 32 recorded decoded image IDs are distinct.
- First native output identity: `9cdc62bb711170a1ae91a47fe7af40c1bc154ab90120c3ce23ffab602302178e`; last: `789042dff8be2a9f488dc72ccf238d9838d2925fbb7d7e9b694ad80f0f359383`.
- Native qualification previously obtained in [Windows workflow 42969432120](https://github.com/techrote/FAULTMINE/actions/runs/42969432120), with original native evidence; the updated code does not alter the source, genome, project or any canonical PNG.
- The reproduced HTML's SHA-256 using the pinned archive and checked-in generator is `cd2f680287a24095dca7801ec5f4b257cb4d4af496542042f441a89a6ddf0781`.

## Actual visual selection

The nine inspected source-native keyframes (0, 4, 8, 12, 16, 20, 24, 28, 31) show recognizable **four lit chairs around a crossed-out empty centre** as repeated registration faults progressively tilt and split the forms. Unlike [Angle of Departure #50](https://github.com/techrote/FAULTMINE/pull/50), which folds layered topographic planes, this work centers on architectural absence and rhythmic repetition. The centre still reads throughout. The potential weakness is pacing: the displaced-chair vocabulary settles into sustained horizontal shear through the second half; frames 27→28 have only 1.8% of RGB bytes changed, while 28→29 jumps to 15.2%. Those are actual adjacent-frame measurements, not a claim of perceptual equivalence or physical playback.

**Reviewer decision:** view the genuine local 3.2-second sequence (including its loop boundary), compare with #50, and decide whether the later sustained shear is intentional duration or warrants a revised composition. Then perform separate *physical* Windows project opening, full/cancelled FM-012 export and source-relink checks. **Keep Draft** until those gates and owner artistic acceptance are satisfied.

This addition changes only artwork-owned review tooling. All previously authored source/genome/project/PNG contents remain immutable. No proxy images have been relabelled canonical.
