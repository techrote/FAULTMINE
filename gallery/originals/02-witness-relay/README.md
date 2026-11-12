# WITNESS RELAY / 02 — The Last Empty Carriage

**Status: Windows-native CPU canonical output and Debug/Release read-only replay achieved; original owner-review package available. Physical GUI, FM-012 and artistic acceptance pending.**

This is a typographic/architectural glitch still of an *empty final-service carriage*. A single heavy horizontally framed vehicle floats between two orange service pylons beneath a long cyan catenary. Seven dark compartments form repeated square absences; a red denial cross attacks the forward door. It is intentionally **not** another specimen moth, circular broadcast aperture or morphological quarry.

The source was authored from elementary vector-like pixel primitives, a custom 5×7 bitmap lettering alphabet and integer-coordinate rail drafting, then committed as a losslessly indexed PNG. The image is original and contains no borrowed photograph, font, texture or generative-model derivative. Its stripped-back black ground, orange carriage, pale lit windows and cyan electrical line furnish robust registration marks for **structural corruption** rather than indiscriminate colour noise.

## Exact replay contract

- Authored 384 × 256 opaque indexed PNG: `source.png`; encoded SHA-256 `b20301c86bbaf0bb1f886fce9fe4f70017286bc0f13b734e261d3a982f049435`.
- Normalized WIC straight RGBA8 source identity: `05ec476c708b7b569ce4c192b1d32d5270d5fb057619c238dc3d46c40a713b69`.
- Canonical genome v1 SHA-256: `58f440d271d56e81e94de2f395931de1774af8a92d64627dad9514a41722e313`; engine contract v1; six typed operators.
- Explicit root seed `a17e08c4d3ad1e02`, **frame 0**.
- Saved project v2: `witness-relay.fmproj`; one explicit manual-root favourite lineage node.

Fault order: seven-row intermittent band repeats; nine short logical address bursts; green-channel horizontal lag; thirteen rectangular red/blue bit perforations; twenty-level quantization; 4×4 ordered dither. Repetition and green-channel lag should visibly misregister the carriage windows against the stable shell. The genuine Windows-native frame-zero pixels have since been inspected: carriage and catenary remain legible amid conspicuous chromatic displacement. The rendering is substantially different in architectural silhouette from MOTH ROM, Bell and Gesture; this is not an owner aesthetic decision.

## Reproduce on supported Windows x64

From the repository root, after `scripts/configure.ps1` and Release/Debug builds:

```powershell
python gallery/originals/02-witness-relay/verify_artwork.py
.\gallery\originals\02-witness-relay\render-win.ps1 -OutputDirectory .\artifacts\witness-relay-native -CrossCheckDebug
```

The PowerShell driver refuses to overwrite evidence, pins source/genome bytes, calls the actual Release CPU `FAULTMINE-render.exe --frame 0`, verifies the exported decoded pixels in Release and optionally Debug, and creates `native-replay-receipt.json` only after replay succeeds. It never treats a D3D preview as canonical. To inspect interactively, open the versioned project using `Ctrl+Shift+O` with repository-relative `source.png` available; source relink, FM-012 GUI export and human aesthetic approval remain independent gates.

## Evidence and admission

The original authored source remains byte-for-byte unchanged. **Actual Windows-native CPU canonical proof now exists** from [run 42955357432](https://github.com/techrote/FAULTMINE/actions/runs/42955357432): 384×256, frame 0, seed `a17e08c4d3ad1e02`, decoded output identity `34009717b2be5aec243ce405d608c5bda001933b8f898cedf4e153caf2ba728c`, encoded transport SHA-256 `e132120a1515a8a70f5e504a6993788ff500ce8aa60f85d531c5eee0d9a1c67a`. The exact native receipt is committed as `canonical-evidence/native-replay-receipt.json`; the 19,644-byte authentic native PNG is in the separately furnished owner review ZIP, **not currently present in this PR** due to binary-upload constraints. This gap must be closed by Integration before admission, without changing the archived image bytes. A source-only checkout cannot pass `OPEN-REVIEW.cmd`, by design. Use the complete owner package, which contains the original authored files, archived PNG/receipt and supported Windows executables.

`OPEN-REVIEW.cmd` requires all original hashes, portable project source binding, the actual archived PNG, a real Windows CPU `FAULTMINE-render.exe --verify` identity replay and a GUI executable **before launching anything**. The updated source verifier fails closed under Python `-O` / `-OO` and pins original manifest bytes. None of these tests establishes physical Win32 GUI launch, correct FM-012 export/cancellation or human artistic selection. **DRAFT / HOLD** until owner ADMIT / REVISE / REJECT and separate Integration acceptance.
