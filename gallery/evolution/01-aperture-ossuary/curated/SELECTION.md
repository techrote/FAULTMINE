# Aperture Ossuary — Dual Relay / 223

**Selected finished work** from an actual Windows-native FAULTMINE canonical quarry, 11 November 2026. This document is the later evidence-backed curation supplement to the original mine-ready session README. It does not change the authored source, parent genome or any historical specimen.

## Composition and selection

The authored aperture source was intended to test *architectural dislocation*: can a circular void and surrounding colonnades survive address faults as a readable composition, rather than dissolve into arbitrary colour noise? The actual 12-work native selected contact sheet was inspected, as were full-resolution candidate #223 and the contrasting picks.

**#223 — Dual Relay** is the editorial selection. Two amber-lit horizontal reserves face one another across dark, divided architectural space; short cyan crosshatching and fine base registers suggest a ruined optical relay. The composition uses large quiet areas against insistent small-scale repetition. Its voids are more deliberately articulated than candidate #000's busy rearranged arcs, and it is less monochromatically saturated than #028. #178 has an attractive repetitive ruled field, but distributes marks almost uniformly and sacrifices the central hollow. Near-blank #031, #146 and #172 fail the requirement for legible compositional activity. Numeric FM-014 novelty alone did *not* make this aesthetic choice.

The complete **native 12-selected-specimen contact sheet** and every canonical still remain in the [Windows gallery evidence run](https://github.com/techrote/FAULTMINE/actions/runs/42940066799) downloadable artifact `FAULTMINE-gallery-native-evidence`. The contact sheet is a comparative *presentation* output; it is not itself a canonical specimen. Native 384×240 pixels and the native FM-012 sidecar were independently checked before selection.

## Exact semantic address and provenance

| Property | Verified value |
| --- | --- |
| Parent | Accepted `examples/fm005-addressing-genome.json`, source snapshot `c5f8ce3` |
| Parent canonical identity | `01217a9e77ea4cd446b7f6142ab6ea71db9bedbcca48425a02d60ea15849524d` |
| Authored source | Original `source.bmp`, 384 × 240, normalized identity `0023ceb14294cf69b7db062a1afc255ebaa8cc6e2a0119f4bb81d91be053125a` |
| Parent root seed | `0123456789abcdef` |
| Independent mutation seed | `4d13b09ec72a8851` |
| Search | High radius, indices [0,384), frame 0, full-res CPU canonical, descriptor v1, near L1 ≤ 8192, select 12 |
| Editorial winner | Descendant `223`, candidate identity `c051a7bf648678d1f70d6d9198ec643a4597366ac8314f9e789b8fefd472fac6` |
| Winning genome SHA-256 | `3128292f906dfc15b661d2cc749c628a541a3301138ef07c150722900f71190b` |
| Canonical decoded RGBA8 identity | `eb70ef28390a69e3e1087471d611b41c94239e4140313ca9b782abd073472c14` |
| FM-014 winning novelty distance | `85804` (descriptor L1, not aesthetic rating) |

The winner has four enabled typed operators, in order: address offset, address mask, zero stride delta (an authored mutation result, not secretly omitted), and 18 bounded address bursts. The exact parameters and instance IDs are in `ossuary-transmission-223.genome.json`; never approximate these from visual appearance.

## Real completion and audit

Integration's pinned Windows MSVC/WIC run [42940066799](https://github.com/techrote/FAULTMINE/actions/runs/42940066799) executed the actual `FAULTMINE-batch.exe`, not a Python mock or D3D proxy. The native summary is: **384 requested and completed; 0 failed; 384 unique genomes; 371 unique pixel outputs; 33 near-duplicate groups; 12 novelty-selected**. The chosen #223 is its own exact-pixel and inclusive near-duplicate representative. Full native Debug/Release tests and native gallery workflow passed in the same run.

The author-supplied `qualify-results.py --all` was executed independently against the downloaded exact evidence: **396 complete PNG decodings and canonical pixel-identity comparisons** (384 cached candidates + 12 selected stills), including strict selected FM-012 sidecar checks. The committed compact PNG is a *lossless re-encoding* of the selected CPU canonical image: its PNG transport is indexed-colour for repository size, but its complete decoded straight RGBA8 buffer is identical to the native selected RGBA8 buffer. The PNG is **not** a proxy or a newly invented render.

- Native FM-012 still PNG encoded SHA-256: `e779818cbb1a3f631d8f4d2479816af9f6496ad46436eb4600479d3cf8cdd70c`.
- Committed lossless indexed PNG encoded SHA-256: `d492bd7a49c74f6ab495a7075889d96fc91336f11fbe7a1bf0aab42f423af81a`.
- Both decode to exactly canonical image identity `eb70ef28390a69e3e1087471d611b41c94239e4140313ca9b782abd073472c14` at 384×240.

The checked-in `ossuary-transmission-223.fmmanifest.json` is the **unaltered native FM-012 still sidecar**, including the original Windows CI source path as historical path metadata. That path is not a required runtime location; relink to the local `source.bmp` only after matching source identity.

## Reproduction and outstanding gates

From the repository root, run `python gallery/evolution/01-aperture-ossuary/curated/verify_selection.py` for stdlib-only strict checking of the checked-in image, recipe, canonical decoded pixels, FM-012 lineage and encoded identities. With the downloaded Windows artifact's evolution directory, add `--native-dir <directory>` for a native evidence comparison; run `python gallery/evolution/01-aperture-ossuary/qualify-results.py <directory> --all` to audit the entire 384-index quarry.

For native reproduction on accepted Windows Release, use `FAULTMINE-render.exe --verify <source.bmp> <curated/ossuary-transmission-223.genome.json> <curated/ossuary-transmission-223.png>`. This exact **committed PNG native read-only verification has not yet been run**, despite the native original's independently audited decoded identity. A second `FAULTMINE-batch.exe --resume` pass using a different worker count, GUI loading of the selected `batch.fmproj`, and owner aesthetic acceptance remain pending. Retain this PR as **Draft**, without declaring a release or altering simulation semantics.
