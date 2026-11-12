# Estuary Moraine — faulted shorelines / evolutionary quarry 02

**Status: complete Windows native mine, selected #128; PR Draft, owner/GUI acceptance and committed canonical PNG pending.** This independent search session deliberately separates an original hand-authored source landscape from a published immutable representation/bit genome. Neither prior gallery works nor engine/UI code are touched.

## Visual problem

Can a fault stack **fracture a coherent sedimentary line system without turning it into undifferentiated noise**? The authored 384×240 opaque BMP uses two shifted families of contour marks, a deep central erased fissure, orange seam cautery, contrasting pale mineral and green-tide deposits, limited stippling and large dark reservoirs. Its geometry and colour vocabulary deliberately reject the circular aperture/column layout of the first quarry and the moth/communications imagery of sibling artists. This is a design description of **the source**, not a claim about unrendered canonical descendants.

The art source is owned and generated locally by `make-source.mjs`. Its output is committed as `source.bmp`; `node make-source.mjs --verify` proves byte identity without overwriting it. No copyrighted images, photographs, fonts, external network data or nondeterministic inputs.

## Immutable parent and independent address

- Parent: accepted main `examples/fm006-representation-bit-genome.json`, committed verbatim as `parent-genome.json`, blob `778727acebba7aa97a8686bec13d38bc222bb430`.
- Source BMP Git blob: `6696a6f2ff112fef59bb79190da983c319302088`; normalized content identity must be measured by native WIC and checked against independent Python decoding.
- Mutation seed `e2a7f04c6bd59183`, descendant indices `[0,256)`, radius `high`, frame `0`, `canonical` (NOT proxy), descriptor v1, inclusive near threshold `8192`, requested selection `12`.
- Thread count defaults to 4, but is non-semantic. The accepted FM-014 batch writes a complete manifest, cache PNGs, selected canonical PNGs/sidecars, GUI project, and a presentation-only contact sheet. Incomplete runs cannot be called a finished atlas.
- Preserve the original engine/schema/seed/operator identities of the parent. The search mutates only derived child genomes.

## Reproduce on supported Windows

From the repository root with Windows MSVC release built:

```powershell
node .\gallery\evolution\02-estuary-moraine\make-source.mjs --verify
.\gallery\evolution\02-estuary-moraine\mine.ps1 -Threads 4 -OutDir "$env:TEMP\FAULTMINE-Estuary-Moraine-02"
python .\gallery\evolution\02-estuary-moraine\verify_native.py "$env:TEMP\FAULTMINE-Estuary-Moraine-02" --all
.\gallery\evolution\02-estuary-moraine\mine.ps1 -Threads 1 -OutDir "$env:TEMP\FAULTMINE-Estuary-Moraine-02" -Resume
```

Use `-BinaryPath C:\path\FAULTMINE-batch.exe` for the portable executable (Git is required for the read-only input blob preflight). The second run revalidates caches; the manifest must remain byte-identical across thread counts. Source PNG generation, Python artwork synthesis, D3D proxy stills and invented scores are not substituted for this native run.

## Admission and image selection

Inspect actual selected native images/contact sheet before choosing a final. Prefer meaningful contour retention, strategically shifted interruption, legible fault edges and genuinely different silhouettes, not trivial palette rotation or mere numerical novelty. Reject exact/near duplicates using native records. Record selected descendant, rendered pixel and genome identities, exact FM-012 provenance, image comparisons, and owner review state. This pending art package is intentionally Draft until actual Windows outputs and visual observation are available. The CI job, if available, is only a reproducibility and binary qualification path, **not** owner aesthetic approval.

## Native outcome — 11 November 2026

Real [Windows native run #42948340940](https://github.com/techrote/FAULTMINE/actions/runs/42948340940) passed, including Debug/Release tests and exact semantic resume. 256/256 canonical candidates completed; 256 unique genomes, 230 unique decoded outputs, 139 near representatives and 12 novelty selections. Editorially selected #128 after inspecting the real native contact sheet. See [curated/SELECTION.md](curated/SELECTION.md) and native image artifact [#13823499050](https://github.com/techrote/FAULTMINE/actions/runs/42948340940/artifacts/13823499050).

Full-resolution PNG is available from that artifact, not yet a committed gallery PNG. No native-render image is synthesized in Python. Exact selected genome and unmodified FM-012 provenance sidecar are committed in `curated/`. This run's original pending-status section above documents the initial gate only.
