# Shearwing #53 — canonical admission audit (11 November 2026)

**Status: REVIEW / HOLD.** Independent, read-only evidence supplement for existing Draft [#53](https://github.com/techrote/FAULTMINE/pull/53). No change to original source, genome, pixels, author branch or accepted gallery. **No new quarry.** Integration/owner control admission.

## One-command local review

Use the exact [current-main four-family Windows review package](https://github.com/techrote/FAULTMINE/actions/runs/43031286196) distributed by Integration: FAULTMINE-current-main-four-family-owner-review-PR67-20261111.zip. The full-archive SHA-256 must be:

    5135f36d2cfca6baa91b27d7b84e0eefea689e95a38232bb112a73f1bb655b8d

From the repository root (Python 3.10+ standard library only):

    python gallery/evolution/admission-20261111-shearwing-dedupe/inspect_shearwing.py /path/to/FAULTMINE-current-main-four-family-owner-review-PR67-20261111.zip --html Shearwing-native-admission.html

Open the resulting HTML locally. It contains **five unchanged native CPU PNG byte streams** as data URIs: descendants **24, 81, 65, 35, 71**, at their original 384×256 resolution (CSS may scale presentation). No proxy, new render, external network, JavaScript or gallery subsystem. A failed pinned archive/manifest/decoded-image check prevents the HTML being generated. The existing merged [four-family comparison desk #62](https://github.com/techrote/FAULTMINE/pull/62) includes **Punched Glass #147**, **Ossuary #223**, **Seismic #101** and Shearwing #65; this is a supplementary intra-quarry comparison.

## Independent evidence, not inflated candidate counts

The native archive reports and this script verifies:

| Measurement | Shearwing #53 | Seismic #57 (separate authored evidence) |
| --- | ---: | ---: |
| Completed/requested | **96/96** | 128/128 |
| Failed render addresses | **0** | 0 |
| Unique canonical decoded outputs | **96** | 125 |
| Exact duplicate outputs | **0** | 3 |
| Descriptor near representatives | **70** | 85 |
| Near-redundant unique outputs | **26** | 40 |

Shearwing threshold: **inclusive descriptor L1 8,192**, descriptor v1, canonical full-resolution frame 0, high-radius mutation seed f70ab531c9d204e6 and parent root seed 0123456789abcdef. **26/96 (27.1%)** unique-pixel outputs collapse into earlier descriptor-near representatives. Exactly **10** groups have multiple members; the largest has **10**. Two groups rooted at **2** and **4** contain **8** and **10** members respectively, accounting for **16/26** redundant-near outputs. Thus 96 hashes are not 96 artistically independent works.

A visually deceptive pair, **#24 and #81**, differs at only **1.551% of RGB pixel positions** and is the same near group despite two distinct pixel hashes. Retain at most one. Selected **#65** is its own near representative; it differs from #24 at **94.611%** of RGB positions and from #71 at **91.314%**; pixel difference alone is not an aesthetic score. The script independently decodes/checks all 96 native RGBA PNGs using the engine's domain-separated FAULTMINE-SOURCE-RGBA8-v1 hash. It validates ordering, source, parent, seed, manifest identity and FM-014 near-group assignments, rejecting corrupt/stale archives.

**Selected author work:** *Shearwing Telemetry — The Doubled Passage*, descendant **65**, canonical pixel identity 6f149f34cb9eca441c4206de1b5e7b0f035cab3b2ad958e3401f5a1cef2d9c50; original native PNG SHA-256 41a2e7863eecd7ef51d460dd7d7909b220b915ee60941cd2077d5df0bb2a4cf5. Immutable parent genome Git blob 64b0756262db458e276861dad84edbd964571558; normalized source identity 31c65bdb62b16ce1c890ac5f4beeca7910dc26e5ad2509b6ca186d1faa34ca1b. Original Windows native proof: [run #42969442678](https://github.com/techrote/FAULTMINE/actions/runs/42969442678).

## Curatorial decision and next gate

Native full-resolution images show **#65** has cleaner separated cream/ochre flight ribbons and a substantial dark airspace. **#24/#81** nearly repeat an overfilled twin-wing layout; **#35** is denser and **#71** disperses its strokes. Reject #81 as a near duplicate. Keep **#65** as this family's one evolutionary contender; new mining is unwarranted. Accepted gallery comparison and the existing Seismic #101 alternative remain inspectable through merged #62. Seismic's dense four-channel raster is less legible: do not admit both solely because their hashes differ.

**Integration/owner action:** review the five-native-image HTML alongside the existing four-family admission desk, then decide **ADMIT / REVISE / REJECT #53 winner 65**. If admitted, independently qualify physical Win32 project loading, exact-source relinking, export and cancellation. No owner aesthetic or physical Win32 GUI acceptance is claimed. Keep #53/#57 Draft until supported admission.

## Qualification and limitations

On the exact archive: 96/96 decoded identities match the native manifest; independent Pillow decode matches all 96 RGBA buffers; bit-flip CRC and truncation negatives reject corrupted images; the five embedded HTML images preserve original encoded PNG bytes. Script writes only an optional review HTML outside the repository. This is **archived Windows-native evidence**, not a fresh native render, current-PR CI result or physical Win32 acceptance. Earlier authored pixels and the merged #62 admission desk remain untouched.
