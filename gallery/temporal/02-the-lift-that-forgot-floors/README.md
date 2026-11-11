# The Lift That Forgot Floors — Mechanical Amnesia

**ART-TEMP-002, draft original temporal composition.** An overloaded elevator's floor registry is drawn as hard-edged technical notation: three separated architectural systems, a suspended car, an interrupted diagonal signal and intentionally large unoccupied shaft. It is not a recolour or descendant of Signal Mausoleum, which is a ring/aperture communications study.

The static **authored source** is a new 288 × 192 Windows-compatible 24-bit BMP, with no imported photography, fonts, textures or external assets. The left register counts eight floors; the right counterweight never matches their intervals. A suspended central cage supplies a stable compositional anchor. Cyan rails and bone/ochre/rust structures should make the resulting displacement visible without filling the dark void.

The six-operator canonical genome is designed for **mechanical forgetting**: triangle phase drift shifts the current plate, a single refresh band preserves earlier addresses, vertical feedback displacement recirculates prior canonical pixels downward by three pixels at weight 222/256, red-channel phase offsets misregister the print, and 20-level quantisation preserves coarse industrial contrast. A semantic `10/1` frame rate is metadata only. Frames `[0,24)` describe an exact, finite study with requested proof at `0,3,6,9,12,15,18,21,23`; independently replay `23,0,18,6,12` to prevent chronology-dependent bugs.

No source, operator, parameter or seed was taken from another author's artwork. The original bitmap was visually inspected. **No temporal FAULTMINE canonical output has been visually inspected as of this draft.** The expected descent, wear and memory effect is an artistic hypothesis until rendered. No proxy or fabricated contact sheet is included.

## Exact authored identities

- `source.bmp`: SHA-256 (encoded bytes) `9ffbeb836d5437487d45dce6cdb394d7d2c08992edb9deba387cab8025a1986f`.
- Normalized source RGBA8 identity: `906aaeb14df9d961ae302c641b053e7db5cf5e13305abeb67ea438733d736d60`.
- `genome.json`: SHA-256 canonical compact LF bytes / genome identity `075c3dadcf6b9d4357f8ff6295c066497e7cb97083f91557b52a3ca18dc999f0`.
- Root seed `67da08c2be71fa30`; genome schema 1; engine contract 1; project schema 2.
- Source path inside `.fmproj` is repository-relative convenience only; the normalized source fingerprint is authoritative. On a moved checkout use the native relink dialogue and require exact source identity.

## Reproduction and gates

Run `python gallery/temporal/02-the-lift-that-forgot-floors/verify_artwork.py` from any working directory. This is **only a strict source/genome/project preflight**, not a replacement for the Windows canonical renderer.

On supported Windows x64 after normal Debug/Release build from the repository root:

```powershell
.\gallery\temporal\02-the-lift-that-forgot-floors\render-sequence-win.ps1 -Configuration Release -OutputDirectory .\artifacts\lift-release-01
.\gallery\temporal\02-the-lift-that-forgot-floors\render-sequence-win.ps1 -Configuration Debug -OutputDirectory .\artifacts\lift-debug-01
```

The helper refuses an existing output directory, uses real `FAULTMINE-render.exe --frame N`, writes 24 decoded-authoritative PNGs, and checks nonchronological `--verify` replay against the exact files (including no mutation of PNG transport bytes). Its JSON audit is **not an FM-012 export manifest**. It writes a clearly marked `PARTIAL` audit on interruption rather than claiming completion. Compare all Debug/Release sampled decoded RGBA identities, inspect actual frames and select a contact sheet only from verified PNGs.

For full admission, open `the-lift-that-forgot-floors.fmproj` in the actual Win32 GUI (`Ctrl+Shift+O`; source relink when necessary), export a genuine FM-012 complete `[0,24)` sequence and check every frame/temporal identity, then test cancellation on a distinct fresh destination and examine its partial manifest. Integration owns independent visual/ownership admission and guarded merge. No historical art is modified by this package.
