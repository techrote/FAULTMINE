# Seismic Debt — The Earth Refused the Baseline

**ART-EVOL-006. Independent evolutionary search candidate; native final still awaits visual curation.**

Four seismogram voices record one rupture differently. The first almost refuses to move; the second erupts violently; the third records an aftershock; the fourth registers a delayed tremor. A discontinuous diagonal red fault physically removes the same stretch of time from every signal. The title and sparse metadata are integral authored pixels, not UI overlay. The specimen search deliberately misaddresses the recordings to reveal false repetitions, displaced aftershocks and new interruptions. This is not an attempt to win a similarity score or make random colourful noise.

Unlike the existing evolutionary studies of aperture, shoreline, punched glass, hanging orchard and flight telemetry, this composition is about the *time structure of an event as recorded spatially*. It begins with four strongly separated rhythmic morphologies, not an image whose parts are mainly decoratively recoloured.

## Immutable accepted inputs

- Self-authored 384×240 24-bit opaque BMP materialized locally at `source.bmp` (not a tracked binary); complete reproducible construction in `make_source.py` (Python 3 standard library only). No external reference photos, fonts, assets, pretrained output or protected marks.
- Encoded BMP SHA-256 `c751316e7615d53cf8a43e79e3a8395ec63f49392e7d16e37be16ad5f100f875`.
- Normalized WIC-compatible RGBA8 source ID `298051e0e8c048f36f4ac830baedf989e3daf4907fa360d75f59c8e3450dec88`.
- Immutable accepted parent `examples/fm005-addressing-genome.json`, Git blob `64b0756262db458e276861dad84edbd964571558` (root seed `0123456789abcdef`). Parent remains completely unchanged; topology is explored by the accepted FM-009 indexed mutation engine.
- Mutation policy v1, mutation seed `6ac9f2e430bd7185`, radius `high`, descendant indices `[0,128)`, canonical full-resolution render at explicit frame `0`, four worker threads, inclusive descriptor-v1 L1 threshold `8192`, 12 deterministic diversity selections, every selected full-resolution FM-012 still and sidecar. Proxy rendering is **not** requested.

## Exact Windows session

From an MSVC x64 Windows checkout with native tools built in **Debug** and **Release**:

```powershell
python .\gallery\evolution\06-seismic-debt\make_source.py --write
python .\gallery\evolution\06-seismic-debt\make_source.py --verify
.\gallery\evolution\06-seismic-debt\mine.ps1 -OutputDirectory .\artifacts\seismic-debt -Threads 4
```

When resuming exactly the same address, explicitly add `-Resume`. The driver requires the entire accepted parent fingerprint and authored source ID, rejects existing output directories unless opted into verified resume, audits every candidate's strict ascending index and diversity counts, compares the full semantic manifest through one-thread verified resume, and independently replays every selected native canonical PNG in both Debug and Release without rewriting it. It checks per-selection FM-012 sidecars and emits an `audit.json` recording observed (not presumed) identities.

Expected original native output layout: `batch.fmbatch.json`, `batch.fmproj` (GUI loadable, schema v2), `selected-contact-sheet.png` and presentation sidecar, `selected/candidate_*.png` and individual FM-012 manifests, cached `candidates/` plus audit. The contact sheet is **presentation only**; candidate PNGs are authoritative decoded full-res CPU pixels. A selected child's genome is reconstructible from its indexed canonical genome inside the batch manifest.

## Artistic gate

The highest novelty distance does **not** establish good art. Selection must follow inspection of genuine Windows canonical full-size candidates and the authentic 12-up presentation. Reject near duplicates, mere swaps of colour, and specimens that erase all four temporal voices. Curate at least one whose fault visibly stages a conflict between the earthquake and the memory of its signal. The selected final still/genome/FM-012 receipt and independent repeatable replay must be archived before admission. If the Windows runner is absent or fails, retain the executable session and record the real blocker; never draw a proxy and call it canonical.
