# WITNESS RELAY / 02 — The Last Empty Carriage

**Status: complete deterministic authoring candidate; Windows canonical export and artistic selection pending.**

This is a typographic/architectural glitch still of an *empty final-service carriage*. A single heavy horizontally framed vehicle floats between two orange service pylons beneath a long cyan catenary. Seven dark compartments form repeated square absences; a red denial cross attacks the forward door. It is intentionally **not** another specimen moth, circular broadcast aperture or morphological quarry.

The source was authored from elementary vector-like pixel primitives, a custom 5×7 bitmap lettering alphabet and integer-coordinate rail drafting, then committed as a losslessly indexed PNG. The image is original and contains no borrowed photograph, font, texture or generative-model derivative. Its stripped-back black ground, orange carriage, pale lit windows and cyan electrical line furnish robust registration marks for **structural corruption** rather than indiscriminate colour noise.

## Exact replay contract

- Authored 384 × 256 opaque indexed PNG: `source.png`; encoded SHA-256 `b20301c86bbaf0bb1f886fce9fe4f70017286bc0f13b734e261d3a982f049435`.
- Normalized WIC straight RGBA8 source identity: `05ec476c708b7b569ce4c192b1d32d5270d5fb057619c238dc3d46c40a713b69`.
- Canonical genome v1 SHA-256: `58f440d271d56e81e94de2f395931de1774af8a92d64627dad9514a41722e313`; engine contract v1; six typed operators.
- Explicit root seed `a17e08c4d3ad1e02`, **frame 0**.
- Saved project v2: `witness-relay.fmproj`; one explicit manual-root favourite lineage node.

Fault order: seven-row intermittent band repeats; nine short logical address bursts; green-channel horizontal lag; thirteen rectangular red/blue bit perforations; twenty-level quantization; 4×4 ordered dither. Repetition and green-channel lag should visibly misregister the carriage windows against the stable shell. This is **artistic intention, not yet an observation of native output**.

## Reproduce on supported Windows x64

From the repository root, after `scripts/configure.ps1` and Release/Debug builds:

```powershell
python gallery/originals/02-witness-relay/verify_artwork.py
.\gallery\originals\02-witness-relay\render-win.ps1 -OutputDirectory .\artifacts\witness-relay-native -CrossCheckDebug
```

The PowerShell driver refuses to overwrite evidence, pins source/genome bytes, calls the actual Release CPU `FAULTMINE-render.exe --frame 0`, verifies the exported decoded pixels in Release and optionally Debug, and creates `native-replay-receipt.json` only after replay succeeds. It never treats a D3D preview as canonical. To inspect interactively, open the versioned project using `Ctrl+Shift+O` with repository-relative `source.png` available; source relink, FM-012 GUI export and human aesthetic approval remain independent gates.

## Evidence and admission

The committed source has been directly inspected: its rail carriage, dark windows, catenary and peripheral typography are distinguishable in the *source*. This is **not** evidence about the post-fault pixels. Native output identity, canonical output PNG and FM-012 export manifest are intentionally absent until a supported Windows run produces and verifies them. Do not merge this Draft on the strength of authoring-only checks. Integration should run the script with Debug + Release on exact PR head, archive the resulting PNG/receipt, then assess actual compositional quality and decide whether to admit it.
