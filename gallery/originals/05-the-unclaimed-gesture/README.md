# ART-ORIG-005 — The Unclaimed Gesture
## Five Fingers, Six Shadows

**Status: authentic Windows CPU canonical frame 0 committed unchanged, independently replayed and visually inspected.** The exact PNG is now available alongside the source/genome/project in this Draft. Physical Win32 GUI and owner aesthetic admission remain pending.

![Original Windows CPU/WIC canonical frame 0](unclaimed-gesture-canonical.png)

A full-scale hand rises through a rectangular scanner. Four digit registration labels identify fingers, while an empty "05?" plate floats over the far-right negative space, as if the gesture could be measured but its owner could not be recorded. A cyan second outline stands six pixels apart, already suggesting a misfiled exposure. The hand is deliberately iconic and structurally unlike the moth, transit carriage, spore canopy and bell original commissions. The picture is an original geometric construction: no photographs, stock textures, borrowed illustrations, third-party fonts, external requests or AI source images.

### Intended faults

Seven registered, ordered, version-1 CPU operators: seven-pixel logical memory band repetition at every seventh band with negative one-band delta; five bounded address bursts over 77-pixel spans (maximum offset 120); red/blue channel displacement (-4,+2) for chromatic registration; seeded two-pixel scanline jitter; eight 17×4 red bit bursts (XOR 64); 32-level RGB quantization; and 16-level Bayer4 ordered dithering. The faults are intended to fracture contours, shift ghost registration and suggest more or fewer fingers. The source-level plan above is distinct from the observed CPU faulted image: the five-finger silhouette survives, but colour-separated registration and dense interference partly obscure small finger details and labels.

### Frozen evidence and identities

| Artifact | Identity |
| --- | --- |
| Authored 320×224 opaque indexed PNG SHA-256 | f7d396be000cd153f29f1b25110162627d0dbf5726be51ea39f35d3d2b005f84 |
| Normalized WIC-equivalent source RGBA8 identity | db8754258a9a788d36f4ac466fc538b021725c0cbc7d293c13f8b4c4a005a108 |
| Canonical genome v1 identity (LF included) | f3493de53d45a4013c64488f16fb9b3d1e666f66113b2e996aca2068b4525da4 |
| Project v2 encoding SHA-256 | 14880df2f490aed709f998d1d52e004eb344fd9c4245ee9f97f53ed20160bfbe |
| Root seed / semantic frame | c18e70d453b29a64 / 0 |
| Canonical output ID (Windows CPU frame 0) | 981bf2f2bb64f69cb8da54a59a343af8b3b9c0c51828bccddff924973b672979 |
| Encoded native PNG SHA-256 | fb3272681ad20147035e9209ed767d1b2d6e4ea26b3a44c7407cf1b27feb07d4 |
| Windows receipt | `native-replay-receipt.json` (Debug + Release replay passed) |

The source.png is an immutable original source. make_source.py records the authored geometry, constructed with Python 3.13.5 / Pillow 12.3.0; regenerating its encoding in a different Pillow version may produce byte-different PNG containers, so treat the archived source's decoded RGBA and recorded SHA as authoritative. genome.json is canonical compact typed JSON with a final LF. the-unclaimed-gesture.fmproj is project v2 with a manual-root favourite, no imaginary mutation lineage and a repository-relative source convenience path. artwork.json records the **committed, byte-exact Windows-native PNG** and its independent external proof.

### Independent local preflight

From the repository root, run:

    python gallery/originals/05-the-unclaimed-gesture/verify_artwork.py --self-test

The stdlib-only test **always** checks the committed original Windows-native PNG, including transport SHA-256, PNG CRC/deflate/filter, exact decoded RGBA8 output identity and corrupted-proof rejection. It also checks source CRC/decompression/normalized identity, strict genome/project encoding and typed stack, seed, lineage and the pinned native replay receipt. It does not execute the fault operators. To additionally cross-check a fresh artifact download byte-for-byte against the committed proof:

    python gallery/originals/05-the-unclaimed-gesture/verify_artwork.py --self-test --canonical PATH\TO\unclaimed-gesture-canonical.png

This optional gate additionally requires exact byte equality between the original external artifact and the committed PNG. An arbitrary source preview or substitute cannot pass either the default or external gate.

### Native Windows proof and local owner review

Native Windows qualification has already passed on **11 November 2026**, at pinned authored head `962e55a3906e256e0a9ad407fdd56c44ecdbce53`, through [Actions run 42990696670](https://github.com/techrote/FAULTMINE/actions/runs/42990696670), [artifact 13841447863](https://github.com/techrote/FAULTMINE/actions/runs/42990696670/artifacts/13841447863). The artifact includes a runnable Windows owner-review ZIP and the genuine native PNG. The accompanying source/engine tests also passed [run 42990696809](https://github.com/techrote/FAULTMINE/actions/runs/42990696809).

To **review immediately**, open `unclaimed-gesture-canonical.png` directly or double-click `OPEN-REVIEW.cmd` from a Windows checkout with a Release build. The launcher opens the archived native image and starts FAULTMINE when the executable is present; it does not silently load/relink a project. In the GUI use Ctrl+Shift+O to open the artwork project and report any source relink prompt.

To reproduce using a **Windows x64** checkout of this draft, from repository root:

    .\scripts\configure.ps1
    .\scripts\build.ps1 -Configuration Debug
    .\scripts\build.ps1 -Configuration Release
    .\scripts\test.ps1 -Configuration Debug
    .\scripts\test.ps1 -Configuration Release
    python .\gallery\originals\05-the-unclaimed-gesture\verify_artwork.py --self-test
    .\gallery\originals\05-the-unclaimed-gesture\render-win.ps1 -CrossCheckDebug -OutputDirectory .\artifacts\original-05-native

The driver fails closed on source or genome drift and refuses an occupied evidence folder. The exact native PNG **and** original receipt are now committed unchanged and independently reconciled against original Windows artifact identities. CLI proof cannot substitute for physical FM-012 project-reopen/export acceptance. For owner review, extract the *inner* Windows review ZIP from artifact 13841447863, run `Run FAULTMINE.cmd`, press Ctrl+Shift+O, open `gallery\originals\05-the-unclaimed-gesture\the-unclaimed-gesture.fmproj`, compare the canvas against `canonical-windows-evidence\unclaimed-gesture-canonical.png`, then exercise Ctrl+E/export and cancellation. Report any source relink prompt instead of silently accepting it.

### Observations / limitations

The **genuine native CPU output was inspected** at 320×224 and enlarged 3× nearest-neighbour. The five-finger silhouette and open right-hand registration aperture remain legible, while pervasive cyan/green/magenta registration and local noise obscure some fingertip gaps and tiny annotations. Relative to the bronze Bell and other authentic submitted originals, this remains a separate hand/measurement-aperture composition rather than an acoustic silhouette. The fault colour treatment is less differentiated than the geometry: owner inspection must decide whether interference overwhelms delicate detail. This is a candidate, not artist approval.

**Closed:** authentic Windows native render/replay, archived byte-exact output PNG, output ID, actual post-fault visual inspection, exact original project/source identity. **Open:** physical Win32 project open/relink and FM-012 still/cancel export; owner aesthetic decision and Integration merge. No historical art, shared engines, other worker branches, license files or automation definitions are changed.
