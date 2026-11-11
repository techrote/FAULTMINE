# ART-EVOL-007 — Fish Bones in the Receiver

**Status: executable canonical quarry candidate; not yet a mined, selected or accepted artwork.** This branch contains an original authored input image constructor, an immutable snapshot of the accepted FM-005 addressing genome, a fixed-index Windows-native mining session and independent source-provenance tests. **No faulted specimen, visual ranking, canonical output hash, or native completion is claimed.**

## Compositional hypothesis

An elongated teal bone-fish threads a measured dark sea from lower left to upper right. Repeated pale ribs imply a single intact body, while disconnected echolocation arcs and three rust registration marks imply an instrument that cannot locate its subject. The artistic problem is to break the reliable relationship between skeleton and coordinate system *without erasing the animal's diagonal movement*. Address-offset, logical mask, tile permutation and bounded burst operators should make it possible to examine displaced ribs, false head/tail positions and contradictory sonar echoes.

The source was constructed independently without any third-party image, fonts, photo, model output or network asset. Its raw, unfaulted RGB BMP was generated and visually inspected locally. **The generated source preview is not a FAULTMINE canonical glitch specimen.** Do not evaluate mutation quality until the actual selected PNGs have been decoded and inspected.

This is distinct in subject and intended form from the seismogram registers in Evolution 06, Shearwing's flight telemetry and the Orchard/Glass studies. No assertion about a descendant's aesthetics follows from the source.

## Accepted fixed parent

- Parent: `examples/fm005-addressing-genome.json` at exact accepted `main` `827f03c3cb761336a3e3c91e822bb9f03b2a451f`.
- Immutable copy: `parent-genome.json`; identical Git blob `64b0756262db458e276861dad84edbd964571558`.
- Accepted genome v1 / engine-contract v1, parent root seed `0123456789abcdef`; four existing FM-005 addressing operators. No manually authored or invented mutation results.
- Source constructor: `make_source.py`, Python 3 standard library; dimensions 384×256, opaque RGB24 BMP.
- Reconstructed BMP encoded SHA-256: `ca6883cd3fc73526abdcba96671acf83f130005ed2b17d2f87e48c35b693f6cc`.
- Independently decoded canonical source RGBA8 identity: `76352a4b6b859a3605568e8b3a9a7f09caeab66bda1418c290d075d54026e6af`.
- Search seed `7e41a9c3d2b8065f`; mutation policy 1, radius **high**, indices **[0,96)**, explicit semantic frame **0**.
- Full-resolution **canonical** CPU rendering only; worker threads **4**; descriptor v1 inclusive near threshold **8192**; novelty-selection request **10**, with full-resolution selected stills. Proxy mode is intentionally not used.

## Reproduce on an actual Windows x64 MSVC checkout

From the repository root with Python 3, Git, and the normal MSVC toolchain:

```powershell
.\scripts\configure.ps1
.\scripts\build.ps1 -Configuration Debug
.\scripts\test.ps1 -Configuration Debug
.\scripts\build.ps1 -Configuration Release
.\scripts\test.ps1 -Configuration Release
python .\gallery\evolution\07-fish-bones-in-receiver\make_source.py --write
python .\gallery\evolution\07-fish-bones-in-receiver\preflight.py
.\gallery\evolution\07-fish-bones-in-receiver\mine.ps1 -OutputDirectory .\artifacts\fish-bones-07 -Threads 4
```

The driver checks the unchanged accepted parent, independently reconstructs the authored BMP, invokes `FAULTMINE-batch.exe` with explicit address parameters, requires a complete strict FM-014 manifest with 96 monotonically ordered records and no failures, checks genome/pixel/near-representative diversity, verifies a one-thread `--resume` generates the **identical manifest SHA-256**, and performs read-only **Debug and Release** `FAULTMINE-render.exe --verify` on every selected full-resolution PNG. It also renders an ordinary parent baseline PNG for comparison, independently verified in Debug. The native tool emits its own GUI-loadable `batch.fmproj`, `selected-contact-sheet.png`, selected stills and FM-012 sidecars; the driver adds `native-audit.json` only after successful checks. Candidate identities and counts are read from the actual native manifest, never guessed.

To resume a genuinely partial native run **against exactly the same address**, add `-Resume` with the same `-OutputDirectory`. Without that switch, an existing output directory is refused. A failed or cancelled batch is not a completed search; preserve its partial manifest, then use the verified resume contract.

## Curation and admission gate

The deterministic novelty picker provides *coverage*, not artistic judgement. After native execution, inspect all ten genuine canonical selected stills and the actual contact sheet at original resolution. Reject visually equivalent pixel structures, simple hue swaps, or collapse into unreadable noise even if the descriptors consider them distinct. Prefer a candidate where the fish's head/tail orientation survives but its rib rhythm or sonar registration changes significantly. Archive the selected native still **unchanged**, its exact canonical genome, source/parent seed and identities, full batch manifest, importable `batch.fmproj`, complete native receipts and a candid comparative selection note. Do not claim those deliverables exist on this branch yet.

## Evidence and limitation

Local Python 3 reconstructed and decoded the opaque 384×256 source; the image and source preview were actually inspected. Source-genome preflight includes positive identities, single-pixel corruption detection, strict schema and parent Git blob comparison. The installed execution environment is Linux, has neither `wine` nor `pwsh`, and cannot execute the actual Windows binaries. Ordinary GitHub PR CI tests the engine but **does not run this custom quarry script**. No native batch, preview fault output, contact sheet or subjective descendant inspection is asserted.

**Integration next action:** run `mine.ps1` on a supported Windows worker; attach/publish the resulting native evidence; inspect actual images; only then select and curate the final specimen. Keep this PR Draft until that gate. All prior art and shared engine/editor/UI code remain untouched.
