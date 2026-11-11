# Shearwing #53 — canonical admission audit (11 November 2026)

**Status: REVIEW / HOLD.** Independent, read-only evidence supplement for existing Draft [#53](https://github.com/techrote/FAULTMINE/pull/53). No change to original source, genome, pixels, author branch or accepted gallery. **No new quarry.** Integration/owner control admission.

## One-command local review

Use the exact [current-main four-family Windows review package](https://github.com/techrote/FAULTMINE/actions/runs/43031286196) distributed by Integration: FAULTMINE-current-main-four-family-owner-review-PR67-20261111.zip. One of the two **exactly pinned** package SHA-256 digests must match:

    5135f36d2cfca6baa91b27d7b84e0eefea689e95a38232bb112a73f1bb655b8d  original Integration ZIP (286 entries)
    323c81905c675f55ed12e1274ed6faa6a5212cacc90b0a2e6cd09841ec82f1eb  owner-acceptance ZIP (289 entries)

The second package contains the original archive members unchanged and three independent noncanonical review/audit records. The verifier additionally pins the native `batch.fmproj`, FM-012 selected contact PNG and its manifest, original source, full FM-014 manifest, all 96 decoded candidate PNGs, and winner #65. Arbitrary ZIPs, altered member bytes and unapproved rebuilt packages are rejected.

From the repository root (Python 3.10+ standard library only):

    python gallery/evolution/admission-20261111-shearwing-dedupe/inspect_shearwing.py /path/to/EXACT-QUALIFIED-FOUR-FAMILY.zip --html Shearwing-native-admission.html

Open the resulting HTML locally. It contains **ten unchanged native CPU PNG byte streams** as data URIs: all **eight selected novelty representatives (0, 2, 8, 35, 46, 65, 71, 74)** plus unselected **24 and its near duplicate 81**. All remain at original 384×256 resolution (CSS may scale presentation). No proxy, new render, external network, JavaScript or gallery subsystem. A failed pinned archive/manifest/decoded-image check prevents the HTML being generated; don't treat a previously created HTML as evidence of a failed new invocation. The merged [four-family comparison desk #62](https://github.com/techrote/FAULTMINE/pull/62) includes **Punched Glass Index (#45 / #147)**, **Dual Relay (#39 / #223)**, **Seismic (#57 / #101)** and Shearwing (#53 / #65). **All four are unadmitted Draft artwork.** Accepted main contains this review infrastructure, not an accepted-art gallery; comparison against admitted finished artworks is not yet possible.

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

## Wider eight-selection triage (actual native pixels)

The authentic eight-image novelty sample was visually compared in one 10-panel native montage, not inferred from diversity scores. Candidate **#8** is almost entirely rainbow-band interference and **#46** is a red-saturated field: **reject both** as stand-alone Shearwing artwork because the wing/airspace relationship disappears. **#0** is sparse row-wise telemetry, **#2** a fractured pale-wing diagonal, and **#74** leaves only a thin echo; these are credible variants but lose the organized two-flight structure. **#35** and **#71** preserve more flight structure at the cost of crowded or heavily fragmented silhouettes. This review does not designate those near-duplicate groups as distinct works; do not interpret uniqueness of encoded pixels as a gallery admission. **#65** remains the only proposed specimen to advance, with subjective approval still reserved to the owner.

## Curatorial decision and next gate

Native full-resolution images show **#65** has cleaner separated cream/ochre flight ribbons and a substantial dark airspace. **#24/#81** nearly repeat an overfilled twin-wing layout; **#35** is denser and **#71** disperses its strokes. Reject #81 as a near duplicate. Keep **#65** as this family's one evolutionary contender; new mining is unwarranted. I also visually inspected #62's four genuine embedded native images: Punched Glass has a diagonal neon relay on an open olive field; Dual Relay places sparse horizontal cyan/amber bands on charcoal; Seismic #101 fragments its four-channel signal into much denser texture. These competing **unadmitted Drafts** are compositionally different from Shearwing's two flight passages. Seismic is the less legible nearest thematic competitor; do not admit both merely because hashes differ. There is no accepted-art baseline to claim comparison against.

**Integration/owner action:** review the ten-native-image HTML alongside the existing four-family admission desk, then decide **ADMIT / REVISE / REJECT #53 winner 65**. If admitted, independently qualify physical Win32 project loading, exact-source relinking, export and cancellation. No owner aesthetic or physical Win32 GUI acceptance is claimed. Keep #53/#57 Draft until supported admission.

## Qualification and limitations

On the exact archive: 96/96 decoded identities match the native manifest; independent Pillow decode matches all 96 RGBA buffers; bit-flip CRC and truncation negatives reject corrupted images; the ten embedded HTML images preserve original encoded PNG bytes. Script writes only an optional review HTML outside the repository. This is **archived Windows-native evidence**, not a fresh native render, current-PR CI result or physical Win32 acceptance. Earlier authored pixels and the merged #62 admission desk remain untouched.
