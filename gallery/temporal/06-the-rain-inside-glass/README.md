# ART-TEMP-006 — The Rain Inside Glass

**A Forecast That Cannot Leave.** A sealed meteorological vessel contains twelve arrested droplets and a rising, asymmetrical tide. A broken right-hand wall interrupts the instrument's apparent precision. The adjacent measuring ticks keep an index of something they cannot measure.

The underlying source is an independently authored 256×176 24-bit Windows BMP; there are no photographs, external assets, fonts or licenses to retrieve. Its actual decoded pixels were inspected: a cool cyan vessel with a diagonal turquoise reservoir, suspended amber/cyan marks and one central red droplet. **This observation is of the source only, not a canonical glitched frame.**

## Temporal composition

The nine typed operators act in this precise order: green-channel registration offset → slow triangular whole-frame drift → staggered fast saw tearing → one-band partial refresh → decaying down-left trail → displaced memory feedback → seeded red-channel sample/hold → 32-level RGB quantization → persisted 8/1 timeline rate. The two incommensurate 32- and 13-frame periods prevent a simple palette flicker; the accumulation and partial refresh should turn the jar's apparently static waterline into an unstable meteorological memory. That is an intended behaviour, **not an observed canonical result**.

- Root seed: `4c6f8a21d3e507b9`; schema/engine: `1/1`; native frame integers: **[0,24)** (24 frames).
- Intended replay and comparative montage: `0, 3, 6, 9, 12, 15, 18, 21, 23`; nonchronological integrity probes: `23, 0, 12, 5, 18, 9`.
- Input SHA-256: `82180916e814d2738eb8c41e1bf3c745022b219cca788c5467819e83e380fba3`; normalized source RGBA8 ID: `60352c93cb2322e3d40d2fd77b9793e68cd0cd0314b4e75996113411bd718fe9`.
- Compact genome SHA-256: `c76d773802faa8e8de5291497dc06e73c277150b65a5779f17dbebaa7d61b189`; project schema: 2.
- Source/project: `source.bmp`, `genome.json`, `rain-inside-glass.fmproj`.
- Authentic canonical result identity and contact sheet: **pending Windows execution**; none fabricated.

## Reproduce (Windows x64)

Build the accepted FAULTMINE renderer using repository MSVC Debug/Release instructions. From repository root:

```powershell
python gallery/temporal/06-the-rain-inside-glass/verify_artwork.py
./gallery/temporal/06-the-rain-inside-glass/render-sequence-win.ps1 -Configuration Release -OutputDirectory ./artifacts/rain-glass-release
./gallery/temporal/06-the-rain-inside-glass/render-sequence-win.ps1 -Configuration Debug -OutputDirectory ./artifacts/rain-glass-debug
```

Each run refuses reused evidence directories, emits 24 actual `FAULTMINE-render.exe` full-resolution PNGs, records output and PNG byte identities, and runs six read-only replay checks in deliberately nonchronological order. Independently compare Debug/Release decoded RGBA identities; inspect frames at the nine designated key indices and assemble a *clearly labelled presentation-only* montage from the genuine PNGs. Do not re-encode source assets or infer canonical pixels from a D3D/proxy preview. Import the bundled project into native Win32 and verify the source relink, then exercise FM-012 sequence complete/cancel paths separately.

## Current gate

Only the immutable authored BMP, typed canonical genome, loadable-format project and input preflight are available here. Native Windows rendering, 24-frame replay, Debug/Release equivalence, FM-012 GUI export/cancellation, motion inspection and independent artistic admission remain **unverified**. PR stays Draft; Integration owns any merge. All previous gallery art and shared source files are unchanged.
