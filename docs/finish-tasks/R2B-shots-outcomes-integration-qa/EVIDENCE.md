# R2B evidence, source audit, and classifications

## Scope of personal audit

Sol read the accepted R2 task documents, candidate changes, merged shot
headers and implementations, scene orchestration and validation, transactional
TGJS/TGDK/TGSR seams, CLI/direct tests, and the relevant clock, lineup, fatigue,
audio, intro, season, flow, Win32, TIP, and TPTI-2 boundaries.

The research decompilation was consulted read-only for address claims. ROM,
ASM, traces, generated packs, PNGs, MP4s, and logs remain ignored local evidence
and are not tracked or runtime dependencies.

## Source and ASM classification audit

| Contract | Read-only source observation | Accepted classification | R2B conclusion |
| --- | --- | --- | --- |
| Profile selector | Bank02 `$A89E-$A8C9`: `$A8AE` reads profile byte 2, `$A8BA` masks `#$20`, `$A8BC` stores `$04B0`; Bank05 consumes the bit | exact/source-pinned bit gate | merged code uses `profile[2] & 0x20`; unavailable raw runtime state is not claimed |
| Close numeric dispatch | Bank05 `$8BDE->$8C79->$8C7D`; `$8C79` writes numeric 1; `$8C7D` indexes state and pose tables | exact numeric identity and dispatch | identities 0/1/2 remain separate |
| Numeric-1 fixed group | `$8CC6` selects fixed base `$10`; `$8CED-$8D3C` contains eight direction entries | exact/source-backed pose group; native pose-only schedule | semantic kind remains unknown; full object/trajectory semantics remain incomplete |
| Jump matrix | Bank05 `$842C-$845E` consumes family, profile bit, and direction | exact/source-pinned selection; native substitution for unavailable family state | selected family/profile/direction and persisted flight pose pass; substitutions remain documented |
| Point classifier | Bank05 `$B995-$BA3F`; `$0398` starts at 1 and advances to 2/3 through source geometry | exact/source-pinned pieces; native geometry binding | immutable launch geometry recomputes 1/2/3 transactionally |
| Terminal polarity | Bank05 `$91BC-$943A`; `$933B` compares threshold `$9A` with sample `$6A`; `$942D` clears bit 7 for make, `$9434` sets it for miss | exact/source-pinned polarity for supported contexts | supported exact three-point schedule and general native schedule remain distinct |
| Rim route dispatch | Bank05 `$A6EE`: low two bits select `$A708,$A7A9,$A8E9,$A708` | exact raw selector/address identity | all four selectors retain selector, route kind, and CPU address; tails remain native approximations |
| Rattle begin/update | `$A7A9` loads snap tables `$BDF3/$BDF5`, altitude `$38`, state/cadence; `$A854` updates rattle; `$A2DF/$AD4E` bridges | exact/source-pinned bounded four-pass prefix | orientation, repeat cadence, object state, velocity restore, and terminal bridge pass |
| Claimant eligibility/order | Bank05 `$B73E` iterates `$9C` through 0..9 and calls `$B80E`; `$B80E` applies bounded position/state predicates | eligibility/relation inputs source-backed; native active-slot order approximation | no rebound, steal, block, or recovery semantic name is inferred |
| Settlement | Bank05 `$B87C-$B8F5` transfers state based on claimant relation | native-faithful terminal relation with incomplete full helper state | same-team possession retention, opponent change, and made-score handoff pass |
| Dunk cutaway | TGDK source records and retained stage/cue schedule | native-faithful preserved schedule; incomplete full presentation mapping | exact cutaway assets remain source-bound; source frame 64 is intentionally black |

Representative research files inspected include the Bank02 roster/team/player
span, Bank05 animation/pose tables, `$91BC-$943A`, and the large Bank05
state/trajectory cluster. Those observations do not establish unavailable full
`$91BC` probability inputs, full `$AD6E` launch inputs, general collision
semantics, native claimant meaning, or complete numeric-1 trajectory.

## Transactional and fail-closed audit

TGCS, TGJS, TGDK, and TGSR loaders parse into independent candidate storage and
publish only after size, header, fingerprint, padding, source-span, same-pack
dependency, and pointer/index checks succeed. A valid object survives a later
invalid load byte-for-byte; a fresh invalid destination publishes a precise
failure without becoming available.

The scene boundary stages actor, ball, result, audio, score, possession,
rattle, and settlement changes in a scene copy. Post-mutation validation and
live-foundation synchronization must succeed before publication. Important
reviewed invariants include:

- native policy samples bind actor coordinates, immutable hoop delta, point value,
  team, roster identity, and all nonzero launch-frame bytes;
- redundant context signatures bind native policy sample plus contact/contest bits;
- contact implies contest;
- point class binds immutable launch geometry, not mutable in-flight state;
- numeric 0, numeric 1, numeric 2, and jump schedules remain distinct;
- the exact three-point make schedule remains limited to its supported context;
- general make schedules, probability, arcs, and landing remain native
  approximations;
- A708 selector 0/3, A7A9, and A8E9 retain raw identity through rim tails;
- rattle reconstructs source-backed state and validates each repeat/velocity
  boundary;
- source-slot claimant settlement adds no unsupported semantic label;
- period-expiry live settle awards or closes exactly once;
- result audio and DMC requests remain transactional.

No classification string, comment, enum, public name, or document upgrades an
accepted approximation to exactness.

## Shared-file exclusion and accepted-main seam

The original exclusion blobs were verified at candidate base, all candidate
commits, old main, the old predicted tree, and original merge `26e6aaf...`:

| Path | Original exact blob |
| --- | --- |
| `src/tecmo_gameplay_scene.c` | `58ad821d31a5559225855fbb30a1566d374063e7` |
| `src/asset_pack/tecmo_asset_pack_source_map.c` | `b6fc46a927f1a0cddedf7a965d3ebb4ad7d23b7f` |

Accepted main `0ef11cf...` alone advances them to:

| Path | Reconciled blob | Audit result |
| --- | --- | --- |
| `src/tecmo_gameplay_scene.c` | `504d3d0459780779f47a533ce8bb548208a4195d` | TIP/TPTI-2 transactional load/pretip work; shot-name switch remains none/jump/dunk/layup and numeric 1 remains `"invalid"` |
| `src/asset_pack/tecmo_asset_pack_source_map.c` | `40417d8544ce9ffaca7b7110f341fa82bd4b486f` | TIP/TPTI-2 metadata/dependencies; numeric 1 remains unsupported and raw-group policy remains `"intentionally unexposed"` |

The normalized reconciliation overlap is zero. These accepted-main changes do
not alter R2 shot classifications or close the deferred wording gap.

## Build and focused evidence

| Gate | Reconciled result |
| --- | --- |
| Full MSVC `/W4` build | PASS, exit 0, both executables, warning/error lines 0 |
| TGCS-1 | PASS, 208 poses, canonical/provenance/reload, negative cases, 43 Rev1 mutations |
| TGSR-3 | PASS, four primary plus four lookup spans, point 1/2/3, polarity, raw routes, rattle 1..4, claimant settlement |
| TPTI-2 | PASS, canonical/source-map/dependency/mutation, input/abort/freeze/toss/jump/live render matrix |
| Full gameplay scene | PASS, all listed scene/shot/cross-seam suites |
| Direct scene self-test | PASS |
| Gameplay-state self-test | PASS, replay `7A204A525C79D21C` |
| TGFL-1 / TGFT-1 / TGAI-1 | PASS / PASS / PASS |

The full scene gate composes clock, lineup, fatigue, camera, court, penalties,
referee, movement, CPU, audio, HUD, period-expiry, TIP/TPTI, and pack seams with
the R2 shot schedules.

Reconciled executables:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/tecmo_port.exe` | 2,174,464 | `A03CB118A982AB6F7B35A68DB666F55D22D1839AAFF05EC3A864AFE272A92DA2` |
| `build/tecmo_port_game.exe` | 2,174,464 | `C2ECAED97ED4917095F75D00B34BF4F3A6048414163A572B66335FF44F7BD2AB` |

## Broad and cross-domain evidence

Reconciled root `build/r2b-sol-recon-20260804T021505Z` contains the 86-entry,
1,406,713-byte pack with SHA-256
`27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29`.

| Evidence | Result | Report SHA-256 |
| --- | --- | --- |
| Broad asset pack | 55/55 PASS, direct self-test PASS | `D6E1155A1CCC69B651B06580B2C1B39BDF5081AAEB3B1A78A3538D57B224F331` |
| Intro | 29/29 PASS, one accurately flagged skip for missing Bucks reference PNGs | `CB681A508929422144A12D42C5A2A4AA06C8938146B73C9655B762DA97DCF757` |
| Isolated bound native flow | all CLI boundaries and flow PASS, process exit 0 | `932334D5AAB9CBA662A63218EB160C1B0A46C637D41B3090744595A362CD96B9` |

Bound direct flow and bound Win32 launch pass. Music, frontend audio, gameplay
audio, season, team data, and team management also pass. Pack-binding and stale
exit-code diagnostics are fully recorded in `FAULT-LEDGER.md`; bound isolated
reruns have no product failure.

## Native shot proof integrity and visual review

Primary reconciled manifest:
`build/r2b-sol-recon-native-shots-20260804T022200Z/PROOF-MANIFEST.json`.

Manifest SHA-256:
`301AEEF5610ACE250C888CEA8B666E6260A0B195C3822AF2B3BCB3B59AB477D3`.

Its bound pack hash is `27D4CEB4...CA6B29`; manifest head/tree are exact
`d3f1980d...` / `37bbb386...`. Independent Sol inventory results:

- exactly 119 files: 102 frames, eight contact sheets, eight MP4s, manifest;
- frames are 640x480 and sheets match declared dimensions;
- zero escaped, missing, unlisted, bad-hash, bad-dimension, or bad-pair records;
- all corresponding frames and fresh videos are byte-equal across passes;
- accepted ordered aggregates are exact:
  - make `47D5B332BCFEFEA472C5CA4FDFCDC9A646FC5CE9E3FD208C6686807B2F74BB99`;
  - miss `B1E87ECF18121FE57348C46326C1C54A5657FB6633362A36AEAB852E030B9AEF`;
  - rattle `9447A2693D285FE702B3C7D67CF7D554C4962CB3B7122F0845FE633C8230AF5A`;
  - dunk `F2C128049E5E28BC9750F6A55011FE2B7065D97D9CFE18B329C43DCEA588CEFD`.

FFmpeg 8.1 fresh MP4 hashes are make `65CE0BD6...E0B3`, miss
`DF3000FD...B7AC`, rattle `7F87E6C5...4426`, and dunk
`F2B60D38...84E3`, exactly equal between passes and exactly reproduced from the
earlier fresh run. Their difference from historical container hashes is
encoder/toolchain variance.

Original-detail review of four pass-1 sheets plus dunk ordinal 5 found coherent
make, miss, multi-frame rattle, and dunk progression; expected source-frame-64
blackout; readable HUD/court/sprites; and no corrupt tile, clipping, overlap,
torn transition, or route discontinuity. Sol severity is P0=0/P1=0/P2=0.
The same pinned Luna independently recomputed the inventory, hashes,
dimensions, aggregates, and pass pairs, opened the fresh visuals, and returned
P0=0/P1=0/P2=0 at exact signed docs tip `fcc520998...`.

## Scene proof integrity and caveat

Secondary reconciled scene manifest:
`build/r2b-sol-recon-scene-20260804T021505Z/PROOF-MANIFEST.json`, SHA-256
`C6ABD02253659272F9DBFCF8FD76289D6C13E9B4F5C375BC22E2279ABF0FC54B`.

All 254 declared artifact records are unique and pass root-containment,
byte-count, and SHA-256 checks. The root has 255 files total because the
manifest cannot inventory its own hash. It contains 14 frame records, two
sheets, two videos, 189 required logs, and 13 negative-regression records.

The manifest remains `DRAFT`, task `R1-LIVE-FOUNDATION`,
`final_sha=PENDING_CLEAN_COMMIT`, `require_pass=false`,
`build_requested=false`, and `build_warning_clean=false`. It is scene-smoke
evidence only beside the fresh warning-clean build, full gates, direct tests,
deterministic shot proof, and independent review.

## Ownership, proprietary scan, and accepted incompletes

The immutable candidate owns exactly the 17 paths in `LINEAGE.md`; candidate
vs old-main and reconciled R2B vs new-main normalized overlaps are both zero.
Diff checks pass. No ROM, pack, capture, trace, PNG, MP4, log, binary, or other
proprietary payload is tracked by R2B.

Still incomplete: numeric-1 full semantics/trajectory/name; complete `$91BC`
and `$AD6E` inputs; neutral substitutions for unavailable state; native
claimant scan ownership and semantic action names; general probability, arcs,
tails, landing, and presentation; CPU autonomous jump/far-shot policy; and the
deferred public/source-map wording. No R2B evidence silently closes these gaps.
