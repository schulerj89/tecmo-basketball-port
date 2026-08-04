# R2B independent Luna QA

## Identity and allocation

Exactly one independent top-level projectless reviewer was created after the
post-merge collision and registry gate:

| Field | Value |
| --- | --- |
| Task ID | `019fca5b-3b84-7a82-b2ab-588c50a4b7fd` |
| Title | `Tecmo R2B Shots Outcomes Integration Independent QA — Luna Max` |
| Model | `gpt-5.6-luna` |
| Thinking | `max` |
| Project association | none; projectless |
| Authority | read-only independent QA, reporting only to authoritative R2B Sol |
| State during QA | pinned |

The task was instructed not to edit, build, test, create proof, create or
delegate tasks, contact master/other tasks, mutate Git, or alter its pin. The
same task is reused for evidence and signed-document revisions.

A first creation request included an invalid extra schema field and was
rejected before a task existed. Registry and app checks then confirmed zero
matching tasks, and the corrected request created the single task above. This
does not represent a replacement reviewer or a second allocation.

## Independent source, merge, and classification audit

The reviewer independently read applicable repository instructions and audited:

- clean worktree, exact branch, base, main ancestry, and refs;
- Good signatures, required merge parent order, merge tree, and candidate
  three-commit lineage;
- exact 17-path ownership, 80-path current-main side, normalized overlap zero,
  diff checks, and proprietary/binary scan;
- both exclusion blobs;
- transactional/fail-closed TGCS/TGJS/TGDK/TGSR loaders;
- shot selection, evaluation, timelines, route/rattle state, claimant
  settlement, scoring, period expiry, audio, lineup, fatigue, and rendering
  seams;
- preservation of exact/source-pinned, native-approximate, and incomplete
  classifications;
- continued deferral of the public numeric-1 `"invalid"` name and stale
  source-map wording.

Initial result:

```text
P0=0, P1=0, P2=0. No actionable findings.
```

The reviewer reported exact merge tree and ordered parents, exact candidate
lineage and ancestry, exact path ownership/overlap, clean diff checks, no
binary/proprietary payload, exact exclusion blobs, and no defect in the reviewed
source or cross-seam state transitions.

At that stage it correctly did not independently accept Sol proof artifacts,
because Sol had not yet supplied the fresh paths and hashes.

## Same-task evidence review

Sol followed up on the same pinned task with exact ignored evidence paths,
manifest hashes, gate results, visual-inspection scope, and harness diagnostics.
The reviewer remained read-only and inspected:

- `build/r2b-sol-native-shots-20260804T013200Z-v2/PROOF-MANIFEST.json`,
  SHA-256
  `96C260D23B27559C9B0907264F80EDCEE56E75A2526BF87ABC356FE82CF884CB`;
- `build/r2b-sol-scene-20260804T012159Z/PROOF-MANIFEST.json`, SHA-256
  `111788F30FC2E834C158E6EDCE38A4CCE3F395194A9FD548E03297921BE6B0EA`;
- `build/asset_pack_test_report.json`;
- `build/intro_sequence_test_report.json`;
- `build/r2b-sol-native-flow-report.json`;
- selected full-resolution PNGs and the scenario/contact-sheet evidence.

Evidence-review result:

```text
P0=0, P1=0, P2=0. No implementation or integration defect found.
```

The independent conclusions were:

- exact shot-proof inventory of 110 PNGs, eight MP4s, and one manifest;
- all declared paths contained and all PNG/contact hashes/dimensions valid;
- all 102 corresponding frame pairs, all four sheet pairs, and all four fresh
  video pairs byte-equal;
- all four accepted frame aggregates exact;
- historical MP4 hash differences consistent with FFmpeg 8.1
  container/toolchain variance, not a rendered-frame defect;
- all 254 declared scene-manifest artifacts hash-valid;
- both scene repeats equal at sheets, videos, decoded frame hashes, and 14/14
  stored frame records;
- coherent make, miss, rattle, live-scene, and dunk visuals;
- the all-black dunk `frame-0005.png` is the expected source-frame-64
  cutaway, not an unexplained blank.

## Required caveats from independent review

The reviewer required the terminal documents to preserve two evidence limits:

1. The scene manifest is a generic integration-smoke `DRAFT` with task
   `R1-LIVE-FOUNDATION`, `require_pass=false`, `build_requested=false`,
   `build_warning_clean=false`, and `final_sha=PENDING_CLEAN_COMMIT`. It may
   support scene-seam evidence only when paired with Sol's separate fresh `/W4`
   build and complete gates; it is not a standalone R2B terminal manifest.
2. The intro report has one unrelated bounded pixel-mask suite skipped because
   its reference PNGs were unavailable. That skip must not be described as
   executed coverage.

These caveats are carried in `README.md`, `COMMANDS.md`, and `EVIDENCE.md`.

## Residual accepted incompletes

Luna separately listed the accepted non-findings that remain incomplete:

- numeric-1 full semantics and trajectory;
- complete `$91BC` and `$AD6E` helper inputs;
- neutral substitutions for unavailable native state;
- approximate claimant ordering;
- deferred CPU autonomous jump/far behavior;
- public numeric-1 name remaining `"invalid"`;
- stale source-map wording remaining deferred.

None was upgraded or converted into a semantic claim.

## Signed-document review state

The first Good-signed documentation candidate is sent back to this same task
for a final read-only review of scope, lineage, gate accuracy, caveat wording,
and severity. The reviewer remains pinned until a durable Good-signed terminal
acceptance is recorded. The exact terminal review and severity ledger are added
here in a docs-only signed revision before guarded handoff.
