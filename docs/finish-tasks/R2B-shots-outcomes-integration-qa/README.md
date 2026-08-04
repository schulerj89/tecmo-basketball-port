# R2B shots and outcomes integration QA

This folder records authoritative Sol integration QA for
`R2B-SHOTS-OUTCOMES-INTEGRATION-QA`, session
`S-SOL-R2B-SHOTS-INTEGRATION-QA-001`, claim
`OWN-R2B-SHOTS-OUTCOMES-INTEGRATION-QA`, and lane
`LANE-R2B-SHOTS-OUTCOMES-INTEGRATION-QA`.

The accepted R2 candidate is immutable commit
`7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a`. It was first integrated with
main `8a5b9928544a430efa34cbf98a248d6a8cbe7b14` by Good-signed branch-only
merge `26e6aaf19b639972cb9043f29fc55daa1efce835`, tree
`2d918e8d672f991c87c293096e315a8bde5685da`.

Main later advanced to accepted TIP terminal
`0ef11cf247e3110b6064e79a4c496be6346f3e13`. The tested reconciled product is
Good-signed branch-only merge
`d3f1980d1d9147c47bd6a3bd555708ad6bfcb0f9`, with ordered parents
`6eaaa535fa69a12e0f63012470dcc052583351b5` first and `0ef11cf...` second.
Its tree is the precomputed and reproduced conflict-free tree
`37bbb3868ee1b2b35fbaec1f7801213d648d7fb0`.

## Decision

Sol's reconciled product, source/ASM, deterministic, visual, and cross-domain
review found no actionable defect: **P0=0, P1=0, P2=0**.

The sole independent projectless `gpt-5.6-luna` reviewer at thinking `max`,
task `019fca5b-3b84-7a82-b2ab-588c50a4b7fd`, already returned
**P0=0, P1=0, P2=0** for the original merged source and supplied evidence.
The same pinned reviewer is reused for the reconciled lineage, fresh evidence,
and exact signed-document tip. This docs candidate is not called terminal until
that final read-only review is recorded in `INDEPENDENT-QA.md`.

No candidate or product correction was made in R2B. Tracked lane mutations are
the two authorized Good-signed no-ff branch-only merges and documentation in
this folder. Main, staging, `origin/main`, live remote main, and push remained
read-only.

## Acceptance anchors

- Clean takeover began at exact main `8a5b992...`; immutable staging resolved
  exactly to `7b9287a...`, three commits after candidate base
  `222d75cfafa9153db1eb44492bf557f11b1a9091`.
- Candidate ownership is exactly 17 accepted normalized paths. The initial
  main side had 80 paths from candidate base, with normalized overlap zero.
- The original merge has parents `8a5b992... 7b9287a...` and exact predicted
  tree `2d918e8d...`.
- Before reconciliation, local main, `origin/main`, and live remote main all
  resolved to accepted Good-signed `0ef11cf...`; the merge base with R2B was
  `8a5b992...`.
- Reconciliation changed-path counts were 23 on the R2B lineage and 31 on the
  new-main side, with normalized overlap zero, both diff checks clean, and
  predicted tree `37bbb386...`.
- Reconciliation merge `d3f1980d...` has exact ordered parents
  `6eaaa535... 0ef11cf...`, exact tree `37bbb386...`, and a Good SSH signature.
- Control `981aa4e3b0aece8569b0be247d0e27ef88fa02c7`, both main tips, all
  candidate commits, both merges, and docs checkpoint `6eaaa535...` verify
  Good for `jaystar524@gmail.com`, RSA key
  `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

## Shared-file disposition after accepted TIP main

The original R2 candidate exclusion contract was exact through the original
merge:

| Path | Original excluded blob |
| --- | --- |
| `src/tecmo_gameplay_scene.c` | `58ad821d31a5559225855fbb30a1566d374063e7` |
| `src/asset_pack/tecmo_asset_pack_source_map.c` | `b6fc46a927f1a0cddedf7a965d3ebb4ad7d23b7f` |

Accepted new main alone advances those paths to blobs
`504d3d0459780779f47a533ce8bb548208a4195d` and
`40417d8544ce9ffaca7b7110f341fa82bd4b486f`, respectively. Their changes are
TIP/TPTI-2 transactional and metadata work, not R2 shot-outcome changes. The
reconciliation overlap is still zero.

Personal seam audit confirms the classification invariant remains intact:
public numeric 1 still falls through to `"invalid"`; the source map still says
`unsupported_numeric_ids:[1]` and `unsupported_raw_group_policy` is
`"intentionally unexposed"`; the public shot-name switch remains only
none/jump/dunk/layup. R2B does not upgrade those labels.

## Product and classification disposition

| Area | Accepted classification | Reconciled disposition |
| --- | --- | --- |
| TGCS numeric 0/2 | exact/source-pinned pose and phase data | preserved; focused gate clean |
| TGCS numeric 1 | source-backed fixed pose group; native pose-only approximation | identity preserved; semantic meaning remains unknown |
| TGJS profile/family/direction | exact/source-pinned selection with documented native substitutions | selected pose survives through flight; no stronger claim |
| TGSR point classifier | exact/source-pinned pieces with native geometry binding | immutable launch inputs classify 1/2/3 transactionally |
| TGSR terminal polarity | exact for supported captured contexts | supported schedule and polarity pass |
| A708/A7A9/A8E9 routes | exact raw selector/address identity; native approximate tails | all raw routes remain distinct; no semantic steal/block/rebound name |
| Four-pass rattle | exact/source-pinned bounded prefix | state, cadence, orientation, velocity restore, and bridge pass |
| Claimant settlement | source-order native approximation plus proven team relation | team relation passes; native scan ownership remains incomplete |
| General probability/arcs/landing | deterministic native approximation | remains explicitly approximate and transactionally validated |
| Dunk cutaway | native-faithful preserved source schedule | cue, stages, expected source-frame-64 black frame, and return pass |

## Reconciled combined gate result

All required gates passed from tree `37bbb386...`:

- full MSVC `/W4` build, both executables, zero warning/error diagnostics;
- TGCS-1, TGSR-3, and newly affected TPTI-2 focused suites;
- complete gameplay-scene runner, direct scene self-test, and gameplay-state
  replay `7A204A525C79D21C`;
- TGFL-1 free-throw lineup, TGFT-1 fatigue, and TGAI-1 CPU steering;
- 55/55 broad asset-pack tests over the rebuilt 86-entry, 1,406,713-byte pack,
  plus direct asset-pack self-test;
- intro 29/29, with the same one explicitly skipped bounded Bucks pixel-mask
  subcase because reference PNGs were unavailable;
- bound direct flow, isolated bound native-flow process exit 0, and bound
  Win32 launch;
- music, frontend audio, gameplay audio, season, team data, and team management.

Reconciled executable hashes are `A03CB118...92DA2` for `tecmo_port.exe` and
`C2ECAED9...BD2AB` for `tecmo_port_game.exe`.

## Deterministic native visual evidence

Fresh reconciled shot proof is under ignored root
`build/r2b-sol-recon-native-shots-20260804T022200Z`, manifest SHA-256
`301AEEF5610ACE250C888CEA8B666E6260A0B195C3822AF2B3BCB3B59AB477D3`.

It contains exactly 119 files: 102 selected 640x480 frames, eight contact
sheets, eight MP4s, and the manifest. The two passes cover 51 checkpoints each:
make 11, miss 15, rattle 16, and dunk 9. A separate read-only audit found zero
escaped, missing, unlisted, hash, dimension, aggregate, or pass-pair failures.
All four ordered frame aggregates exactly match accepted R2 values.

Sol opened the four pass-1 sheets at original detail and separately checked the
intentional all-black dunk ordinal 5/source frame 64. Make, miss, rattle, and
dunk progression is coherent; HUD, court, and sprites remain readable; no
corrupt sprite, clipping, overlap, torn transition, or route discontinuity is
visible. Sol visual severity is **P0=0, P1=0, P2=0**.

FFmpeg 8.1 again reproduces the prior fresh MP4 hashes exactly on both passes,
while those container hashes differ from historical encoder hashes. Exact PNG
aggregates and fresh pass equality make this container/toolchain variance, not
rendering variance.

The fresh reconciled scene smoke is under
`build/r2b-sol-recon-scene-20260804T021505Z`, manifest SHA-256
`C6ABD02253659272F9DBFCF8FD76289D6C13E9B4F5C375BC22E2279ABF0FC54B`.
All 254 declared artifacts independently match containment, byte count, and
hash; the only additional file is the manifest itself.

That scene manifest deliberately remains `DRAFT`, identifies generic task
`R1-LIVE-FOUNDATION`, has `require_pass=false`, `build_requested=false`,
`build_warning_clean=false`, and `final_sha=PENDING_CLEAN_COMMIT`. It is scene
integration-smoke evidence only when paired with the separate fresh build and
complete gate ledger; it is not a standalone R2B terminal manifest.

## Honest residual limits

The following accepted gaps remain incomplete and are not upgraded here:

1. Full numeric-1 object/trajectory semantics and a public semantic name.
2. Full native inputs to `$91BC` and `$AD6E`.
3. Neutral geometry/stable-sample substitutions for unavailable runtime state.
4. Native dynamic claimant scan ownership and semantic claimant labels.
5. General probability, arcs, tail motion, landing, and full presentation.
6. CPU autonomous jump/far-shot policy.
7. Deferred numeric-1 source-map wording and public `"invalid"` name.

## Evidence map and handoff

- `COMMANDS.md` records takeover, reconciliation, gates, proofs, and guards.
- `EVIDENCE.md` records source/ASM classification and artifact integrity.
- `INDEPENDENT-QA.md` records the sole persistent Luna review.
- `LINEAGE.md` records control, candidate, merges, scope, and guarded handoff.
- `FAULT-LEDGER.md` records non-product harness/configuration diagnostics.

Master alone owns the final guarded fast-forward and any ordinary push.
