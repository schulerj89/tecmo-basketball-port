# Combined QA evidence

## Accepted product boundary

The base-to-candidate change contains exactly six regular text paths: four
accepted R2 task reports, `src/tecmo_cli_render_gameplay_checkpoint.c`, and
`tools/Run-GameplayPresentationTests.ps1`. The product implementation adds only
canonical `gameplay-layup-frameN` parsing/execution for frames 1-17.

The live fixture writes only actor position/anchor, holder/ball coordinates,
and camera settle. Normal production cancel input enters
`scene_start_shot`; the existing selector chooses layup numeric variant 2. The
checkpoint observes that selection during active frames and observes
`shot=none` at frame 17. It does not write shot kind, close-shot variant, pose,
schedule, outcome, score, claimant, settlement, or layup facing.

Parser validation remains transactional. Only decimal canonical suffixes 1-17
are accepted. Empty, signed, nondecimal, trailing-character, leading-zero,
zero, overflow, and above-range forms fail before configuration commit.

No renderer, camera, clipping, scene-mechanics, gameplay-asset, numeric-1,
ordinary-two-point, pass/defense/contact, free-throw, violation, restart, or
broad presentation behavior is added or altered.

## Corrective runner safety

Good-signed corrective commit
`a4c1286351add0450b1820cd79876e04aa3a08f9` changes only the focused runner,
with 17 insertions and 4 deletions. PowerShell AST parsing reports zero syntax
errors and `git diff --check` is clean.

The corrected ownership contract is:

1. create a GUID-suffixed per-invocation name under resolved `build`;
2. require the resolved selected path to begin with the resolved build prefix;
3. fail closed if that exact path already exists;
4. set `ScratchCreated` only after successful non-forced directory creation;
5. enter cleanup only when this invocation created the directory;
6. re-resolve and require exact equality, build containment, and directory type
   before literal recursive removal;
7. leave all parser, frame, state, manifest, hashing, two-pass, six-negative,
   proof, product, and classification semantics unchanged.

Pre/post inventories and Luna's independent check found no fixed
`build/gameplay_presentation_test` directory and no residual GUID scratch.

## Build and suite matrix

| Gate | Corrective-tip result |
| --- | --- |
| canonical `build.ps1` console + GUI | exit 0; warning/error clean |
| fresh VS CMake x64 configure | exit 0 |
| fresh VS CMake Release console + GUI | exit 0; warning/error clean |
| corrected focused presentation runner | exit 0; 17x2 + 6 negatives |
| full GameplayScene with build | exit 0; suites complete; proof remains DRAFT |
| direct scene / gameplay state / HUD | all exit 0 |
| close shot / dunk / jump / shot resolution | all exit 0 |
| camera / court / viewport / orientation | all exit 0 |
| violation-referee / penalty / gameplay assets | all exit 0 |
| asset pack / gameplay audio / frontend audio / music | all exit 0 |
| direct flow with exact root / NativeFlow / Win32 | all exit 0 |
| season / team data / team management | all exit 0 |
| controls / bank07 / video / current-main smokes | all exit 0 |

The asset-pack gate covered all 86 entries. Focused labels included TGCS,
TGDK (`BA611C75`), TGSR-3, TGCP-2, TGCT-1, TGOR-1, TGVR-1, TPNL-1, and TGPL-1.
Deterministic state replay remained `7A204A525C79D21C`.

## Focused corrective proof

Ignored proof root:
`build/gameplay-layup-proof-r2e-corrective-a4c1286`.

- schema: `tecmo.gameplay-presentation/TGPR-1`
- manifest SHA-256:
  `DCFF7AD9670D0C630785401F8E083F10019E458BD23001E3575D124EF0F60E4C`
- exact head/clean state: corrective SHA / `true`
- build requested/exit/warnings: `true` / `0` / `0`
- pass-one/pass-two: `17/17` native PNGs at `640x480`
- pass mismatch: `0`; distinct pass-one hashes: `17`
- mismatch against accepted proof: `0/17`
- mismatch against fresh d8 proof: `0/17`
- contact sheet: `2560x2400`, 17 ordered frames, SHA-256
  `85995E1654354BFFF874AEA8510F91FD6E0AB1BCECD49C5138EBC116FB9B6A6C`
- asset-pack SHA-256:
  `27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29`
- TGCS payload: `3144` bytes, FNV1A32 `DACDC976`
- TGCS variant-2 phases:
  `0,1,2,3,3,4,4,4,5,5,5,5,5,5,5,5`
- negative cases: `6/6` rejected transactionally; frame-18 sentinel unchanged

Frames 1-16 report `shot=layup`, `phase=live`, score `0/0`, clock `3:00`,
shot clock `24`, and violation `NONE`. Frame 17 reports `shot=none`, live play,
and production-settled score `2/0` with the same clocks/violation state.

The accepted proof manifest remains
`90EFB0AAAF55BB9853B8ED52A1E476701E388F389FD895922A4111E9BF1A91DD`;
the first fresh d8 integration proof remains
`AC6BCF9CAECE054A7E855ADC6BAB5A8FDEE73E743BE19F484686BDE295398645`.
All three proof generations share the exact layup contact-sheet hash.

## Full scene proof

Primary ignored proof root:
`build/gameplay-scene-proof-r2e-corrective-build-a4c1286`.

- schema: `tecmo.live-proof-manifest/TGLP-1`
- manifest SHA-256:
  `E490A5C330D88D7B33C17E76C8CC855D5220C6C4B8A3E476909B3B125F7249B1`
- status: `DRAFT`
- `require_pass=false`, `build_warning_clean=true`, `suites_complete=true`
- original reference: `PENDING_ORIGINAL_REFERENCE_MANIFEST`, no local runs
- artifact inventory: `254/254` present, byte/hash/containment failures `0`
- each contact sheet: `1920x1440`, 7 frames, SHA-256
  `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C`
- each native MP4: 7 stored/decoded frames at `640x480`, SHA-256
  `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595`
- decoded-frame-list SHA-256:
  `6FA0AA43130E1EFF92986485EC6305ABE9A86781968FEC24371FA45616F13E9B`
- cadence/time base: `39375000/655171` / `1/39375000`

The scene proof is deliberately not promoted. The external original-reference
baseline is pending and the runner's pass requirement is pinned outside this
slice. Native build/test determinism does not establish emulator parity.

## Original-resolution visual inspection

Sol inspected every corrective pass-one frame at original 640x480 resolution,
the 2560x2400 layup sheet, both fresh scene sheets, and relevant accepted
historical jump-make, jump-miss, rim-rattle, dunk, and rules-presentation
sheets/media.

The layup sequence shows stable court, hoop, crowd, camera, and HUD; coherent
actor/ball changes and source phase plateaus; and a clean terminal return to
live court with score 2-0. No corrupt sprite, tear, bad crop, HUD overlap, or
unexplained clipping was observed. The scene sheet likewise shows no obvious
presentation corruption.

Historical media is contextual only. Different rosters, camera states, and
action composition do not convert the narrow native fixture into original
frame parity or wider gameplay coverage.

## Evidence classes and disposition

- **Exact-source-pinned:** TGCS variant-2 payload identity, step count, and
  phase schedule.
- **Native-faithful:** production scene/input selection and existing
  camera/HUD/render composition inside the named deterministic fixture.
- **Native-approximate:** stable CLI geometry, host-native PNG rendering, ball
  arc/outcome/claimant/landing behavior already classified by the product task.
- **Incomplete/unproven:** original trigger policy, complete layup object
  semantics, general directions/profiles, contact meaning, numeric-1,
  ordinary two-point, both-edge action proof, original OAM ordering, broad
  parity, and performance.

Corrective-tip disposition: product `P0=0/P1=0/P2=0/P3=0`; QA tooling and
integration `P0=0/P1=0/P2=0/P3=0`. The isolated direct-flow limitation and
scene original-reference limitation are environment/evidence constraints, not
candidate defects.
