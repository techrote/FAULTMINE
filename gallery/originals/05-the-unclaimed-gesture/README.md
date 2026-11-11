# ART-ORIG-005 — The Unclaimed Gesture
## Five Fingers, Six Shadows

**Status: reproducible authored candidate. The native post-fault image has not yet been produced or inspected.** Do not admit this as a finished canonical gallery work until Windows-native proof, visual review and physical project evaluation are complete.

A full-scale hand rises through a rectangular scanner. Four digit registration labels identify fingers, while an empty "05?" plate floats over the far-right negative space, as if the gesture could be measured but its owner could not be recorded. A cyan second outline stands six pixels apart, already suggesting a misfiled exposure. The hand is deliberately iconic and structurally unlike the moth, transit carriage, spore canopy and bell original commissions. The picture is an original geometric construction: no photographs, stock textures, borrowed illustrations, third-party fonts, external requests or AI source images.

### Intended faults

Seven registered, ordered, version-1 CPU operators: seven-pixel logical memory band repetition at every seventh band with negative one-band delta; five bounded address bursts over 77-pixel spans (maximum offset 120); red/blue channel displacement (-4,+2) for chromatic registration; seeded two-pixel scanline jitter; eight 17×4 red bit bursts (XOR 64); 32-level RGB quantization; and 16-level Bayer4 ordered dithering. The faults are intended to fracture contours, shift ghost registration and suggest more or fewer fingers. **This is compositional intent, not observed FAULTMINE output.**

### Frozen evidence and identities

| Artifact | Identity |
| --- | --- |
| Authored 320×224 opaque indexed PNG SHA-256 | f7d396be000cd153f29f1b25110162627d0dbf5726be51ea39f35d3d2b005f84 |
| Normalized WIC-equivalent source RGBA8 identity | db8754258a9a788d36f4ac466fc538b021725c0cbc7d293c13f8b4c4a005a108 |
| Canonical genome v1 identity (LF included) | f3493de53d45a4013c64488f16fb9b3d1e666f66113b2e996aca2068b4525da4 |
| Project v2 encoding SHA-256 | 14880df2f490aed709f998d1d52e004eb344fd9c4245ee9f97f53ed20160bfbe |
| Root seed / semantic frame | c18e70d453b29a64 / 0 |
| Canonical output ID | **UNAVAILABLE, not invented** |

The source.png is an immutable original source. make_source.py records the authored geometry, constructed with Python 3.13.5 / Pillow 12.3.0; regenerating its encoding in a different Pillow version may produce byte-different PNG containers, so treat the archived source's decoded RGBA and recorded SHA as authoritative. genome.json is canonical compact typed JSON with a final LF. the-unclaimed-gesture.fmproj is project v2 with a manual-root favourite, no imaginary mutation lineage and a repository-relative source convenience path. artwork.json distinguishes verified source identities from unverified native outputs.

### Independent local preflight

From the repository root, run:

    python gallery/originals/05-the-unclaimed-gesture/verify_artwork.py --self-test

The stdlib-only test checks PNG chunk CRCs/decompression, actual 320×224 normalized source pixels, strict genome/project encoding, schema, typed stack, seed, all stable IDs, manual-root lineage, pinned fingerprints, deliberate corrupt-PNG rejection and duplicate-JSON-key rejection. This does **not** render or evaluate canonical faulted pixels.

### Exact Windows native qualification to finish this artwork

Using a **Windows x64** checkout of this exact draft branch, from repository root:

    .\scripts\configure.ps1
    .\scripts\build.ps1 -Configuration Debug
    .\scripts\build.ps1 -Configuration Release
    .\scripts\test.ps1 -Configuration Debug
    .\scripts\test.ps1 -Configuration Release
    python .\gallery\originals\05-the-unclaimed-gesture\verify_artwork.py --self-test
    .\gallery\originals\05-the-unclaimed-gesture\render-win.ps1 -CrossCheckDebug -OutputDirectory .\artifacts\original-05-native

The driver fails closed on source or genome drift and refuses an occupied evidence folder. It must produce unclaimed-gesture-canonical.png and native-replay-receipt.json, and require both Release and Debug to verify identical decoded RGBA8 pixels without rewriting output. Inspect the *actual* canonical PNG before describing the artistic result. If accepted, add the **unchanged** native image, exact output ID and receipt to this same artwork subtree in a subsequent guarded commit. A CLI proof does not substitute for physical FM-012 project-reopen/export acceptance.

### Observations / limitations

The **authored source image was rendered and inspected locally**: a pale-edged dark hand dominates the left two-thirds, a deliberately separated cyan outline and muted internal tendons suggest a latent exposure, a red empty registration plate occupies the negative space to the right. Text labels are small by design; the figure remains legible at 3× nearest-neighbour presentation. No post-fault image was rendered or visually inspected. The author environment is Linux with no Wine or supported Windows native runner, and repository scope forbids adding shared CI workflows. Standard Windows CI may validate the engine on a PR, but does not independently render this artwork without the explicit driver invocation above.

**Acceptance gates:** actual Windows native render, exact known output ID and replay, inspected final fault morphology, physical Win32 project open/relink and FM-012 export, and Integration/owner aesthetic admission. No historical art, shared engines, other worker branches, license files or automation definitions are changed.
