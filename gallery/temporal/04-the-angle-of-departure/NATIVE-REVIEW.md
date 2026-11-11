# ART-TEMP-004 — native chronological owner review

**Unadmitted.** This is a presentation/evidence addendum to #50, not a changed artwork, native export or substituted proxy. Source, genome, project, original receipt and 36 authored frame identities remain immutable.

## Actual Windows evidence

- Original native evidence: [Integration Windows run 42969432120](https://github.com/techrote/FAULTMINE/actions/runs/42969432120), [artifact 13832958088](https://github.com/techrote/FAULTMINE/actions/runs/42969432120/artifacts/13832958088). The later four-family owner-review ZIP also retains the 36 original Release PNGs and its native CLI receipt.
- Authored source ID: `332cf105a6625bf25949b68b053f4c8ac748b16a9aa9b86b734ee683c459b497`; genome ID: `851824bb7e806b75f0e819ddb9b1f902e763e5a2700421fdee9cc5951a55b680`; root seed: `b64a7c291d0ee305`.
- Exact pinned Release receipt SHA-256: `fedc650318aedf9b6f1cbe5809bd5e0f648c4a25fffcac8092e3ced396ec6b5c`.
- **36/36** complete, ordered, distinct Windows CPU/WIC PNGs at **320×192**. The separate read-only auditor verifies PNG chunk CRCs, encoded SHA-256 and independently decoded RGBA8 identities; native out-of-order replays were `35,0,19,7,28`.
- First frame output ID: `363a27beb0514ea69283d41e104c3f486f66c83e937a95ebbc4b7562505b3891`. Last: `8b4d198ed084f5809ecb6cfb5f96c53806ee53f7e269cc129af5f4bafcad7ccb`.
- All 36 full-frame APNG presentation images independently decoded by Pillow and matched their corresponding original native PNG bytes **pixel-for-pixel**; preview SHA-256 `3fe11a50b525c0d397716a7ea650adab23d0a075668ae6e567baf802b840d534`.

## Rebuild the watchable preview

Extract the Windows native review artifact. From the repository root, run:

```powershell
python .\gallery\temporal\04-the-angle-of-departure\verify_artwork.py
python .\gallery\temporal\04-the-angle-of-departure\verify_native_frames.py .\CANONICAL-REVIEW-EVIDENCE\angle-04-release
python .\gallery\temporal\04-the-angle-of-departure\make_presentation.py .\CANONICAL-REVIEW-EVIDENCE\angle-04-release .\ANGLE-36-FRAME-PRESENTATION.png
```

Open the resulting **APNG** in Firefox or another APNG-capable viewer for all 36 frames in chronological order at **12 fps**. The image is presentation only: the generator copies native WIC compressed scanlines into APNG full-frame SOURCE replacements; it never recomputes FAULTMINE pixels. The output refuses an existing destination, changed native receipt, corrupted/missing frame or wrong decoded image ID. To generate the existing nine-sample contact, use `verify_native_frames.py ... --contact new-contact.png`.

## Curatorial comparison and actual next gate

I inspected the genuine nine-frame native Angle contact and eight actual native keyframes (Angle frames 0,12,24,35 versus Seat #54 frames 0,12,24,31). Angle retains a legible black diamond hinge and three coloured planes while drifting, with stronger low-noise negative space than Seat's highly streaked later chairs. Its slow opening is measurable: 0→1 changes **2.47%** of pixels, but 16→17 changes **41.94%** and 34→35 **45.81%**. This is a pacing concern, not a claim of unusable motion. A separate owner-review HTML can add deliberately labelled **presentation-only** holds without changing native frame order or canonical bytes.

**Integration/owner decision:** Inspect the full 36-frame motion and decide admit, revise or reject. Separately perform physical Win32 project load/relink and **FM-012 full-sequence plus cancellation/partial-manifest** acceptance. Neither a passing CLI replay, an APNG, a browser preview nor a CI smoke test qualifies those GUI gates. Keep #50 Draft until they pass; do not merge this stacked review alone as an admitted artwork.
