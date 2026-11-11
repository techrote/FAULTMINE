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

## 11 November 2026 — Independent Shearwing #53 archive qualification

No new mining. The separate `audit_shearwing_native.py` independently decodes **all 96** archived full-resolution Windows CPU canonical candidates from the already distributed four-family review ZIP. It verifies original source BMP **transport SHA and normalized straight-RGBA8 content identity** using a Python-stdlib decoder; the native project v2's source binding; every PNG's actual decoded ID; all 24 descriptor-v1 values; the exact and inclusive-L1 near representative assignments; all eight original selected PNG/genome pairs; and the deterministic farthest-point selection and novelty distances. The original source, genome, selected images, project, manifests and author branches are unchanged.

**Observed source-exact correction:** Shearwing has **96/96 completed**, **zero failures**, **96 unique decoded images**, **zero exact pixel duplicates**, **70 near representatives** and **26 near-redundant unique images** (27.08% at inclusive L1 threshold 8192). The original #53 PR summary's **10 near representatives is inconsistent** with the manifest and independent descriptor replay: do not propagate that count. This is an objective provenance/count correction, not artistic qualification.

Original eight selections by descendant index: `[0,2,8,35,46,65,71,74]`. Independently reconstructed farthest-point novelty **ranking order**: `[0,74,8,2,71,35,46,65]`. The rank order and sorted selected-index order must not be confused. Winner #65 remains the preferred physical selection; no bytes change.

Run, with the existing four-family current-main Windows review ZIP:

```powershell
python gallery/evolution/admission-20261111-seismic-existing-selection/audit_shearwing_native.py "FAULTMINE-current-main-four-family-review.zip" --negative-test
```

**Portable winner entrypoint qualified at the file/identity level:** the historical `CANONICAL-REVIEW-EVIDENCE/shearwing-05/batch.fmproj` still records an absolute Windows CI path. **Do not use that archival batch project as the owner launcher.** The already-packaged `REVIEW-PROJECTS/04-shearwing-WINNER-65.fmproj` instead has the relative source path `gallery/evolution/05-shearwing-telemetry/source.bmp`. Independent read-only checks establish that the referenced BMP is present in the extracted bundle, its normalized identity matches, the active project genome equals native candidate #65, the active lineage identity matches the selected genome, and the FM-012 native still receipt binds the same source/genome/pixels at frame 0. Portable review-project SHA-256: `aad78dfbd102b379b19f56a5edd514ee4d9d73dd6f7a60bc46161671be7e1655`. Run the existing `START-REVIEW.cmd`, then import `REVIEW-PROJECTS/04-shearwing-WINNER-65.fmproj` from the package root. **This is source-exact package qualification, not observed physical Win32 project-open, export/cancel, or owner artistic acceptance.**

Curatorial order unchanged: **REVIEW #53 Shearwing #65**, then **HOLD #57 Seismic #101**, using real native contact sheets. There is no already-admitted art on main. The independent script is a read-only gate, not acceptance authority. It now rejects a stale, missing or mismatched packaged winner project and mismatched native export receipt, as well as a changed source, image or descriptor.

**Cross-family native-pixel screening, not an aesthetic score:** the two byte-pinned full-resolution winners have independent FM-014 descriptor-v1 L1 distance **123170**, versus the inclusive within-quarry near threshold **8192**. The review verifier now rejects an unexpected descriptor-near cross-family match and reports the actual distance from decoded pixels. Shearwing #65 has 73.35% pixels at integer luminance Y<24 versus Seismic #101's 78.14%; the readable rising wing forms—not this numeric difference—justify prioritising Shearwing.

**Windows launch-directory contract:** main's `src/platform/win32/main_window.cpp::resolve_project_source` loads the recorded project source path relative to the **process current working directory**, not relative to the selected `.fmproj`. The packaged `Run FAULTMINE.cmd` first executes `cd /d "%~dp0"`; launch via `START-REVIEW.cmd` from the extracted bundle root before opening `REVIEW-PROJECTS/04-shearwing-WINNER-65.fmproj`. Direct launches with a different working directory may correctly prompt for source relinking; the file-identity check alone does not demonstrate zero-prompt Win32 import.

Accepted main has already merged provenance-path repair [#72](https://github.com/techrote/FAULTMINE/pull/72) at `b267c6aa6d473404cec061dc8536a807c05204c3`. This review branch does not duplicate that fix.


## Follow-on gate — portable Shearwing lineage is not only an active-genome checksum

The portable `REVIEW-PROJECTS/04-shearwing-WINNER-65.fmproj` can carry the correct active genome and canonical frame while still containing an incorrect retained lineage. That would mislead the owner's provenance inspection even though the rendered still remains correct. The independent archive audit now checks **all eight selected nodes**, exactly two historical roots, contiguous creation ordinals, source identity, selected genomes and favourites, indexed mutation addresses (`f70ab531c9d204e6`, high radius, policy 1, immutable parent), and active #65. `--negative-test` also exercises **eleven isolated corrupted-lineage cases** in memory in addition to the existing tampered-PNG rejection; no original archive or project bytes are rewritten.

Local current-source fixture: the unchanged four-family native review archive's lineage passed; corrupt seed/parent/index/source/genome/favourite, missing/reordered/extra nodes, wrong parent genome and root derivation were each rejected (11/11). Full exact-branch archive audit and Windows CI are **separate required gates**; this source-only fixture is not a physical Win32 project-open or export test. This correction does not change artistic prioritisation: Shearwing #65 to owner, Seismic #101 remains on HOLD.
