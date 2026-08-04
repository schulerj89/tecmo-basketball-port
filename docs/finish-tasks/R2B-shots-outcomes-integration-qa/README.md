# R2B shots and outcomes integration QA

This folder records authoritative Sol integration QA for
`R2B-SHOTS-OUTCOMES-INTEGRATION-QA`, session
`S-SOL-R2B-SHOTS-INTEGRATION-QA-001`, claim
`OWN-R2B-SHOTS-OUTCOMES-INTEGRATION-QA`, and lane
`LANE-R2B-SHOTS-OUTCOMES-INTEGRATION-QA`.

The tested product is the conflict-free, Good SSH-signed branch-only merge
`26e6aaf19b639972cb9043f29fc55daa1efce835`. Its ordered parents are current
main `8a5b9928544a430efa34cbf98a248d6a8cbe7b14` first and immutable accepted
R2 candidate `7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a` second. Its tree is the
precomputed and independently reproduced tree
`2d918e8d672f991c87c293096e315a8bde5685da`.

## Decision

Sol's combined product, source/ASM, deterministic, visual, and cross-domain
review found no actionable defect: **P0=0, P1=0, P2=0**.

The sole independent projectless `gpt-5.6-luna` reviewer at thinking `max`,
task `019fca5b-3b84-7a82-b2ab-588c50a4b7fd`, independently returned
**P0=0, P1=0, P2=0** for both the merged source/lineage audit and the supplied
fresh evidence. The reviewer remains pinned while the signed documentation tip
undergoes its final same-task read-only review.

No product or candidate file was corrected in R2B. The only tracked mutation
before these documents was the one authorized signed no-ff reconciliation
merge. R2B documentation is confined to this folder.

## Acceptance anchors

- The branch began clean at exact base `8a5b992...`, with local `main`,
  `origin/main`, and live remote main all equal to that SHA.
- Immutable staging resolved exactly to candidate `7b9287a...`; the candidate
  is exactly three commits after base `222d75cfafa9153db1eb44492bf557f11b1a9091`.
- All candidate, merge, main, and applicable control commits verified with a
  Good SSH signature for `jaystar524@gmail.com`, RSA key
  `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.
- Candidate ownership is exactly 17 normalized paths, all accepted under
  `OWN-R2-SHOTS-OUTCOMES`. Current-main changed 80 normalized paths from the
  common candidate base; exact overlap is zero.
- `git merge-tree --write-tree 8a5b992... 7b9287a...` reproduced
  `2d918e8d...`; both candidate and predicted-tree `git diff --check` passed.
- The two accepted exclusions remained byte-identical at candidate base,
  every candidate commit, current main, predicted tree, and merged tree:
  `src/tecmo_gameplay_scene.c` blob
  `58ad821d31a5559225855fbb30a1566d374063e7` and
  `src/asset_pack/tecmo_asset_pack_source_map.c` blob
  `b6fc46a927f1a0cddedf7a965d3ebb4ad7d23b7f`.
- The latest acknowledged control checkpoint for the combined personal ledger
  is Good-signed `5a625bdf62b9d23db57d3808ff1242ebf0b574cf`.

## Product and classification disposition

The merge preserves the accepted distinction among exact/source-pinned,
native-faithful or native-approximate, and incomplete behavior.

| Area | Accepted classification | R2B disposition |
| --- | --- | --- |
| TGCS numeric 0/2 pose and phase data | exact/source-pinned | preserved and focused-gate clean |
| TGCS numeric 1 | source-backed fixed pose group; native pose-only approximation | preserved as a distinct numeric identity with unknown semantic meaning |
| TGJS profile/family/direction matrix | exact/source-pinned selection with documented native substitutions | persisted selected pose is consumed through flight; no stronger claim added |
| TGSR point classifier | exact/source-pinned pieces with native geometry binding | 1/2/3 classifier and immutable launch inputs pass |
| TGSR terminal polarity | exact for supported captured contexts | make/miss bit polarity and supported exact schedule pass |
| TGSR A708/A7A9/A8E9 routes | exact raw selector/address identity; native approximate tails | all four selectors stay distinct; no rebound/block/steal name inferred |
| TGSR four-pass rattle | exact/source-pinned bounded prefix | altitude, cadence, orientation, state, velocity restore, and bridge pass |
| Claimant scan and settlement | source-order native approximation plus proven team relation | same-team retention/opponent handoff pass; dynamic native order remains unproven |
| General shot probability, arcs, and landing | deterministic native approximation | remains explicitly labeled and transactionally validated |
| Dunk cutaway | native-faithful preserved source schedule | cue, cutaway, intentional source-frame-64 black frame, and court return pass |

The public numeric-1 name therefore intentionally remains `"invalid"` in
`src/tecmo_gameplay_scene.c`. The source-map sentence that calls numeric 1
“intentionally unexposed” also remains stale and deferred. Neither shared file
was writable in this lane, and neither wording is upgraded here.

## Combined gate result

All accepted gates passed from merged tree `2d918e8d...`:

- full MSVC `/W4` build, both executables, exit 0, zero warning/error
  diagnostics;
- focused TGCS and TGSR suites, including 43 TGCS source mutations and all
  point/polarity/route/rattle/claimant vectors;
- full gameplay-scene runner and direct scene self-test;
- gameplay-state replay `7A204A525C79D21C`;
- clock, free-throw lineup, fatigue, audio, camera/court, penalties,
  out-of-bounds, backcourt, referee, CPU steering, movement, HUD, and period
  expiry coverage through the complete scene gate;
- 55/55 broad asset-pack tests over the 86-entry, 1,401,618-byte pack;
- intro, bound native flow/CLI boundaries, Win32 launch, music, frontend and
  gameplay audio, season, team-data, and team-management smokes.

The intro report contains one accepted skipped bounded pixel-mask subcase
because its reference PNGs were unavailable. Its other 29 reported cases pass,
including the active native semantic, malformed, deterministic, and pixel
checks. This skip is not represented as executed coverage.

## Deterministic native visual evidence

Fresh R2B shot proof is under the ignored root
`build/r2b-sol-native-shots-20260804T013200Z-v2` with manifest SHA-256
`96C260D23B27559C9B0907264F80EDCEE56E75A2526BF87ABC356FE82CF884CB`.

It contains two passes over 51 selected source checkpoints per pass:
make 11, miss 15, rattle 16, and dunk 9. The 102 640x480 frame records, eight
contact sheets, and eight MP4s are path-contained and hash-valid. All
corresponding frame, sheet, and fresh-video pairs are byte-equal. The four
ordered frame aggregates exactly match accepted R2 values.

Sol opened all four pass-1 contact sheets at original detail and separately
opened dunk `frame-0005.png`, the intentional full-black source frame 64.
There was no visible corrupt sprite, clipping, HUD collision, torn transition,
route discontinuity, or cutaway error. Independent Luna inspection reached the
same visual conclusion.

The installed FFmpeg 8.1 build produces MP4 container hashes different from
the historical R2 encoder hashes while preserving exact rendered-frame
aggregates and byte-equal fresh pass pairs. This is recorded as encoder
toolchain variance, not source or rendering variance.

The complete scene smoke proof remains at
`build/r2b-sol-scene-20260804T012159Z`, manifest SHA-256
`111788F30FC2E834C158E6EDCE38A4CCE3F395194A9FD548E03297921BE6B0EA`.
All 254 declared artifacts independently match path, byte count, and hash.
This manifest is deliberately `DRAFT`, identifies its generic task as
`R1-LIVE-FOUNDATION`, has `require_pass=false`, `build_requested=false`,
`build_warning_clean=false`, and `final_sha=PENDING_CLEAN_COMMIT`. It is used
only as live-scene integration-smoke evidence together with the separate fresh
warning-clean R2B build and complete gate ledger; it is not called a standalone
R2B final-acceptance manifest.

## Honest residual limits

The following accepted gaps remain incomplete and are not release blockers for
this integration QA lane:

1. Full numeric-1 object/trajectory semantics and a public semantic name.
2. Full native inputs to `$91BC` and `$AD6E`.
3. Raw runtime state replaced by documented neutral geometry/stable-sample
   substitutions.
4. Native dynamic claimant scan order and semantic claimant labels.
5. General arcs, tail motion, probability, and landing beyond the exact pieces.
6. Deferred CPU autonomous jump/far-shot behavior.
7. Presentation beyond existing TGDK, camera, HUD, and focused proof contracts.
8. The stale numeric-1 source-map wording and public `"invalid"` name.

## Evidence map and handoff

- `COMMANDS.md` records commands, results, diagnostics, and final verification.
- `EVIDENCE.md` records source/ASM classification and proof integrity.
- `INDEPENDENT-QA.md` records the sole persistent Luna review.
- `LINEAGE.md` records control, candidate, merge, scope, and guarded handoff.
- `FAULT-LEDGER.md` records non-product harness/configuration retries.

Main, staging, `origin/main`, live remote main, and push remained read-only.
Master alone owns the final guarded fast-forward and any ordinary push.
