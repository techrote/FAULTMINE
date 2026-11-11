# MOTH ROM / 07 — Chitin Memory

An entomological specimen plate made from **an invented electronic moth**, not an appropriated biological photograph. The moth is symmetrical until its recording substrate fails: logical address bursts displace short pixel runs, a red-channel spatial delay unseats its contour, seeded bit bursts perforate individual rectangular cells, and scanline jitter and blue bitplane substitution corrupt its wings. Final integer quantization and Bayer dither leave an austere mineral surface.

The source is deliberately legible as a complete specimen rather than relying on random data to generate composition. Thin wing veins radiate from a segmented amber thorax; three nested optical spots on each side become useful visual landmarks for evaluating which errors are actually structural. A narrow registration system, archival legend and asymmetric memory scars situate the piece as a damaged scientific plate. The design is unrelated to the communications-aperture theme of temporal PR #37.

## Files and authority

- `source.bmp` — original, embedded, opaque, uncompressed **384 × 256** RGB source. All pixels were drawn from deterministic primitives for this commission; no image model, stock artwork, remote asset or external font was used.
- `genome.json` — actual FAULTMINE v1 compact canonical typed genome; seven enabled v1 operators and an explicit 64-bit root seed.
- `moth-rom.fmproj` — FM-010 project schema v2 with exact source binding and one manual-root lineage record.
- `artwork.json` — authoring inventory/identity record. **Not an FM-012 native export manifest.**
- `verify_artwork.py` — read-only, Python-stdlib static qualification for BMP bytes/pixels, source/seed/genome/project identities, typed fault-stack geometry and local metadata.
- `render-win.ps1` — supported native Windows exact-frame-0 render route from a compiled Release/Debug CLI, with opt-in separate output directory and immutable source/genome.

Source encoded SHA-256: `41a2be8d45d83c4b15e09ce9d1846256cb89f71d48551f592d1e78f772670e48`

Normalized RGBA8 source identity: `49aca43e0330c82dec3d579f42f6cca3728603e5155cec434bf0ee81303fff4e`

Canonical genome SHA-256: `fa4fda19e0701619a57b2c3eb0ee422ebf8f9f6911537ccc9ee912c125bf72e4`

Root seed: `0d07c1cada55e7a1`

## Reproduction and acceptance

From the repository root, the source can be checked without third-party Python packages:

```powershell
python .\gallery\originals\01-moth-rom\verify_artwork.py
.\gallery\originals\01-moth-rom\render-win.ps1 -Configuration Release
```

The runner invokes **the repository's actual `FAULTMINE-render.exe`**, never a Python approximation or D3D proxy. The ordinary three-path CLI exports canonical **frame 0**. The runner logs stdout and encoded-PNG SHA-256 only as transport evidence; this is intentionally not a claim of canonical decoded pixel identity. Capture the actual `output image identity` printed by the native engine, and independently replay/verify the decoded PNG using Integration's proposed PR #36 `--verify` command once accepted. Use `FAULTMINE.exe` to open `moth-rom.fmproj`; if launched elsewhere, relink to this exact `source.bmp` only if the normalized source identity matches.

**Current acceptance:** structural and source/model identity can be checked independently, but Windows WIC decoding, native render, final canonical PNG, image evaluation and exact output identity have **not** been performed in this authoring environment. Never treat the untouched source preview as final post-fault art. Integration must run native Release, exact PNG replay and verify output IDs before accepting this Draft PR; do not merge on generic CI alone.

**Artist's intention:** the image should survive its own corruption as a readable biological diagram. The space between wing veins, along with the asymmetric damaged rectangular cells, is as important as the colour. Reject the preset or revise a new branch if native visual inspection yields incoherent noise rather than damaged structure.
