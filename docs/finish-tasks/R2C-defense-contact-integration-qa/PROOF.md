# R2C defense/contact integration proof

This is the detailed proof ledger supporting the **ACCEPT** decision in
`README.md`. Private ROM and local decompilation locations are represented as
`<LOCAL_REV1_ROM>` and `<DECOMP_ROOT>`; neither input nor its private path is
committed.

## Authority and identity

| Field | Exact value |
| --- | --- |
| Task | `R2C-DEFENSE-CONTACT-INTEGRATION-QA` |
| Session | `S-SOL-R2C-DEFENSE-CONTACT-INTEGRATION-QA-001` |
| Claim | `OWN-R2C-DEFENSE-CONTACT-INTEGRATION-QA` |
| Lane | `R2C / LANE-R2C-DEFENSE-CONTACT-INTEGRATION-QA` |
| Branch | `codex/r2c-defense-contact-integration-qa-sol` |
| Worktree | `C:/Users/joshs/Projects/tecmo-basketball-port-r2c-defense-contact-integration-qa-sol` |
| Signed reservation | `bd3160a6d52f05c89d07a2314b590366573aa10b` |
| Signed assignment control | `6f32bd8704f20422f4aa07c42b4ecaeb26a278b6` |
| Signed new-main authority | `2894a25c20532c642cc282408b69f28997b1166c` |
| Writable tracked scope | `docs/finish-tasks/R2C-defense-contact-integration-qa/**` |

The initial assignment authorized one deliberate Good-signed branch-only
non-fast-forward merge. When stable main later advanced, signed control
`2894a25...` authorized one additional Good-signed branch-only reconciliation
with the R2C lineage first and immutable accepted new main second. Candidate
and product files, normal build registries, main, `origin/main`, live remote
main, immutable staging, control files, push, and unrelated paths remained
read-only.

## Read-only takeover and merge proof

Before mutation, Sol read root `AGENTS.md` and `PORTING.md` completely and
proved all of the following:

- the exact assigned worktree was clean and attached uniquely to the exact
  assigned branch;
- branch HEAD, local main, `origin/main`, and live remote main were all
  `0ef11cf247e3110b6064e79a4c496be6346f3e13`;
- immutable staging `codex/round-2c-defense-contact-staging` resolved to
  `ed70d884c3c75d900df589f442816c9566eb38df`, tree
  `dc989c39a9dd5c5a52325f16a9bc0ee06c3a9416`;
- candidate base was `edf16ca9059158452798dbe5667f5e64ef444e39`;
- candidate lineage was strictly linear:
  `edf16ca... -> d5c5fa9b... -> 9d6f0227... -> ed70d884...`;
- all six unique checked lineage/merge commits returned exit `0` from
  `git verify-commit` with a Good SSH signature for `jaystar524@gmail.com`,
  RSA `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`;
- candidate delta was exactly eight accepted text paths; current-main delta
  from the common base was 55 paths; normalized overlap was zero;
- candidate and current-main diff checks both returned exit `0`;
- no candidate binary numstat, proprietary/binary payload extension, or
  ownership escape existed;
- read-only ordered `git merge-tree` predicted a conflict-free merge with
  current main first and the immutable candidate second.

Only after the master accepted that durable takeover did Sol create:

| Merge property | Exact result |
| --- | --- |
| Commit | `ec4c0958519f78d033f4b49936d07bdd30a2a400` |
| Tree | `960ccd3c7d3b5861a342caa783edcf0fd09a4c2f` |
| Parent 1 | `0ef11cf247e3110b6064e79a4c496be6346f3e13` |
| Parent 2 | `ed70d884c3c75d900df589f442816c9566eb38df` |
| Signature | Good SSH, registered identity/key above |
| Conflicts/resolution edits | none |

Both parents are ancestors of the merge. Its tree exactly equals the
read-only ordered merge-tree prediction. Candidate-path blobs are byte-for-byte
the immutable candidate blobs; all remaining paths are byte-for-byte current
main. The patch identity from either side is stable at
`1bd610926caf388e0915f73b18a311b46ceac91f`.

## New-main movement and second reconciliation

Before any second mutation, local main, `origin/main`, and live remote main
all resolved to accepted R2B terminal
`522909264f67673bd3242ecc62b343e1238bb142`, tree
`adb18fceb477644a0cb98d3155ea67f1f9a24482`. Old main `0ef11cf...` was the
exact merge base and an ancestor of both R2C and new-main lineages. Reservation
`bd3160a...`, assignment `6f32bd8...`, new authority `2894a25...`, both main
tips, initial merge `ec4c095...`, and every unique commit in
`0ef11cf...5229092` verified Good SSH for the registered identity/key.

The accepted main movement was exactly 23 text paths: nine R2/R2B reports,
four gameplay headers, nine gameplay/CLI sources, and one focused runner.
Its diff check returned exit `0`, every numstat was text, and it introduced no
proprietary payload. Normalized overlap with the eight R2C product paths was
zero. Normalized overlap with the two uncommitted reserved task-doc paths was
also zero. The prospective merged tree contained zero defense-module
registration references outside its isolated header/source/runner.

Read-only ordered merge-tree prediction with R2C `ec4c095...` first and new
main `5229092...` second returned exit `0`, no conflict, and tree
`7b8234fc3b45919ff59657d8dbb11c22f82a68dd`. Sol durably reported that clean
checkpoint before creating:

| Reconciliation property | Exact result |
| --- | --- |
| Commit | `32721888d9fd37b46c0ae2f529edf72c83844e88` |
| Tree | `7b8234fc3b45919ff59657d8dbb11c22f82a68dd` |
| Parent 1 | `ec4c0958519f78d033f4b49936d07bdd30a2a400` |
| Parent 2 | `522909264f67673bd3242ecc62b343e1238bb142` |
| Signature | Good SSH, registered identity/key above |
| Conflicts/resolution edits | none |

The actual tree exactly equals the prediction. Both parents and immutable
candidate `ed70d884...` are ancestors. Both parent-side diff checks return
exit `0`. No correction or rescope was required.

## Personal source and provenance review

Sol personally read every line of:

- the five accepted files below `docs/finish-tasks/R2-defense-contact/`;
- `include/tecmo_gameplay_defense_contact.h` (complete file);
- `src/tecmo_gameplay_defense_contact.c` (complete file);
- `tools/Run-GameplayDefenseContactTests.ps1` (complete file);
- `<DECOMP_ROOT>/decomp/lifted/bank06/C-0067_bank06_candidate_target_select_B081_B32E.asm`;
- the applicable B05 lifted source around `$9968-$999E` and `$9A24-$9A5F`.

The committed API and implementation retain raw labels and bounded data
contracts. The review confirmed:

- wrapped 16-bit absolute X delta and 8-bit absolute depth delta;
- metric `max + floor(min / 2)` with documented narrowing;
- high threshold initialized to `$07`, stale low threshold preserved;
- exactly one descending candidate scan from slot 9 through slot 0;
- self exclusion and candidate gate `($04B0 & $10) != 0`;
- strict improvement only, preserving stale fields and earlier descending
  winner on ties, with `$037F[$030B]` mirrored only on improvement;
- B05 coordinate-pair subtraction with independent X/depth borrow flags and
  windows;
- raw route byte validated as exactly `1`, wrapping `$0754` increment, raw
  plan byte `0x17`, documented mask/set operations, conditional low-nibble
  branch, and `$C042`/X=`$07` recorded as an external helper request rather
  than invoked;
- validation precedes commit, so invalid inputs and alias violations roll
  back without partial mutation.

## Independent ROM provenance

Canonical input identity and iNES structure:

| Property | Result |
| --- | --- |
| File size | `393232` bytes |
| SHA-256 | `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4` |
| Header | `4E 45 53 1A 08 20 42 00 00 00 00 00 00 00 00 00` |
| Trainer | absent |
| PRG/CHR | 8 PRG banks / 32 CHR banks |
| Expected total | `393232` bytes |

Mapping was recomputed with
`PrgStart = 0x10 + bank * 0x4000 + (CPU - $8000)`:

| Raw span | Offset | Bytes | FNV-1a 32 | SHA-256 |
| --- | ---: | ---: | --- | --- |
| B06 `$B081-$B108` | `0x1B091` | 136 | `87A88720` | `6BD687EABED16010B1DB4A0D81F532680DD3503F4757F3B5DCEA584A910CAF6D` |
| B06 `$B081-$B365` | `0x1B091` | 741 | `547FA51B` | `AAA9670DA5942FA2614F925A266674893A352BB2DB3A8F4158F61C8AE891AE36` |
| B05 `$9968-$999E` | `0x15978` | 55 | `FF699FE9` | `5F3742E3D833700C25811333B3B4B9FE737FFC2FB62414CDA9B1FCC206CEEBF3` |
| B05 `$9A24-$9A5F` | `0x15A34` | 60 | `953B37A4` | `1751F2A4AAC9A23A385BF172BC419260D7EFB20650B360FDB604DF67A7A5A66B` |

B06 boundary `$B103-$B109` is
`60 AC 0B 03 B9 0C 03`; B05 `$999E` is `AD`. The `$9A24` span contains
`A9 17 8D 78 04 8D 28 05`, independently confirming the raw `0x17` plan.

## Initial combined command and result ledger

All commands ran from the assigned worktree unless an external caller is
stated. `<LOCAL_REV1_ROM>` and `<DECOMP_ROOT>` are read-only local inputs.

| Gate / command shape | Result |
| --- | --- |
| `$env:TECMO_SKIP_SHORTCUT = '1'; .\build.ps1` | PASS; full MSVC build, both normal executables, zero warning/error diagnostics; defense/contact source intentionally absent from normal compilation |
| `powershell -NoProfile -ExecutionPolicy Bypass -File ./tools/Run-GameplayDefenseContactTests.ps1 -RomPath <LOCAL_REV1_ROM>` | PASS from repository root; isolated `/std:c11 /W4 /WX`, direct/enclosing fingerprints, B081 scan, B05 matrices/raw plan, rollback, repeatability, dependency-boundary hashes |
| absolute focused runner with `-ProjectRoot <ASSIGNED_WORKTREE> -RomPath <LOCAL_REV1_ROM>` from the independent projectless caller directory | PASS with identical focused output |
| `./tools/Run-GameplayPreTipTests.ps1` | PASS |
| `./tools/Run-GameplayShotResolutionTests.ps1` | PASS |
| `./tools/Run-GameplaySceneTests.ps1` | PASS |
| first `./tools/Run-NativeFlowTests.ps1 -DecompRoot <DECOMP_ROOT>` | stopped honestly at the required ignored `build/tecmo.assetpack` precondition; no product assertion failed |
| `./build/tecmo_port.exe --build-assetpack <LOCAL_REV1_ROM> ./build/tecmo.assetpack` | PASS; supported canonical pack build, 8 PRG banks, 32 CHR banks, 86 entries |
| repeated `./tools/Run-NativeFlowTests.ps1 -DecompRoot <DECOMP_ROOT>` | PASS; complete CLI boundaries and native flow |
| `./tools/Run-Win32LaunchSmokeTest.ps1 -DecompRoot <DECOMP_ROOT>` | PASS |
| `./tools/Run-AssetPackTests.ps1 -RomPath <LOCAL_REV1_ROM>` | PASS; complete asset-pack suite |
| `./tools/Run-SeasonTests.ps1 -SkipBuild -RomPath <LOCAL_REV1_ROM> -DecompRoot <DECOMP_ROOT>` | PASS |
| `./tools/Run-GameplayAudioTests.ps1` | PASS |
| `./tools/Run-FrontendAudioTests.ps1` | PASS |
| `./tools/Run-MusicTests.ps1` | PASS |
| `tecmo_port.exe --bank07-test`, `--controls-test`, `--assetpack-test`, `--music-test`, `--frontend-audio-test`, `--gameplay-audio-test`, `--gameplay-state-test`, `--team-management-test`, `--season-test` | all PASS, exit `0` |

The first NativeFlow stop was an environmental setup precondition: the
ignored pack did not yet exist. The supported CLI rebuilt that pack, and the
same unmodified source and runner then completed successfully. It is neither
hidden nor classified as a product correction.

## Reconciled combined gate ledger

After signed reconciliation `32721888...`, Sol rebuilt and reran every gate
affected by the R2B shots/scene movement plus the bounded cross-domain seam:

| Reconciled gate / command shape | Result |
| --- | --- |
| `$env:TECMO_SKIP_SHORTCUT = '1'; .\build.ps1` | PASS; full warning-clean MSVC build, both normal executables; build output again omitted defense/contact source |
| focused defense runner from repository root | PASS; isolated `/std:c11 /W4 /WX`, exact provenance/oracles/rollback/repeatability/boundary hashes |
| focused defense runner from the independent external caller with explicit `-ProjectRoot` | PASS; identical output and cleanup |
| `Run-GameplayCloseShotTests.ps1` | PASS; TGCS canonical/provenance/reload, 208 poses, negative cases, 43 mutations |
| `Run-GameplayShotResolutionTests.ps1` | PASS; TGSR point/polarity/raw routes/rattle/claimant settlement |
| `Run-GameplayPreTipTests.ps1` | PASS; TPTI provenance, dependency/mutation, input, abort/freeze, toss/jump/live-render matrix |
| `Run-GameplaySceneTests.ps1` | PASS; complete gameplay-scene matrix and deterministic proof generation |
| `tecmo_port.exe --gameplay-state-test` | PASS; replay `7A204A525C79D21C` |
| `Run-GameplayFreeThrowLineupTests.ps1` | PASS; TGFL exact provenance/orientations/negative cases |
| `Run-GameplayFatigueTests.ps1` | PASS; TGFT |
| `Run-GameplayCpuSteeringTests.ps1` | PASS; TGAI provenance/mutations/transactional live adapter |
| `Run-AssetPackTests.ps1` | PASS; all 55 checks, 8 PRG, 32 CHR, 86 entries |
| supported canonical `--build-assetpack`, then direct `--assetpack-test` | PASS; rebuilt ignored 86-entry pack and direct self-test |
| direct `--flow-test`, `Run-NativeFlowTests.ps1`, and `Run-Win32LaunchSmokeTest.ps1` | PASS; flow/CLI boundaries and GUI launch/exit |
| direct `--gameplay-scene-test` against the rebuilt canonical pack | PASS |
| `Run-MusicTests.ps1`, `Run-FrontendAudioTests.ps1`, `Run-GameplayAudioTests.ps1` | PASS |
| `Run-SeasonTests.ps1 -SkipBuild` | PASS; provenance/save isolation/native handoff and pixel checkpoints |

Unlike the recorded initial setup precondition, the reconciled run rebuilt the
canonical ignored pack before flow gates; NativeFlow passed on its first
reconciled invocation. No tracked source, runner, or product correction was
made.

## Isolation, registration, and cleanup

Registration searches returned zero references to the new module in
`CMakeLists.txt`, existing normal `src/*.c` translation units, CLI/main,
gameplay-scene, or asset-pack registration. `CMakeLists.txt` does not list
`src/tecmo_gameplay_defense_contact.c`. Initial main-to-initial-merge tracked
changes were exactly the eight candidate paths. The later 23-path new-main
movement was disjoint and the reconciled tree retained the same zero-hit
registration result.

Both focused calls compiled separate objects in system-temporary scratch,
checked exact harness output, removed scratch, and found no newly created
repository-root artifacts. Final scans found no root `.obj`, `.exe`, `.pdb`,
`.ilk`, `.idb`, `.res`, `.lib`, `.exp`, `.sbr`, `.bsc`, `.manifest`, or
`.tlog` artifact and no remaining `tecmo-gameplay-defense-contact-*` scratch
directory. Read-only CPU/LIVE/TIP/TPNL/TGSR dependency boundary hashes were
unchanged. Generated normal-build and asset-pack material remains ignored
below `build/`. The same checks after reconciliation again found zero root
compiler artifacts and zero focused scratch directories.

## Independent read-only QA

After the collision/registry gate, Sol created and pinned exactly one
top-level projectless reviewer:

| Field | Exact value |
| --- | --- |
| Task | `019fcaa4-615e-7a41-b919-f001132bdcb9` |
| Model | `gpt-5.6-luna` |
| Thinking | `max` |
| Initial target | merge `ec4c0958519f78d033f4b49936d07bdd30a2a400`, tree `960ccd3c7d3b5861a342caa783edcf0fd09a4c2f` |
| Reconciled target | merge `32721888d9fd37b46c0ae2f529edf72c83844e88`, tree `7b8234fc3b45919ff59657d8dbb11c22f82a68dd` |
| Mode | independent, projectless, read-only |
| Verdict | **PASS — zero findings** |
| Severities | P0 `0`, P1 `0`, P2 `0`, P3 `0` |

The Luna independently verified identity, refs, signatures, ancestry,
ordered merge-tree equality, exact scope and zero overlap, no proprietary
artifacts, no registration, complete source/header/runner/docs content,
source/ROM provenance, raw C semantics, both focused caller contexts, scratch
cleanup, and final clean status. It explicitly did not substitute its focused
review for normal runtime, emulator, visual, audio, or semantic completion;
those remain either Sol regression evidence or out of scope.

The same sole Luna lineage is reused for the terminal docs/tip read-only
verification. No second Luna is authorized or used.

Its first documentation audit returned P0 `0`, P1 `0`, P2 `0`, P3 `1`: the
original full-build ledger row used POSIX-style environment assignment in a
PowerShell repository. That documentation-only issue did not invalidate the
historical build; the row now uses the exact PowerShell environment assignment
and invocation. The Sol handoff records the same Luna's corrected-draft and
exact reconciled signed-tip confirmation.

## Honest limitations

- This accepts a raw/neutral standalone foundation and its focused proof, not
  normal runtime integration.
- It makes no player-facing or semantic steal, block, rebound, recovery, foul,
  possession, scoring, or completed-contact outcome claim.
- It makes no cycle-exact, emulator, or visual parity claim.
- The normal build and broad smokes prove no regression on registered current
  main; because the module is deliberately unregistered, those gates do not
  prove runtime use of the module.
- Any runtime registration, semantic naming, external-helper completion, or
  scene/game-flow behavior requires a separate master rescope.

## Final guard and handoff

At this record's pre-commit guard, local main, `origin/main`, and live remote
main were each `522909264f67673bd3242ecc62b343e1238bb142`. That commit and
immutable candidate `ed70d884...` are ancestors of signed reconciliation
`32721888...`. Status before documentation contained exactly the two reserved
untracked documentation paths and no other worktree change; only those two
paths are introduced by the terminal docs commit.

This task does not mutate main or staging and does not push. The Sol handoff
must report the self-referentially unavailable docs commit ID/tree, its Good
signature, clean status, exact changed-path ownership, final main observation,
and the sole Luna's terminal-tip verdict. Master alone may perform the guarded
fast-forward and ordinary non-force push.
