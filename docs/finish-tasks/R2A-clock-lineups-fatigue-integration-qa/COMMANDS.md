# R2A commands and results

Private local inputs are represented by placeholders:

```powershell
$ProjectRoot = '<R2A_WORKTREE>'
$Rom = '<CANONICAL_REV1_ROM>'
$DecompRoot = '<LOCAL_DECOMP_ROOT>'
$env:TECMO_SKIP_SHORTCUT = '1'
```

The canonical Rev1 ROM was 393,232 bytes with SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
It was test/research input only.

## Pre-mutation audit and merge

Representative exact checks:

```powershell
git status --porcelain=v2 --untracked-files=all
git branch --show-current
git rev-parse HEAD main origin/main
git ls-remote origin refs/heads/main
git rev-parse codex/round-2a-clock-lineups-fatigue-staging
git merge-base edf16ca9059158452798dbe5667f5e64ef444e39 `
  ed4e56fc595894c692ffca84ae3b35f129317049
git diff --name-only 222d75cfafa9153db1eb44492bf557f11b1a9091..edf16ca
git diff --name-only 222d75cfafa9153db1eb44492bf557f11b1a9091..ed4e56fc
git merge-tree --write-tree --messages edf16ca9059158452798dbe5667f5e64ef444e39 `
  ed4e56fc595894c692ffca84ae3b35f129317049
git diff --check 222d75cfafa9153db1eb44492bf557f11b1a9091..ed4e56fc
```

Results: exact clean assigned branch at `edf16ca...`; local main,
`origin/main`, and live main all equal `edf16ca...`; immutable staging exact
`ed4e56fc...`; merge-base `222d75cf...`; current-main delta `56` paths;
candidate delta `18` paths; overlap `0`; clean predicted merge tree
`59e81bee9f8e94057a584dbfd7e45053a6d4f8c2`.

All seven candidate commits were individually checked:

```powershell
git verify-commit 6c87dbed170c8ca2ba68e29671f7cfebf5adb60a
git verify-commit 540ae0ba47ef44d6096781ffd0c276012e683221
git verify-commit 97277cbecf685a9f8ac8e29dde1a6de61f0e2db8
git verify-commit 1536ae31e7016f6e9adbddb7868e2d40e51c1085
git verify-commit bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be
git verify-commit 1567f284ff48a2334fb6a9bd82d00aadf0cdb373
git verify-commit ed4e56fc595894c692ffca84ae3b35f129317049
```

Each returned a Good SSH signature. The only authorized branch merge was:

```powershell
git merge --no-ff -S ed4e56fc595894c692ffca84ae3b35f129317049 `
  -m 'Integrate R2 clocks lineups fatigue with current main'
git verify-commit 8233cb4b7c86612cd290615927439caf83947b1e
```

The merge has ordered parents `edf16ca...` then `ed4e56fc...` and the exact
predicted tree.

## Initial combined QA

The initial warning-clean build and domain gates were:

```powershell
.\build.ps1
.\build\tecmo_port.exe --gameplay-state-test
.\build\tecmo_port.exe --assetpack-test
.\tools\Run-GameplayFreeThrowLineupTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayFatigueTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayCameraProjectionTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayCourtOrientationTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayPenaltyTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayViolationRefereeTests.ps1 -ProjectRoot . -RomPath $Rom
```

All passed. Gameplay-state replay was `7A204A525C79D21C`. TGFL rejected 12
source mutations; TGCP rejected 21; TPNL rejected 24. TGFT, rollback, alias,
staged replacement, corrupt-object destructor, and fail-closed vectors passed.

## P2 detection and runner-only correction

The first broad asset-pack run had one failure among 55 tests:
`assetpack-finale-native`. The issues were `header-reserved`,
`reverse-palette-frames`, `title-metadata`, and `band-0/1/2`. Focused
`Run-IntroSequenceTests.ps1` simultaneously passed TFIN semantic round trips,
native pixel checkpoints, determinism, and malformed cases. Source inspection
showed that the broad checker retained the pre-R4 oracle.

After the master collision-checked and granted only
`tools/Run-AssetPackTests.ps1`, the checker was aligned to:

- `TFM1` semantic bytes 116..180 and reserved bytes 181..191;
- reverse frames `8/12/16/20/25`;
- title write duration `344`;
- bands `0..144/ch0`, `144..152/ch1`, and `152..240/ch0`;
- distinct semantic-header and reserved-tail negative mutations.

The durable Good-signed control record for this grant and its collision check
is `360c7806bc9c1b052f9bb249cb62d08348fb1916`. The staged set was checked as
exactly one file, then committed:

```powershell
git diff --check
git add -- tools/Run-AssetPackTests.ps1
git diff --cached --check
git diff --cached --name-only
git commit -S -m 'test: align broad finale asset contract'
git verify-commit 73e87dcccbfe1ddc6a78d9b313e8dd75252fb857
```

The signed commit changes one file only, with 32 insertions and 7 deletions.

## Terminal broad, focused, and cross-domain gates

The complete terminal gate ran from signed tip `73e87dcc...`:

```powershell
.\build.ps1
.\build\tecmo_port.exe --gameplay-state-test
.\build\tecmo_port.exe --assetpack-test
.\tools\Run-AssetPackTests.ps1 -ProjectRoot . -RomPath $Rom `
  -AssetPackPath 'build\r2a-integration-qa\tecmo.assetpack' `
  -ReportPath 'build\r2a-integration-qa\asset-pack-report.json'
.\tools\Run-IntroSequenceTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayFreeThrowLineupTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayFatigueTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayCameraProjectionTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayCourtOrientationTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayPenaltyTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayViolationRefereeTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayCpuSteeringTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayMovementTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayCourtTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayBackcourtTests.ps1 -ProjectRoot . -RomPath $Rom
```

All passed. The build emitted zero warnings. The broad report contains
`55/55` passing tests and all 15 finale malformed subcases reject.

With the validated 86-entry pack configured:

```powershell
$env:TECMO_ASSETPACK =
  (Resolve-Path 'build\r2a-integration-qa\tecmo.assetpack').Path
.\tools\Run-NativeFlowTests.ps1 -ProjectRoot . -DecompRoot $DecompRoot `
  -ReportPath 'build\r2a-integration-qa\native-flow-report-terminal.json'
.\tools\Run-MusicTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-FrontendAudioTests.ps1 -ProjectRoot . -RomPath $Rom `
  -DecompRoot $DecompRoot
.\tools\Run-GameplayAudioTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-SeasonTests.ps1 -ProjectRoot . -RomPath $Rom `
  -DecompRoot $DecompRoot -SkipBuild
.\tools\Run-TeamDataTests.ps1 -ProjectRoot . -RomPath $Rom `
  -DecompRoot $DecompRoot -SkipBuild
.\tools\Run-TeamManagementTests.ps1 -ProjectRoot . -RomPath $Rom `
  -DecompRoot $DecompRoot -SkipBuild
.\tools\Run-Win32LaunchSmokeTest.ps1 -ProjectRoot . -DecompRoot $DecompRoot
```

Every flow, CLI-boundary, audio, season/team, PE/shortcut, GUI-lifetime, and
clean-shutdown check passed.

The terminal scene integration command was:

```powershell
.\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath $Rom `
  -ProofRootPath 'build\r2a-integration-qa\gameplay-scene-proof-terminal-73e87dc'
```

It returned `GAMEPLAY SCENE TEST PASS`. The proof status remains `DRAFT`,
which is the correct integration-smoke classification.

## Proof integrity and deterministic rerender

PowerShell path containment, byte counts, and SHA-256 revalidated all 97
accepted R2 artifact records and proved exactly 98 total files including the
manifest. Fresh `ffprobe -count_frames` and `ffmpeg -f framemd5 -` reproduced
all 81 stored decoded rows.

The merged executable rendered:

```powershell
0..80 | ForEach-Object {
  .\build\tecmo_port.exe --root $ProjectRoot --render-test-mode `
    ('gameplay-shot-clock-violation-frame' + $_) <FRAME_PNG>
}
.\build\tecmo_port.exe --root $ProjectRoot --render-test-mode `
  gameplay-free-throw-left <LEFT_PNG>
.\build\tecmo_port.exe --root $ProjectRoot --render-test-mode `
  gameplay-free-throw-right <RIGHT_PNG>
```

All 83 terminal PNG hashes match the accepted v2 proof. Sol opened the two
contact sheets and the seven key full-resolution images at original detail.

## Harness and retry record

These were orchestration diagnostics, not accepted product failures:

1. Direct gameplay-audio and the first Win32 probe were attempted without the
   strict combined pack; both passed when rerun with the validated pack.
2. The first merged proof comparison used an incorrect local free-throw
   directory name after all 83 renders had succeeded; the corrected comparison
   found zero mismatch.
3. One ownership-scan one-liner used a newer .NET `SHA256.HashData` API not
   available in Windows PowerShell; the `SHA256.Create().ComputeHash` retry
   passed.
4. The initial broad asset-pack failure was the P2 that led to the authorized
   signed correction; the terminal rerun is 55/55.
5. The Luna's first two focused-script invocations did not bind its quoted ROM
   argument and did not reach the tests. Its corrected invocations passed.
6. The first `COMMANDS.md` patch request had an unescaped JavaScript template
   delimiter and failed before changing the filesystem; the encoded retry
   succeeded.
7. There was no bad-request fault, task creation retry, replacement worker,
   Git mutation retry, merge conflict, or signature failure.

## Final docs verification

```powershell
git diff --check
git status --short --branch
git add -- docs/finish-tasks/R2A-clock-lineups-fatigue-integration-qa
git diff --cached --check
git diff --cached --name-only
git commit -S -m 'docs: accept R2A clock lineup fatigue integration QA'
git verify-commit <SIGNED_TERMINAL_DOCS_SHA>
git merge-base --is-ancestor 73e87dcccbfe1ddc6a78d9b313e8dd75252fb857 `
  <SIGNED_TERMINAL_DOCS_SHA>
git status --porcelain=v2 --untracked-files=all
git rev-parse main origin/main
git ls-remote origin refs/heads/main
```

The exact terminal docs SHA, signature result, clean status, main observations,
and worker unpin result are supplied in the Sol-to-master handoff.
