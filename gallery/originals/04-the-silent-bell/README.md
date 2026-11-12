# The Silent Bell / 04 — An Instrument for a Drowned Signal

**Status:** genuine Windows-native CPU/WIC frame-0 render and Debug/Release replay verified. The unmodified native PNG is now committed in this artwork subtree, byte-identical to the verified Windows artifact. Physical Win32/FM-012 and owner artistic admission remain pending.

A suspended, asymmetrical brass bell dominates a cold acoustic chart. A displaced vermilion clapper hangs inside its dark throat; the lower lip breaks into a black slit. Six incomplete arcs advance into the empty right third while registration ticks and boxed coordinates count a sound that cannot be heard. The artwork asks what remains of an instrument when its echoes no longer agree with its body.

The **actual authored 384×256 source** was inspected at nearest-neighbour enlargement: the brass lip, suspended apparatus, red clapper and staggered cyan acoustic arcs are distinct. This describes only the source. It differs materially from the earlier moth specimen, railway carriage and fungal canopy commissions. It is original geometric pixel construction, with no third-party images, textures, fonts, network resources or borrowed source artwork.

## Deliberate fault composition

Seven ordered registered operators: 10-pixel horizontal memory bands repeated every eight groups with +2 band source delta; nine logical address bursts of 93 pixels displaced by at most 212; red-blue channel shift (3,-1); scanline jitter of at most two pixels; seven 19×5 green-bit bursts (XOR 32); 24-level RGB quantization; 16-level Bayer4 ordered dithering. The design intent is acoustic discontinuity with a still-recognizable casting, **not** a claimed canonical observation.

## Observed canonical output (not a proxy)

The Windows-native 384×256 PNG was retrieved from [exact-head qualification run 42969432120](https://github.com/techrote/FAULTMINE/actions/runs/42969432120/artifacts/13832958088) and independently decoded and visually inspected. Its broad bell silhouette, disconnected red clapper and broken turquoise acoustic arcs remain legible; high-contrast horizontal scars cross the bronze edges, with distinct cyan/magenta channel registration and intermittent noise in the right third. The artwork reads as an acoustic instrument interrupted by a damaged signal rather than an unrecognizable image. That is a critical visual observation, not physical GUI acceptance.

- Native Windows output ID (normalized 384×256 RGBA8): `d1778bc3d688e5fa5694485954c268cce88c54cc033a6933d3edf2311cf35b8e`.
- Exact original native PNG SHA-256: `e9b492eddbc4d16ee528c18a236b0930c278aa8909ecdca3d68ef07c4e1d657a`.
- Workflow run/artifact: `42969432120` / `13832958088`, member `original-04/silent-bell-canonical.png`.
- Read-only Debug/Release CPU replays: PASS. The attached `native-replay-receipt.json` copies the real evidence receipt; *no PNG has been re-encoded*.
- The exact Windows-generated PNG is committed and retained in this gallery package; the GitHub Actions artifact provides independent test provenance but may expire.

## Frozen author inputs

- source.png: indexed opaque 4-bit PNG, 384×256. Encoded SHA-256: be6f9c19e0b24544e1fc8883f55a041e06fd309c52bea5771c0bbff8b730da04.
- Normalized FW-003-style FAULTMINE source ID: 275bc5c264376ac81442ca3d5b399cff01b33449c21906be2dc140fe8085e747.
- Genome schema 1, engine contract 1, exact LF canonical genome SHA-256: dc0340692b48259ed77fbf751ed692360c0345945bbb3b50f975ec0c3d8bde61.
- Root seed: 9f6b31ca0d42e857. Explicit canonical frame: 0.
- Project schema 2; one manual-root favourite with full embedded genome, no fabricated mutation lineage.
- artwork.json records the real Windows-native output identity, the committed `silent-bell-canonical.png` path and the independent CI artifact location.

## Independent verification

Run from the repository root on any Python 3 machine:

    python gallery/originals/04-the-silent-bell/verify_artwork.py

That verifies source PNG CRC/filter/indexed-RGBA identity, frozen source SHA, exact typed genome hash, normalized project/lineage consistency, corrupted-input/duplicate-key rejection, recorded Windows receipt, and the **committed** canonical PNG encoded hash and decoded RGBA8 identity. It does **not** execute FAULTMINE on the local machine.

To independently compare a separately downloaded **unchanged** PNG from the Windows artifact:

    python gallery/originals/04-the-silent-bell/verify_artwork.py --canonical PATH/TO/silent-bell-canonical.png

The default verifier checks the committed PNG. The optional path checks an external copy instead. Both validate exact encoded SHA-256, PNG CRC/filter/decompression, decoded normalized output identity and rejection of corrupted bytes without requiring Windows.

On supported Windows with the existing Debug/Release executables:

    .\scripts\configure.ps1
    .\scripts\build.ps1 -Configuration Debug
    .\scripts\build.ps1 -Configuration Release
    .\gallery\originals\04-the-silent-bell\render-win.ps1 -OutputDirectory .\artifacts\silent-bell-native -CrossCheckDebug

This fail-closed driver renders the actual CPU/WIC canonical frame zero, read-only verifies decoded output with both configurations, and writes an author CLI receipt. The receipt is not an FM-012 GUI export. It refuses to overwrite nonempty evidence folders.

To inspect the project in the desktop application, use Ctrl+Shift+O on the included the-silent-bell.fmproj, then explicitly relink to source.png if required. Reopening must preserve the normalized source identity.

**Remaining acceptance:** physical Win32 source relinking/project opening and GUI FM-012 export; owner aesthetic judgement. This branch is Draft. A passing CLI replay is not an FM-012 acceptance or permission to merge. Do not substitute a D3D proxy or silently rewrite the authored input.
