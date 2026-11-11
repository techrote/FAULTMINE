# Signal Mausoleum — Residual Carrier

An original 256 × 144 pixel temporal glitch composition for the native Windows FAULTMINE instrument. A sealed communications aperture persists while its surrounding traces shear, accumulate stale scan bands and fold into chromatic residues. The result is intended as a **32-frame finite study, not a seamless loop**.

![Original opaque source image](source.bmp)

## Composition and time

The authored opaque bitmap is an original, locally constructed 16-colour signal architecture: offset horizon trusses, interrupted concentric pylons, sparse geometric optical markings, a hot occluded centre, and substantial quiet negative space. No third-party imagery, external asset path or generated service is required.

The eight ordered genome stages are: fixed row offset; red-channel offset; sawtooth band tearing (32-frame period, 8-pixel bands); triangular independent red/blue channel phase (44-frame period); one-band-at-a-time partial refresh (9-pixel bands); persistent +1-pixel trail with 208/256 decay; 16-level RGB quantization; and **12/1** semantic timeline-rate metadata. Feedback is owned by the temporal operator and replayed from frame 0; no viewport/frame clock participates in the image identity.

The progression is intentional: frames **0–8** establish the aperture and break row alignment; **9–20** distribute retained fragments and coupled trails; **21–31** permit the central signal to become a collection of residual, asynchronous impressions. Inspect **0, 4, 8, 12, 16, 20, 24, 28, 31** for the short-form contact sheet.

## Open in FAULTMINE (Windows x64)

From the **repository root**, build the current native application (see the root README), or use the accepted portable FAULTMINE executable. The source-reference path in the project is repo-root-relative. If using a portable executable outside the source tree, relink the included `source.bmp` when prompted and require its normalized source identity to match.

1. Run `python gallery/temporal/01-signal-mausoleum/verify_artwork.py` for the platform-independent integrity gate. This checks only data consistency; it is **not** native validation.
2. Start `build/Release/FAULTMINE.exe` from the repository root, or use `launch.ps1` below.
3. Use **Ctrl+Shift+O** to load `gallery/temporal/01-signal-mausoleum/signal-mausoleum.fmproj`. Alternatively, open `source.bmp` and manually import `genome.json` using the supported native workflow.
4. Confirm that the semantic timeline rate is **12/1**; seek to frame 0 and inspect the source-to-result distinction, then step to 8, 16, 24 and 31.
5. Use **Export → Frame sequence** with range **[0, 32)** (end-exclusive), stem `residual-carrier`, six-digit frame numbering, **manifest ON** and **canonical/full-resolution** mode. Leave overwrite disabled for the first export. Use a new destination outside the repository artwork package.
6. Inspect the 32 complete PNGs and FM-012 manifest. Re-export a sample of frames (0, 8, 16, 24, 31) to a second destination and independently compare decoded RGBA identities and FM-011 temporal frame identities. Test cancellation as a **separate** sequence request, checking valid partial outputs and manifest status.

### Native 32-frame replay driver (Windows)

With accepted [PR #36](https://github.com/techrote/FAULTMINE/pull/36) now on `main` (`6c56b654`), the same exact authored inputs can be exercised with **one command** from a checkout containing this package:

```powershell
.\gallery\temporal\01-signal-mausoleum\render-sequence-win.ps1 -Configuration Release
# Re-run without overwriting historical evidence:
.\gallery\temporal\01-signal-mausoleum\render-sequence-win.ps1 -Configuration Debug -OutputDirectory .\artifacts\signal-mausoleum-debug
```

This script refuses to reuse an existing output directory and refuses changed BMP/genome bytes. It invokes **the actual FAULTMINE Windows CPU renderer** for explicit frames 0–31, checks the normalized source identity and each reported decoded canonical output identity, then replays frames 31, 0, 24, 8, 16 in deliberately nonchronological order with the **read-only** `--verify` path. It also checks that verification leaves the PNG bytes unchanged. Results are 32 native PNGs plus an `native-cli-replay-audit.json` with per-frame output identity, encoded PNG transport SHA-256 and replay evidence. Failure preserves correctly labelled partial evidence; no fake frame/identity record is inserted.

**Important separation:** this native CLI authoring audit is **not** a genuine FM-012 sequence manifest, nor a proof of the FM-011 temporal frame identity. The Win32 application must still export a true FM-012 complete [0,32) sequence with canonical provenance and independently test a cancelled sequence with a partial manifest. The real PNGs and image progression also still require visual inspection. The accepted `main` renderer now implements `--frame` and `--verify`. Integration has independently rendered and replayed the authored inputs on Windows, but the **repaired author driver** still requires its own execution gate. Generic CI alone does not establish artwork completion.

**Windows acceptance gate:** native source decode and project strict parse; registry acceptance of all typed operators; exact frame replay (same frame in different request orders); 32 committed full-resolution PNGs with complete manifest and correct ordering; sample frame identity agreement; cancellation semantics; Debug/Release required tests. The stored `artwork.json` is an **authoring manifest**, deliberately **not** an FM-012 canonical export manifest.

## Reproducibility

| Contract | Value |
|---|---|
| Source | `source.bmp`, opaque 24-bit BMP, 256 × 144 |
| Source encoded SHA-256 | `cbb58573b44b78a7a162289ede71eb541689d078b795960d0504888e3a8090de` |
| Normalized FAULTMINE source identity | `03502053b9d752dd2db92329108c2d1ed1c769e00db8a08f6ce7b9331992dc75` |
| Genome identity | `ac6651f58f3ee3b40d3852c6fa9a1435e5de2e6341cd7f5f894306d52695462c` |
| Root seed | `a1b2c3d4e5f60718` |
| Schema / engine / project | 1 / 1 / 2 |
| Exact frame range | `[0,32)` |
| Semantic rate | 12/1 |

**Current admission status (11 Nov 2026):** native Windows Integration [run 42940066799](https://github.com/techrote/FAULTMINE/actions/runs/42940066799) generated all **32 genuine 256 × 144 canonical CPU frames**, and independently verified nonchronological Release/Debug decoded pixel replays. Author follow-up independently decoded all 32 PNGs, confirmed 32 unique decoded identities and visually inspected their real presentation. See [NATIVE-EVIDENCE.md](NATIVE-EVIDENCE.md) for pinned identities, exact artifacts and visual limitations. The source/genome/project and initial `artwork.json` remain as authored; its initial pending export status is superseded by this append-only evidence note. To produce a truth-labelled 3 × 3 contact sheet from the artifact, run `python gallery/temporal/01-signal-mausoleum/verify_native_frames.py path/to/artifact/temporal --contact path/to/new-contact.png`. **Still pending:** native GUI project/relink and genuine FM-012 complete/cancelled sequence manifests with FM-011 frame IDs, independent aesthetic acceptance and an execution of the repaired author driver. Keep PR #37 Draft; CLI audit alone is not FM-012 GUI export.
