# MOTH ROM / 07 — Chitin Memory

An entomological specimen plate made from **an invented electronic moth**, not an appropriated biological photograph. The moth is symmetrical until its recording substrate fails: logical address bursts displace short pixel runs, a red-channel spatial delay unseats its contour, seeded bit bursts perforate individual rectangular cells, and scanline jitter and blue bitplane substitution corrupt its wings. Final integer quantization and Bayer dither leave an austere mineral surface.

The source is deliberately legible as a complete specimen rather than relying on random data to generate composition. Thin wing veins radiate from a segmented amber thorax; three nested optical spots on each side become useful visual landmarks for evaluating which errors are actually structural. A narrow registration system, archival legend and asymmetric memory scars situate the piece as a damaged scientific plate. The design is unrelated to the communications-aperture theme of temporal PR #37.

## Files and authority

- `source.bmp` — original, embedded, opaque, uncompressed **384 × 256** RGB source. All pixels were drawn from deterministic primitives for this commission; no image model, stock artwork, remote asset or external font was used.
- `genome.json` — actual FAULTMINE v1 compact canonical typed genome; seven enabled v1 operators and an explicit 64-bit root seed.
- `moth-rom.fmproj` — FM-010 project schema v2 with exact source binding and one manual-root lineage record.
- `artwork.json` — authoring inventory/identity record. **Not an FM-012 native export manifest.**
- `verify_artwork.py` — read-only, Python-stdlib static qualification for BMP bytes/pixels, source/seed/genome/project identities, typed fault-stack geometry and local metadata. It validates LF-canonical JSON content even if Git for Windows transparently checked out text as CRLF; duplicate JSON keys, bare CR, extra data and changed identities remain rejected.
- `render-win.ps1` — native Windows exact-frame-0 CPU render and **read-only exact decoded-RGBA8 replay** through accepted `--verify` CLI, with optional independent Debug/Release cross-check, immutable source/genome preflight, no-overwrite evidence and a separate author replay receipt (not an FM-012 export manifest).

Source encoded SHA-256: `41a2be8d45d83c4b15e09ce9d1846256cb89f71d48551f592d1e78f772670e48`

Normalized RGBA8 source identity: `49aca43e0330c82dec3d579f42f6cca3728603e5155cec434bf0ee81303fff4e`

Canonical genome SHA-256: `fa4fda19e0701619a57b2c3eb0ee422ebf8f9f6911537ccc9ee912c125bf72e4`

Root seed: `0d07c1cada55e7a1`

## Reproduction and acceptance

From the repository root, the source can be checked without third-party Python packages:

```powershell
python .\gallery\originals\01-moth-rom\verify_artwork.py
.\gallery\originals\01-moth-rom\render-win.ps1 -Configuration Release
# When both native configurations are built:
.\gallery\originals\01-moth-rom\render-win.ps1 -Configuration Release -VerifyWithOtherConfiguration -OutputDirectory .\artifacts\moth-rom-crosscheck
```

The runner invokes **the repository's actual `FAULTMINE-render.exe`** on accepted main after the merge of PR #36, never a Python approximation or D3D proxy. It renders explicit **frame 0**, then invokes the native `--verify --frame 0` operation against the resulting PNG, checks matching normalized source/output identities and proves the expected PNG was not changed. With `-VerifyWithOtherConfiguration` it additionally verifies the identical decoded RGBA8 image using the other native configuration. An output-directory-local `native-replay-receipt.json` records the measured output identity only after those checks pass; this is an **author qualification receipt, not a native FM-012 export sidecar**. The script never rewrites prior outputs or original source/genome/project. Use `FAULTMINE.exe` to open `moth-rom.fmproj`; if launched elsewhere, relink to this exact `source.bmp` only if the normalized source identity matches.

**Current acceptance:** structural and source/model identity can be checked independently, but the updated native runner has **not yet executed** on this branch. Its presence is not evidence of a canonical post-fault PNG, native decoded output identity, Debug/Release equivalence or visual quality. Integration PR #40 separately qualifies pinned original inputs on actual Windows CI; once canonical output is available, visually inspect it, compare strict replay and evaluate the GUI project/relink and genuine FM-012 export. Never treat the untouched source as final post-fault art; do not merge on generic CI alone.

**Artist's intention:** the image should survive its own corruption as a readable biological diagram. The space between wing veins, along with the asymmetric damaged rectangular cells, is as important as the colour. Reject the preset or revise a new branch if native visual inspection yields incoherent noise rather than damaged structure.
