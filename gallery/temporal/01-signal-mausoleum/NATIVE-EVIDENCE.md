# Signal Mausoleum — native temporal evidence (2026-11-11)

**Status:** native CPU frame-sequence proof available; GUI FM-012 and independent artistic admission outstanding. This note describes actual pinned Windows output and is not itself an FM-012 export manifest.

## Fixed semantic input

- Original author package (source, genome, project): FAULTMINE PR #37, immutable authored head 65f4048f87121a2d36bef38d78518a04598f52eb.
- Source BMP: 256 × 144; encoded SHA-256 cbb58573b44b78a7a162289ede71eb541689d078b795960d0504888e3a8090de; normalized source identity 03502053b9d752dd2db92329108c2d1ed1c769e00db8a08f6ce7b9331992dc75.
- Genome identity ac6651f58f3ee3b40d3852c6fa9a1435e5de2e6341cd7f5f894306d52695462c, root seed a1b2c3d4e5f60718, frame indices 0–31, rate 12/1.
- Renderer contract: accepted main 6c56b6543f9cda5330408d05eaf47da03485a94f (merged PR #36's explicit-frame canonical CLI and read-only pixel verifier).

## Genuine Windows execution

Integration's [native gallery CI run 42940066799](https://github.com/techrote/FAULTMINE/actions/runs/42940066799) **passed** its actual Windows gallery job, not merely generic project CI. Download [artifact 13820860462](https://github.com/techrote/FAULTMINE/actions/runs/42940066799/artifacts/13820860462) (FAULTMINE-gallery-native-evidence), whose GitHub-provided archive digest is sha256:9e5b7adaf3f487be4aeca728e0c5c27ebca3b7d1009d99d5597aebe6bb24b06a.

Inside that archive, temporal/ contains **32 actual full-resolution 256 × 144 native canonical PNGs** and integration-native-cli-audit.json. The audit records 32 explicit sequential frame addresses, 32 per-image canonical decoded RGBA8 identities, 32 PNG transport SHA-256 values, and independent out-of-order Release and Debug native verifier probes at 31,0,24,8,16. The audit itself has SHA-256 137ae2c4da223cbc215b9753725ab0a879cdb626525d31f394a8e758ee9d04c0 and explicitly declares fm012_gui_manifest: false.

Representative canonical identities (not PNG transport hashes):

| Frame | Decoded canonical RGBA8 identity |
| ---: | --- |
| 0 | f48675366a2af79421d73f2eb72cbe830da26ecc486903c308c27320e7c4734d |
| 8 | 74099aae4c865f81f75934b4deaf37e01476d05ae6eea1023d55f3ef10645ead |
| 16 | 17bd3d756c8ac5127e508af1f14704870adf56d690dee95b29f1e066acb9b863 |
| 24 | 118031790000641d0626fd5b9219cd14a8139c4e337f6a78029cf605891b2e41 |
| 31 | 371b14287796e29101b8016eaeccdcd35ac3384a653654a028b56064da5c8588 |

## Independent cross-platform reinspection

The artwork-only verify_native_frames.py decodes all 32 Windows PNGs using only Python's standard library. It verifies PNG chunk CRCs and row filters, PNG transport checksums, decoded full-resolution straight-RGBA8 identities, strict sequential addresses, seed/source/genome, and 32 distinct pixel results. It does **not** reimplement FAULTMINE's fault engine.

Command from a repository checkout with the extracted CI artifact:

    python gallery/temporal/01-signal-mausoleum/verify_native_frames.py path/to/artifact/temporal --contact path/to/new-contact.png

The script outputs a **presentation-only** 3 × 3 contact sheet from actual canonical frames in row-major order 0,4,8 / 12,16,20 / 24,28,31, with 8-pixel gutters and no rescaling or invented frames. The verified output produced during author follow-up was SHA-256 bbc7472800e697e6659cb348f55b6a622756cf85fa33a4ee93387aa903d359b5. The contact sheet itself is not a canonical frame, FM-012 manifest, or a substitute for the 32 PNGs.

The independent Python readback **passed all 32 PNG encoded-byte hashes, all 32 decoded RGBA identities, and 32/32 distinct frame identities**. A deliberately corrupted PNG was rejected by its CRC check. The author visually inspected a contact sheet of actual Windows frames: the central hot-red aperture remains legible; green/cyan traces and magenta phase offsets migrate across it, with lateral band tears progressively decoupling the centre from the frame. Changes are structural and temporal, not simply colour flicker. The trajectory is an oscillating/displaced signal, **not** a claim that the aperture fully disappears. This is author inspection, not independent human aesthetic acceptance.

## Driver repairs and remaining gates

The same PR repairs the author PowerShell driver interpolation parser defect observed in Windows run 42939377682, and permits Git-for-Windows CRLF checkout of canonical JSON **without weakening the logical LF-canonical identity requirement**. The companion cross-platform source/genome/project validator uses the same strict logical-content distinction.

The independent Integration workflow did **not** exercise the repaired author PowerShell driver. That driver needs a separate fresh-output Windows run before declaring it qualified. Neither the Integration audit nor the CLI PNG replay is a genuine FM-012 GUI sequence export. Remaining gates: load/relink project schema v2 in the native GUI, export a **complete** FM-012 [0,32) sequence with per-frame temporal identities, cancel a separate sequence and inspect the **partial** manifest, and secure independent visual admission. Keep PR #37 **Draft** pending those gates. No existing artwork or engine semantics were changed.
