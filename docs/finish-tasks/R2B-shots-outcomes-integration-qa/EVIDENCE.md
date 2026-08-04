# R2B evidence, source audit, and classifications

## Scope of personal audit

Sol read the accepted R2 task documents, candidate changes, merged shot
headers and implementations, scene shot orchestration, shot-state validation,
transactional TGJS/TGDK changes, CLI/direct-test seams, and the relevant
current-main clock, lineup, fatigue, audio, intro, season, flow, and Win32
boundaries.

The research decompilation was consulted read-only to check the address claims.
ROM, ASM, traces, generated packs, PNGs, MP4s, and logs remain ignored local
evidence and are not tracked or runtime dependencies.

## Source and ASM classification audit

| Contract | Read-only source observation | Accepted classification | R2B conclusion |
| --- | --- | --- | --- |
| Profile selector | Bank02 `$A89E-$A8C9`: `$A8AE` reads profile byte 2, `$A8BA` masks `#$20`, `$A8BC` stores `$04B0`; Bank05 consumes the bit | exact/source-pinned bit gate | merged code uses `profile[2] & 0x20`; unavailable raw runtime state is not claimed |
| Close numeric dispatch | Bank05 `$8BDE->$8C79->$8C7D`; `$8C79` writes numeric 1; `$8C7D` indexes state and pose tables | exact numeric identity and dispatch | identities 0/1/2 remain separate |
| Numeric-1 fixed group | `$8CC6` selects fixed base `$10`; `$8CED-$8D3C` contains the eight direction entries | exact/source-backed pose group; native pose-only schedule | semantic kind remains `UNKNOWN`; full object/trajectory semantics remain incomplete |
| Jump matrix | Bank05 `$842C-$845E` consumes family, profile bit, and direction | exact/source-pinned selection; native substitution for unavailable family state | selected family/profile/direction and persisted flight pose pass; substitutions stay documented |
| Point classifier | Bank05 `$B995-$BA3F`; `$0398` starts at 1 and advances to 2/3 through source geometry | exact/source-pinned pieces; native geometry binding | immutable launch geometry recomputes 1/2/3 transactionally |
| Terminal polarity | Bank05 `$91BC-$943A`; `$933B` compares threshold `$9A` with sample `$6A`; `$942D` clears bit 7 for make, `$9434` sets it for miss | exact/source-pinned polarity for supported contexts | supported exact three-point schedule and general native schedule remain distinct |
| Rim route dispatch | Bank05 `$A6EE`: low two bits select `$A708,$A7A9,$A8E9,$A708` | exact raw selector/address identity | all four selectors retain selector, route kind, and CPU address; tails remain native approximations |
| Rattle begin/update | `$A7A9` loads snap tables `$BDF3/$BDF5`, altitude `$38`, state/cadence; `$A854` updates the rattle; `$A2DF/$AD4E` bridge | exact/source-pinned bounded four-pass prefix | orientation, repeat cadence, object state, velocity restore, and terminal bridge pass |
| Claimant eligibility/order | Bank05 `$B73E` iterates `$9C` through 0..9 and calls `$B80E`; `$B80E` applies bounded position/state predicates | eligibility and relation inputs source-backed; native active-slot order approximation | no rebound, steal, block, or recovery semantic name is inferred |
| Settlement | Bank05 `$B87C-$B8F5` transfers state based on claimant relation | native-faithful terminal relation with incomplete full helper state | same-team possession retention, opponent change, and made-score handoff pass |
| Dunk cutaway | TGDK source records and retained stage/cue schedule | native-faithful preserved schedule; incomplete full presentation mapping | exact cutaway assets remain source-bound; source frame 64 is intentionally black |

Representative research files inspected include:

- `decomp/lifted/bank02/C-0177_bank02_roster_team_player_data_9000_BFFF.asm`;
- `decomp/lifted/bank05/C-0094_bank05_anim_state_machine_cluster_88F9_8CE4.asm`;
- `decomp/lifted/bank05/C-0095_bank05_state_and_pose_lookup_tables_8CE5_8D7C.asm`;
- `decomp/lifted/C-0005_bank05_91BC_943A.asm`;
- `decomp/lifted/bank05/C-0111_bank05_large_state_and_trajectory_cluster_985B_BFA7.asm`.

The address and table observations support only the classifications above.
They do not establish the unavailable full `$91BC` probability inputs, full
`$AD6E` launch inputs, general collision semantics, native claimant meaning, or
complete numeric-1 trajectory.

## Transactional and fail-closed implementation audit

The merged TGCS, TGJS, TGDK, and TGSR loaders parse into independent candidate
storage and publish only after size, header, fingerprint, padding, source-span,
same-pack dependency, and pointer/index checks succeed. A valid loaded object
survives a later invalid parse/load byte-for-byte. A fresh invalid destination
publishes a precise failure status without becoming available.

The scene shot boundary follows the same candidate-copy pattern:

- start and update paths validate the current boundary;
- actor, ball, result, audio, score, possession, rattle, and settlement changes
  are staged in a scene copy;
- post-mutation validation and live-foundation synchronization must succeed
  before publication;
- invalid schedule, actor/controller binding, context signature, route,
  rattle, score, expiry, or audio state fails closed without partial mutation.

Important reviewed invariants include:

- stable samples bind actor coordinates, immutable hoop delta, point value,
  team, roster identity, and all nonzero launch-frame bytes;
- redundant context signatures bind stable sample plus contact/contest bits;
- contact implies contest;
- point class is bound to immutable launch geometry, not mutable in-flight
  actor state;
- numeric 0, numeric 1, numeric 2, and jump schedules stay distinct;
- the exact three-point make schedule is limited to its supported context;
- general make schedules, probability, arcs, and landing remain marked native
  approximation;
- A708 selector 0/3, A7A9, and A8E9 retain raw identities through rim tails;
- rattle state reconstructs the source-backed object and validates every
  pass/repeat/velocity boundary;
- source-slot claimant settlement does not introduce an unsupported semantic
  label;
- period-expiry live settle awards or closes exactly once and does not expose a
  synthetic extra live update;
- result audio and DMC requests are transactional.

No classification string, comment, enum, public name, or document was found
that upgrades an accepted approximation to exactness.

## Exclusions and deferred shared wording

The excluded shared files retain exact blobs:

| Path | Required blob | Observed disposition |
| --- | --- | --- |
| `src/tecmo_gameplay_scene.c` | `58ad821d31a5559225855fbb30a1566d374063e7` | untouched; public numeric 1 still falls through to `"invalid"` |
| `src/asset_pack/tecmo_asset_pack_source_map.c` | `b6fc46a927f1a0cddedf7a965d3ebb4ad7d23b7f` | untouched; “intentionally unexposed” wording remains stale/deferred |

That wording is an honest metadata/name follow-up, not evidence that runtime
numeric-1 pose exposure failed.

## Build and focused evidence

| Gate | Result |
| --- | --- |
| Full MSVC `/W4` build | PASS, exit 0, both executables, zero diagnostics |
| TGCS focused runner | PASS, 208 poses, canonical/provenance/reload, strict negative cases, 43 source mutations |
| TGSR focused runner | PASS, source spans, point 1/2/3, polarity, routes, rattle 1..4, claimant settlement |
| Full gameplay-scene runner | PASS, all listed scene/shot/cross-seam suites complete |
| Direct scene self-test | PASS |
| Gameplay-state self-test | PASS, replay `7A204A525C79D21C` |

The complete scene runner covers current-main clock, lineup, fatigue, camera,
court, penalty, referee, movement, CPU, audio, HUD, period-expiry, and pack
seams in the same scene that executes the R2 shot schedules.

## Broad and cross-domain evidence

The canonical generated pack is 1,401,618 bytes, 86 entries, SHA-256
`CC9A522A1EC5025193FD525419096D5A5AA15AF2F63A9E18E68DB8D81E87AC6F`.

| Evidence | Result | Durable report SHA-256 |
| --- | --- | --- |
| Broad asset-pack | 55/55 PASS, including shots provenance and malformed/cross-pack rejection | `41B8584D034E5466058D757A6CFA19A665E4660553C0B8C7A736FE51B7DB3E8B` |
| Intro sequence | report PASS; 29 cases pass; one bounded pixel-mask case skipped for missing reference PNGs | `0687BF25D767A3CAD5AA8D37B5C4BD75976D1929C907C75A13C7740F694F33D3` |
| Native flow | flow and CLI boundary matrix PASS | `932334D5AAB9CBA662A63218EB160C1B0A46C637D41B3090744595A362CD96B9` |

Focused TGFL and TGFT pass. Music, frontend audio, gameplay audio, season, team
data, team management, direct bound flow, and Win32 launch also pass. The
initial unbound flow setup stop and stale-`$LASTEXITCODE` wrapper are recorded
as corrected configuration/harness diagnostics, not accepted gate failures.

## Native shot proof integrity

Primary R2B manifest:
`build/r2b-sol-native-shots-20260804T013200Z-v2/PROOF-MANIFEST.json`.

Manifest SHA-256:
`96C260D23B27559C9B0907264F80EDCEE56E75A2526BF87ABC356FE82CF884CB`.

Independent Sol and Luna integrity passes agree:

- exact root inventory: 110 PNG, 8 MP4, 1 manifest = 119 files;
- 102 numbered 640x480 scenario frames, 51 per pass;
- eight 1300-pixel-wide tiled sheets;
- no escaped, missing, unlisted, wrong-size, wrong-dimension, or wrong-hash
  record;
- every corresponding frame pair is byte-equal;
- every contact-sheet pair is byte-equal;
- every fresh-video pair is byte-equal;
- all four ordered frame aggregates equal the accepted R2 aggregate;
- current FFmpeg 8.1 video hashes differ from historical containers but not
  from one another across fresh passes.

Visual review at original detail found:

- make: coherent gather, release, arc, score change, and clean return;
- miss: coherent release, flight, miss tail, unchanged shooting score, and
  clean return;
- rattle: visibly distinct multi-frame rim motion, correct handoff, unchanged
  shooting score;
- dunk: court approach, cutaway, visible dunk, intentional all-black source
  frame 64, court rebuild, and resumed route;
- no corrupt sprites, torn frame, unexpected clipping, unreadable HUD, or
  inconsistent scene transition.

Sol severity: P0=0, P1=0, P2=0. Independent Luna severity: P0=0, P1=0,
P2=0.

## Complete scene proof integrity and caveat

Secondary scene-smoke manifest:
`build/r2b-sol-scene-20260804T012159Z/PROOF-MANIFEST.json`.

SHA-256:
`111788F30FC2E834C158E6EDCE38A4CCE3F395194A9FD548E03297921BE6B0EA`.

All 254 declared artifact records pass path-containment, byte-count, and hash
verification. Two contact sheets, two videos, decoded hashes, and all 14 stored
frame records are repeat-equal.

The manifest remains an integration-smoke `DRAFT`. It reports task
`R1-LIVE-FOUNDATION`, `require_pass=false`, `build_requested=false`,
`build_warning_clean=false`, and `final_sha=PENDING_CLEAN_COMMIT`. It is not a
standalone R2B terminal manifest and is not used to claim a build it did not
perform. Its scene evidence is accepted only together with the separate R2B
warning-clean build, focused gates, direct self-test, deterministic shot proof,
and independent review.

## Ownership and proprietary scan

Candidate ownership is exactly the accepted 17-path set recorded in
`LINEAGE.md`. Candidate/current-main normalized path overlap is zero. Diff
checks pass. The changed candidate set contains source, headers, runners, and
Markdown only; no ROM, pack, capture, trace, PNG, MP4, log, binary, or other
proprietary payload is tracked.

R2B tracked additions are confined to
`docs/finish-tasks/R2B-shots-outcomes-integration-qa/**`.

## Accepted incomplete work

The following remain explicitly out of the exactness claim:

- numeric-1 full object/trajectory semantics and semantic public name;
- complete `$91BC` and `$AD6E` helper inputs;
- neutral family/close substitutions for unavailable raw state;
- full native claimant scan ordering and semantic action names;
- general probability, arcs, tail motion, landing, and complete presentation;
- CPU autonomous jump/far-shot policy;
- deferred shared-file public name and source-map wording.

No R2B evidence or decision silently closes these gaps.
