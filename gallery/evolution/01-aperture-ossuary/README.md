# Aperture Ossuary — evolutionary quarry 01

**Status: reproducible native-mining candidate; no completed batch or selected artwork admitted yet.** Original static source imagery for an **architectural dislocation** search: retain the cold circular aperture while exploring broken colonnades, interrupted symmetry, raster address fractures and unexpected spatial rhythms. This differs deliberately from the pending temporal Signal Mausoleum artwork. The native contact sheet is a navigation aid, not a canonical specimen, nor an aesthetic quality score.

## Immutable input and exact search address

- **Source:** `source.bmp`, original 384×240 opaque 24-bit RGB, authored with integer geometry only; no photographs, external fonts/assets, or borrowed art. Encoded SHA-256: `46dc8809a764257cfb61fbec1f85663e64211080a4611606f85fc340ea44ab09`. Normalized straight-RGBA8 image identity, independently calculated using accepted `FAULTMINE-SOURCE-RGBA8-v1` bytes: `0023ceb14294cf69b7db062a1afc255ebaa8cc6e2a0119f4bb81d91be053125a`. **Actual Windows WIC normalization remains to be checked.**
- **Accepted immutable parent:** byte-identical copy `parent-genome.json` of `examples/fm005-addressing-genome.json` at main `c5f8ce3ea595795d5aca8a76771005eb5d006f52`, Git blob `64b0756262db458e276861dad84edbd964571558`; canonical genome identity / SHA-256 `01217a9e77ea4cd446b7f6142ab6ea71db9bedbcca48425a02d60ea15849524d`. Four accepted addressing operators: offset, mask, tile permutation, address burst. Parent root seed `0123456789abcdef`.
- **Independent descendant address:** mutation seed `4d13b09ec72a8851`, indices `[0,384)`, high radius, semantic frame 0, canonical full resolution (never proxy), inclusive near-duplicate descriptor L1 threshold 8192, up to 12 diverse novelty representatives, 4 workers by default. Version contracts: engine/genome/mutation-policy/batch-manifest/descriptor all 1. Worker count must not affect semantic output. `--resume` only trusts revalidated source/genome/address/pixels/descriptors, per FM-014.

## Run on Windows

Build the existing Release executable via `scripts/configure.ps1` and `scripts/build.ps1 -Configuration Release`, or use the compatible self-contained portable `FAULTMINE-batch.exe`. From this folder:

~~~powershell
.\mine.ps1 -Threads 4 -OutDir "$env:TEMP\FAULTMINE-Aperture-Ossuary-01"
# Verified re-use after Ctrl+C, or reproducibility test with alternate thread count:
.\mine.ps1 -Threads 1 -OutDir "$env:TEMP\FAULTMINE-Aperture-Ossuary-01" -Resume
# If using a portable native binary rather than a repo build:
.\mine.ps1 -BinaryPath 'C:\path\FAULTMINE-batch.exe' -OutDir 'C:\path\quarry-01'
~~~

The accepted native miner, not this wrapper, is responsible for:

- `batch.fmbatch.json`: strict complete/partial manifest with all candidate addresses, pixel identities, descriptors, dedupe and novelty data;
- `candidates/candidate_<20-digit-index>.png`: resumable canonical candidate cache PNGs;
- `batch.fmproj`: GUI-openable v2 lineage;
- `selected-contact-sheet.png` and its provenance sidecar (presentation contact sheet, not a canonical specimen);
- `selected/`: up to 12 full-resolution canonical still PNGs and export manifests.

The results live only in the chosen **external** output directory, never as fabricated tracked finished art. The launcher checks immutable input SHA-256 and essential completed-manifest invariants; it exits rather than silently accepting a wrong identity, an incomplete result or a partial cancellation. Existing output requires explicit `-Resume`.

A dependency-free independent read-only decoder/audit is supplied:

~~~powershell
python .\qualify-results.py 'C:\path\quarry-01' --all
~~~

This checks both pinned inputs, the native manifest, candidate/selected counts, uniqueness, PNG chunk CRC and row filtering, and independently hashes every decoded RGBA8 candidate against its recorded canonical identity. It does **not** re-run the C++ fault engine or replace Debug/Release CI and a Windows GUI evaluation.

## Artistic acceptance gate

Inspect actual native contact sheet and canonical stills. Select an aesthetically strong result only after comparing aperture readability, spatial rhythm, void versus information density, structural contrast and visually meaningful novelty. Reject excessive noise and pixel-different but visually trivial variants. Record chosen descendant index, exact source/genome/seed/frame, canonical output identity, selection provenance and why it is artistically different. Re-run the winner through native full-resolution renderer and exact `--resume` comparison; verify GUI import and standard CI on the exact PR head. Keep this artwork branch **Draft** until those results are available. No mine has been run in the authoring container (no Windows executable runtime), and no selection counts, sample outputs or aesthetic observations are claimed as completed results.

Original source composition is produced by the author for this package; old gallery artifacts and the FAULTMINE engine are unchanged.
