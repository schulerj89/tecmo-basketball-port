# R2A evidence, proof, and classifications

## Source and behavior classification

This integration task preserves the accepted R2 classifications:

| Surface | Classification | Accepted source boundary | Terminal integration result |
| --- | --- | --- | --- |
| Clock decrement, expiry, fixed wait, period/final flow, and A-release dismissal | `native_faithful` | Fixed-bank `$E58D-$E898` and `$E7D0-$E822`; Bank06 `$A05A-$A24F`/`$BC3C-$BCF9`; Bank03 `$EA14-$EA2F` | Gameplay-state transaction, alias, regulation, OT, and dismissal vectors pass with replay `7A204A525C79D21C`. |
| Possession reset | `native_faithful` | Fixed-bank `$E6ED/$E6FF` and `$E765-$E76F` | Reset remains LIVE-only and rejects every tested non-LIVE phase unchanged. |
| TGFT evolution | `exact_source_pinned` plus `native_faithful` native boundary | Bank02 `$B4E6-$B5C7` and fixed caller `$ED2F-$ED3E` | Canonical descriptor/pointer/hash/dependency checks, cadence `6/4/1`, countdown wrap, recovery, active/bench, staged replacement, and bounded destructor vectors pass. |
| TGFT live fixed-slot coupling | `native_approximation_with_justification` | Existing caller-owned five-slot actor bridge | Integration scene passes; no dynamic replacement policy is inferred. |
| TGFL raw spans and base projection | `exact_source_pinned` | Bank06 `$88B0-$88D9`, `$9621-$976E`, `$976F-$985C`, `$985D-$9918` | Both orientations, all base placements, exact source map, pose indexes, SHA/FNV, staged replacement, alias, and destructor vectors pass. |
| TGFL bounded caller predicate tail | `exact_source_pinned` | Two contiguous predicate/effect tails inside `$976F-$985C` | Four control combinations over every base placement and explicit nonzero predicate pass. No selection/aim/release/ownership semantics are added. |
| Dynamic substitutions and active-lineup replacement | `incomplete` | No exact live caller, eligibility, timing, or scene owner is proven | Remains deferred; this QA lane makes no completeness claim. |
| Shot-clock/referee and free-throw proof frames | bounded `native_faithful` presentation evidence | Accepted render modes and current scene composition | Deterministic and visually clean, without upgrading scene ownership. |

No classification was silently upgraded by the merge or by the test-runner
correction.

## Canonical ignored evidence

| Artifact | Result | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Terminal combined asset pack | 86 entries | 1,401,618 | `CC9A522A1EC5025193FD525419096D5A5AA15AF2F63A9E18E68DB8D81E87AC6F` |
| Terminal broad asset-pack report | 55 pass, 0 fail | 100,164 | `E4D049792936B91737ECB6155BDA0434BBBB902ABDFDE71A7E1443AF8C245363` |
| NativeFlow terminal report | flow plus all CLI boundaries pass | 897 | `932334D5AAB9CBA662A63218EB160C1B0A46C637D41B3090744595A362CD96B9` |
| IntroSequence terminal report | 29 rows, one explicit optional-reference skip, zero failures | 272,406 | `2FE292D0F508A1DD015F8CAEE0A8265737C12DAE3DC6D27647273D97A665628B` |
| Terminal GameplayScene manifest | `DRAFT` integration smoke | 567,666 | `01E05F49B2D2D72B9F0765E1A066E75F79990E83790A210D938D963937AA541B` |
| Accepted R2 v2 proof manifest | 97 artifact records | 33,453 | `1FA074FB90D87AF48A3FB78DB50E8B96A78C7F653EC9EFA76BF581B8FC0F51C3` |
| Accepted shot-clock MP4 | 81 native-rate frames | 15,769 | `AD682F67F0EF43C2BDD08D1FE80E4F2146A83E211FEA9B2459AAB9E005683FFE` |
| Accepted decoded frame-MD5 record | 81 rows | 6,706 | `F7AA9B69C4ADEE3B2D954E38816AF9930A271D7635BF5C1C4FC7702F9B40FD5B` |

These files are ignored dynamic evidence. The tracked reports contain hashes,
classification, and reproduction commands, not private paths or payloads.

## Build and focused gates

| Gate | Terminal result |
| --- | --- |
| Full MSVC C11 `/W4` build | pass, zero warnings, console and GUI executables produced |
| Gameplay-state self-test | pass, replay `7A204A525C79D21C` |
| Asset-pack self-test | pass |
| TGFL focused | pass, exact Rev1 iNES/FNV/SHA, two orientations, strict map and rejection, 12 source mutations |
| TGFT focused | pass |
| TGCP focused | pass, camera follow/settle/projection and TGFL slot-3 composition, 21 source mutations |
| TGOR focused | pass, two-basket ownership and 12 bounded source mutations |
| TPNL focused | pass, pure M01/M06/M07 rules and 24 source mutations |
| TGVR focused | pass, shot-clock and live out-of-bounds/backcourt referee sequences |
| TGCT focused | pass, full-world decode, viewport slicing, fine scroll, transactional failure |
| TGBC focused | pass, both orientations, hysteresis, possession-owned trigger |
| TGAI focused | pass, 680 aligned commands, 24 handlers, 17 mutations |
| TGMO focused | pass, movement vectors, transactionality, 7 mutations |

The focused negative surface covers missing, malformed, undersized, oversized,
wrong-revision, dependency-corrupt, cross-pack, pointer/object overlap,
duplicate/out-of-range active lists, impossible cadence, output alias, failed
replacement rollback, and bounded corrupt-object destruction. No failed parse
or update commits partial caller-visible state.

## Broad asset-pack P2 and closure

Before correction, the broad checker alone failed `assetpack-finale-native`
because it expected the pre-R4 finale schema. The accepted importer, runtime,
focused IntroSequence gate, and R4 report agree on:

- `TFM1` inside TFIN-1 bytes 116..180;
- reserved bytes 181..191;
- reverse palette frames `8/12/16/20/25`;
- title write frames `344`;
- title bands `0..144/ch0`, `144..152/ch1`, `152..240/ch0`.

The master granted only the broad runner in signed control commit
`360c7806bc9c1b052f9bb249cb62d08348fb1916`. Signed remediation
`73e87dcc...` aligned those assertions and added separate semantic-header and
reserved-tail corruptions. The terminal report has:

- `55` tests;
- `55` pass;
- `0` failure;
- `assetpack-finale-native` with an empty issue list;
- `15/15` finale malformed subcases rejected;
- no mutated or raw asset payload persisted.

The correction changes no importer, runtime, asset payload, renderer, build
registry, or candidate path.

## Combined scene and current-main regression gates

GameplayScene passes full-pack provenance; HUD; human and CPU movement;
ball/bounce/audio state; fatigue; free throws; penalties; out-of-bounds and
backcourt settlement; violation referee; camera/projection; court orientation;
jump/make/miss/rattle; dunk cutaway; halftime/final; deterministic rendering;
and missing/malformed/oversized/dependency/CHR failures.

The terminal scene manifest records:

- branch `codex/r2a-clocks-integration-qa-sol`;
- current SHA `73e87dcccbfe1ddc6a78d9b313e8dd75252fb857`;
- status `DRAFT` and `require_pass=false`;
- `clean=true` and `suites_complete=true`;
- 254 inventory rows and exactly 255 files including the manifest;
- 189 required logs;
- 14 stored frames over two repeats;
- two byte-identical 7-frame MP4s;
- byte-identical 1920x1440 contact sheets;
- native rate `39375000/655171`, time base `1/39375000`, resolution `640x480`.

Every inventory path/byte/SHA and every required log SHA was independently
rechecked by Sol. Fresh ffprobe and framemd5 runs found 7/7 frames in each
video; both MP4s have SHA-256
`B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595`.

The following current-main gates also pass:

- NativeFlow normal flow and every CLI boundary;
- IntroSequence TFIN semantic round trip, pixel checkpoints, determinism, and
  malformed inputs;
- season/TSAV and native gameplay handoff;
- team data and team management;
- TMUS music, TFSX frontend audio, TSFX/TDMC gameplay audio;
- Win32 PE subsystem, shortcut, explicit developer flow, window lifetime, and
  clean shutdown.

Audio is regression evidence only; the R2 boundary changed no audio semantic.

## Accepted proof and merged-tip determinism

The accepted v2 proof manifest SHA-256 is
`1FA074FB90D87AF48A3FB78DB50E8B96A78C7F653EC9EFA76BF581B8FC0F51C3`.
Sol revalidated:

- declared artifact count `97`;
- actual files excluding manifest `97`;
- total files including manifest `98`;
- missing `0`, size mismatch `0`, hash mismatch `0`, extras `0`;
- 81 shot-clock frames;
- two distinct free-throw orientation frames;
- 640x480 native video, exact native cadence and time base;
- 81 fresh decoded frame-MD5 rows exactly equal the stored record.

From terminal signed tip `73e87dcc...`, the combined pack rendered all 81
shot-clock PNGs and both free-throw PNGs. All `83/83` SHA-256 values match the
accepted proof:

| Frame | SHA-256 |
| --- | --- |
| Shot-clock frame 0 | `2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A` |
| Shot-clock frame 80 | `68718769B999B4B3359997690D82FD3A146FDCDC75DA58991F775AF93BBAFD91` |
| Free throw, left basket | `1A51687E4F98A4CAA79D20A21DF6BD4DB395E3127A4BF257FEDBF75A3373C8FA` |
| Free throw, right basket | `F47ADE68F027309A74744376D0DE0B2CBA180314D21F752BC27F93839F39815A` |

## Sol full-resolution visual review

The Sol used the repository screenshot-QA checklist and opened at original
detail:

- the 81-frame numbered shot-clock sheet;
- shot-clock keyframes 0, 9, 23, 27, and 80;
- the labeled free-throw orientation sheet;
- both 640x480 free-throw source frames;
- the terminal GameplayScene contact and all seven 640x480 state frames.

Observed shot-clock sequence: intentional black frame 0; referee entrance by
frame 9; distinct pointing progression at frame 23; settled raised-hand pose at
frame 27; stable hold through frame 80. The text remains readable and centered.

Observed free throws: left and right views remain distinct court orientations;
shooter, ball, lane actors, bench/crowd, basket, court markings, score, clock,
names, and remaining-player HUD stay readable and coherently mirrored.

Observed scene smoke: no collision-box edge, wall/court gap, broken texture,
missing principal actor, corrupt palette, stale debug overlay, HUD overlap,
unintended crop, or transition discontinuity. No visual severity finding was
opened.

## Ownership and proprietary scan

The pre-documentation scan record SHA-256 is
`8AEDDD99894B5293DD075607DF1F35969131F281C5CE9BD9D69AF13B8E964A8B`.
It records:

- current-main delta from common base: `56` paths;
- immutable candidate: `18` paths;
- intersection: `0`;
- merge first-parent delta: exactly the candidate's `18` paths;
- runner remediation: exactly `tools/Run-AssetPackTests.ps1`;
- terminal pre-doc delta from current main: `19` paths;
- ownership escapes: `0`;
- prohibited ROM/CHR/save/trace/image/audio/video/pack/archive/executable paths:
  `0`;
- binary numstat rows: `0`;
- tracked status rows: `0`.

Generated ROM-derived and media evidence remains ignored below `build/`. The
tracked delta contains only native C/header/test/report text. The one runner
rescope is durably authorized by signed master control
`360c7806bc9c1b052f9bb249cb62d08348fb1916`.

## Honest deferred limits

- Dynamic substitution caller, eligibility, timing, selection, and
  active-lineup replacement remain incomplete.
- Stable fixed actor slots are not reclassified as exact dynamic lineup
  ownership.
- TGFL predicate effects are not evidence of free-throw aim, release, attempt,
  control, pose selection, or scene ownership.
- The clock port is semantic/native-faithful, not cycle exact.
- The proof video is silent because no audio semantic changed.
- GameplayScene evidence is a `DRAFT` integration smoke, not a new formal R1
  proof.
- No accepted R2 approximation or residual is upgraded by this QA task.
