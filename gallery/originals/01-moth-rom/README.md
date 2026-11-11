# MOTH ROM / 07 — Chitin Memory

An entomological specimen plate made from **an invented electronic moth**, not an appropriated biological photograph. The moth is symmetrical until its recording substrate fails: logical address bursts displace short pixel runs, a red-channel spatial delay unseats its contour, seeded bit bursts perforate individual rectangular cells, and scanline jitter and blue bitplane substitution corrupt its wings. Final integer quantization and Bayer dither leave an austere mineral surface.

The source is deliberately legible as a complete specimen rather than relying on random data to generate composition. Thin wing veins radiate from a segmented amber thorax; three nested optical spots on each side become useful visual landmarks for evaluating which errors are actually structural. A narrow registration system, archival legend and asymmetric memory scars situate the piece as a damaged scientific plate. The design is unrelated to the communications-aperture theme of temporal PR #37.

## Files and authority

- `source.bmp` — original, embedded, opaque, uncompressed **384 × 256** RGB source. All pixels were drawn from deterministic primitives for this commission; no image model, stock artwork, remote asset or external font was used.
- `genome.json` — actual FAULTMINE v1 compact canonical typed genome; seven enabled v1 operators and an explicit 64-bit root seed.
- `moth-rom.fmproj` — FM-010 project schema v2 with exact source binding and one manual-root lineage record.
- `moth-rom-canonical.png` — 384×256 **lossless RGB8 re-encoding of the genuine Windows-native CPU output**. All decoded RGBA8 bytes match the native verified image; encoded container checksum differs from the original WIC PNG.
- `verify_canonical.py` — stdlib-only read-only PNG CRC, strict archived decode, decoded RGBA8 identity and corruption-negative checks.
- `artwork.json` — authoring and measured canonical identity record. **Not an FM-012 native export manifest.**
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
python .\gallery\originals\01-moth-rom\verify_canonical.py
.\build\Release\FAULTMINE-render.exe --verify --frame 0 .\gallery\originals\01-moth-rom\source.bmp .\gallery\originals\01-moth-rom\genome.json .\gallery\originals\01-moth-rom\moth-rom-canonical.png
.\gallery\originals\01-moth-rom\render-win.ps1 -Configuration Release
# When both native configurations are built:
.\gallery\originals\01-moth-rom\render-win.ps1 -Configuration Release -VerifyWithOtherConfiguration -OutputDirectory .\artifacts\moth-rom-crosscheck
```

The runner invokes **the repository's actual `FAULTMINE-render.exe`** on accepted main after the merge of PR #36, never a Python approximation or D3D proxy. It renders explicit **frame 0**, then invokes the native `--verify --frame 0` operation against the resulting PNG, checks matching normalized source/output identities and proves the expected PNG was not changed. With `-VerifyWithOtherConfiguration` it additionally verifies the identical decoded RGBA8 image using the other native configuration. An output-directory-local `native-replay-receipt.json` records the measured output identity only after those checks pass; this is an **author qualification receipt, not a native FM-012 export sidecar**. The script never rewrites prior outputs or original source/genome/project. Use `FAULTMINE.exe` to open `moth-rom.fmproj`; if launched elsewhere, relink to this exact `source.bmp` only if the normalized source identity matches.

**Current acceptance:** Native Windows CPU output and read-only Debug/Release pixel replay of pinned authored inputs succeeded in Integration CI. A canonical-pixel-identical PNG is now committed in the artist's gallery package, with an additional independent read-only stdlib PNG decoder/identity audit. The specific committed re-encoded PNG still needs exact-head native `--verify`; GUI project load/relink, genuine FM-012 export and independent artistic acceptance remain pending. The author runner itself has not been re-executed on this exact PR head; generic CI alone does not close these gates.

## Verified native output — first author review

Integration's [Windows gallery workflow run 42939377682](https://github.com/techrote/FAULTMINE/actions/runs/42939377682) rendered the original author commit `4fcf670ff13fe9669e31d5535f767d6def3750e9` on real MSVC/WIC/CPU, and its **original-art lane passed** exact decoded RGBA8 replay with both Release and Debug. The full multi-artwork workflow was red because the separate temporal driver had a PowerShell parser error; that failure must not be presented as a failure of the moth's canonical pixels.

- Native full-resolution canonical PNG: `original/moth-rom-canonical.png` in [artifact 13820331534](https://github.com/techrote/FAULTMINE/actions/runs/42939377682/artifacts/13820331534).
- Raw decoded canonical output identity: `171e395bac997e35d740f400cbbce3e9fdb556056f5be7fd45eef5d158536c7c`.
- Encoded PNG transport SHA-256: `70f8ed386a18f8cdb76212ddf7ec0799b8dfba8ab19f2b3d24f0e511ff0f8b81`.
- Explicit semantic frame `0`; dimensions `384×256`; source and genome match this package exactly.
- `native-qualification.json` preserves the native audit and exact source/engine/CI association; it is an **author receipt, not an FM-012 export manifest**.

The actual native PNG was downloaded and visually inspected alongside the authored BMP. The moth remains strongly recognizable: two large wing fields and the central segmented body survive, while cyan/red contour splits, interrupted lettering, horizontal scars and granular interference create a coherent damaged-specimen aesthetic. A fair limitation is that upper specimen labels lose legibility under the selected full-stack faults. An independent artistic acceptance, GUI project/relink operation and native FM-012 export have **not** yet been performed. The original WIC-encoded 31,714-byte PNG remains in the cited CI artifact (SHA-256 `70f8ed386a18f8cdb76212ddf7ec0799b8dfba8ab19f2b3d24f0e511ff0f8b81`). This gallery's smaller 16,337-byte PNG was losslessly re-encoded from those **actual native pixels**, and decoded RGBA8 identity was independently rechecked before committing; its transport SHA-256 is `59488382c0ad776ee00dc1419d11ba1ddbf79871b161059037a6f4234af33088`. It is not a newly simulated render, nor a substitute for an exact-head native `--verify` run.

**Artist's intention:** the image should survive its own corruption as a readable biological diagram. The space between wing veins, along with the asymmetric damaged rectangular cells, is as important as the colour. Reject the preset or revise a new branch if native visual inspection yields incoherent noise rather than damaged structure.
