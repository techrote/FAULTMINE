# The Silent Bell / 04 — An Instrument for a Drowned Signal

**Status:** original source and typed genome ready for Windows-native canonical qualification. The post-fault pixels have **not** yet been rendered or visually accepted.

A suspended, asymmetrical brass bell dominates a cold acoustic chart. A displaced vermilion clapper hangs inside its dark throat; the lower lip breaks into a black slit. Six incomplete arcs advance into the empty right third while registration ticks and boxed coordinates count a sound that cannot be heard. The artwork asks what remains of an instrument when its echoes no longer agree with its body.

The **actual authored 384×256 source** was inspected at nearest-neighbour enlargement: the brass lip, suspended apparatus, red clapper and staggered cyan acoustic arcs are distinct. This describes only the source. It differs materially from the earlier moth specimen, railway carriage and fungal canopy commissions. It is original geometric pixel construction, with no third-party images, textures, fonts, network resources or borrowed source artwork.

## Deliberate fault composition

Seven ordered registered operators: 10-pixel horizontal memory bands repeated every eight groups with +2 band source delta; nine logical address bursts of 93 pixels displaced by at most 212; red-blue channel shift (3,-1); scanline jitter of at most two pixels; seven 19×5 green-bit bursts (XOR 32); 24-level RGB quantization; 16-level Bayer4 ordered dithering. The design intent is acoustic discontinuity with a still-recognizable casting, **not** a claimed canonical observation.

## Frozen author inputs

- source.png: indexed opaque 4-bit PNG, 384×256. Encoded SHA-256: be6f9c19e0b24544e1fc8883f55a041e06fd309c52bea5771c0bbff8b730da04.
- Normalized FW-003-style FAULTMINE source ID: 275bc5c264376ac81442ca3d5b399cff01b33449c21906be2dc140fe8085e747.
- Genome schema 1, engine contract 1, exact LF canonical genome SHA-256: dc0340692b48259ed77fbf751ed692360c0345945bbb3b50f975ec0c3d8bde61.
- Root seed: 9f6b31ca0d42e857. Explicit canonical frame: 0.
- Project schema 2; one manual-root favourite with full embedded genome, no fabricated mutation lineage.
- artwork.json is a preliminary provenance record; the unknown canonical output ID remains null.

## Independent verification

Run from the repository root on any Python 3 machine:

    python gallery/originals/04-the-silent-bell/verify_artwork.py

That verifies actual PNG CRC/filter/indexed-RGBA identity, frozen source SHA, exact typed genome hash, normalized project/lineage consistency, and corrupted-input/duplicate-key rejection. It is **source-only** and does not execute FAULTMINE.

On supported Windows with the existing Debug/Release executables:

    .\scripts\configure.ps1
    .\scripts\build.ps1 -Configuration Debug
    .\scripts\build.ps1 -Configuration Release
    .\gallery\originals\04-the-silent-bell\render-win.ps1 -OutputDirectory .\artifacts\silent-bell-native -CrossCheckDebug

This fail-closed driver renders the actual CPU/WIC canonical frame zero, read-only verifies decoded output with both configurations, and writes an author CLI receipt. The receipt is not an FM-012 GUI export. It refuses to overwrite nonempty evidence folders.

To inspect the project in the desktop application, use Ctrl+Shift+O on the included the-silent-bell.fmproj, then explicitly relink to source.png if required. Reopening must preserve the normalized source identity.

**Acceptance gate:** obtain genuine native PNG and source/output IDs; inspect those rendered pixels; test physical Win32 source relinking and GUI FM-012 export; then propose archival output in a guarded incremental PR. This branch is Draft and artwork has no owner aesthetic acceptance. Do not substitute a D3D proxy or silently rewrite the authored input.
