# Existing Seismic #57: exact-archive selection review

Status: **review supplement / HOLD, not artwork admission**. This is a bounded reuse of the authentic native 128-member quarry, **not** a new mining session, source, genome, render, or mutation. All existing artwork and author branches remain unchanged.

## One owner-facing decision

**Retain Seismic candidate #101 as the sole Seismic contender, but defer admission of #57 behind Shearwing #53 winner #65.** In the genuine 12-up native sheet, #101 has richer interleaved cyan/chalk/orange breaks than the more literal #102, but it obscures the original four seismic channels. #102 is the legibility alternative, not a second proposed admission. #14 is overwhelmed by a red plane, #33 by checker noise, and #84 by near-total void. Comparison with Shearwing's clearly separated wing passages favors #65 as the first piece for owner review. There is **no already-accepted gallery art on current main** to compare against: Shearwing and Seismic are both still Drafts.

The displayed 12 selected Seismic stills and #65 Shearwing still are original archived Windows CPU **canonical** PNG bytes. The reviewer HTML is a noncanonical, static, offline presentation; it embeds bytes unchanged rather than regenerating pixels.

## Existing evidence, not fresh Windows rendering

| Evidence | Exact record |
| --- | --- |
| Current main used | `798bc3b827126b4d94564a34bd19fd880e4b2069` (11 Nov 2026) |
| #57 author Draft head observed | `bb2bc1e81bb04df376fa19e06260c3b6da4c6007` (author branch, not this archive's run head) |
| #53 author Draft head observed | `149d38de35533fe41d84736c9ec2f4bbde805345` |
| Native Seismic package | [Windows native archive, run 42973963152, artifact 13834946825](https://github.com/techrote/FAULTMINE/actions/runs/42973963152/artifacts/13834946825) |
| Seismic archive SHA-256 | `16cd21878c8872e134c3c8783f9ea56924800ea37755fa2e7e72c8a2de56f977` |
| Seismic archive's actual CI run/source head | `0d91cea9a74a5db786b27a0bdc1a2fb1cc6ed0c4` — **not** final #57 PR head |
| Immutable semantic manifest SHA-256 | `85e2faa021132bb803192c2471d2b453f064c71522cfc5bc6beef8551f18cf6d` |
| Seismic source / mutation | authored 384×240 BMP; normalized source ID `298051e0e8c048f36f4ac830baedf989e3daf4907fa360d75f59c8e3450dec88`; accepted parent `examples/fm005-addressing-genome.json`; seed `6ac9f2e430bd7185`; high radius; canonical frame 0 |
| #101 winner | PNG SHA-256 `19629f3ccf873f8eb39cac7f8729c58ddb6a71e27b07039720f0766f32a9b337`; normalized decoded RGBA8 ID `492f844f1dc21cd3abf4516aa7c383fc276fbfda72205ae628b03a6b52a443e2` |
| Shearwing #65 companion | archived Integration four-family Windows review ZIP; SHA-256 for its winner PNG `41a2e7863eecd7ef51d460dd7d7909b220b915ee60941cd2077d5df0bb2a4cf5` |

## Run the review on existing downloaded evidence

From repository root, with the independent [four-family Windows review archive](https://github.com/techrote/FAULTMINE/pull/67) that includes `CANONICAL-REVIEW-EVIDENCE/shearwing-05/`:

```powershell
python gallery/evolution/admission-20261111-seismic-existing-selection/verify_and_review.py "SEISMIC-NATIVE-ZIP" "FOUR-FAMILY-REVIEW-ZIP" --negative-test --html "OPEN-SEISMIC-SELECTION.html"
```

Then open `OPEN-SEISMIC-SELECTION.html` offline in Firefox or Chrome. The generated reviewer has 12 labelled, exact native Seismic selected stills and a #101-versus-#65 decision comparison. Refuses overwriting existing output. Requires Python 3 standard library only.

Actual local source-exact test on 11 Nov 2026: **128/128** archived canonical PNG decoded RGBA8 identities, **0 failures**, **128** genomes, **125** pixel-unique outputs, **3** exact pixel repeats (2.34% of 128), **85** descriptor-near representatives, and **40** extra near-redundant unique images (32.0% of the 125 unique). The selected set is **[0, 1, 3, 12, 14, 24, 33, 84, 97, 101, 102, 125]**. Exact and near repeats are intentionally separate metrics. The pinned archive/head, complete manifest, every cached candidate identity, selected PNG transport, winner PNG SHA, and independent Shearwing #65 decoded identity all passed. Altered winner PNG CRC was rejected in the negative test. No new quarry was launched.

Generated offline HTML SHA-256: `da9a05f202cca5a428e803b7d65a7e1fa1f123b2063fcf77c87edbd8c76b2180`. Local **Chromium in-memory** 1440px/390px checks: 14/14 images decoded, no overflow, no JavaScript errors or outgoing requests. This environment blocked direct `file://`, so no direct local-file or physical Firefox assertion is made.

**Integration/owner gate:** first decide ADMIT / REVISE / REJECT Shearwing #65; then evaluate Seismic #101 against the alternate #102 and decide separately. Before admitting either, verify the exact latest artwork PR head, native project source relink, complete FM-012 export/cancellation and physical Win32 GUI. Static archive identity and Chromium observation do not satisfy those human gates.

No new named artwork, accepted record, engine code, GUI subsystem or original/temporal asset is included in this Draft.