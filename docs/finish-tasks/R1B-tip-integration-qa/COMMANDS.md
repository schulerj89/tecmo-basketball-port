# Personal commands and results

## Fixed inputs

```powershell
$ProjectRoot = 'C:/Users/joshs/Projects/tecmo-basketball-port-r1b-tip-integration-qa-sol'
$Rom = 'C:/Users/joshs/Projects/disassem/Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
$DecompRoot = 'C:/Users/joshs/Projects/disassem/tecmo-basketball-decompilation'
$InitialProofRoot = 'build/proof/r1b-tip-integration-564d83835258'
$ProofRoot = 'build/proof/r1b-tip-integration-3aa7dfb523d6'
$Ffmpeg = 'C:/Users/joshs/AppData/Local/Microsoft/WinGet/Packages/Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe/ffmpeg-8.1-full_build/bin/ffmpeg.exe'
Set-Location -LiteralPath $ProjectRoot
```

The canonical ROM is `393232` bytes with SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
The ROM, decompilation, mapper-gated observations, and original screenshots
were used read-only and are not committed.

## Takeover, collision, and candidate audit

```powershell
git branch --show-current
git rev-parse HEAD main origin/main codex/round-1b-tip-fidelity-staging
git worktree list --porcelain
git status --porcelain=v2 --untracked-files=all
git merge-base edf16ca9059158452798dbe5667f5e64ef444e39 e21f9a6621df5527544be1de4d0dc60382539c60
git log --format='%H %P %G? %GS %GK %s' --reverse 222d75cfafa9153db1eb44492bf557f11b1a9091..e21f9a6621df5527544be1de4d0dc60382539c60
git verify-commit a37e10207455933be3930e90c55b10b669cb0ef3
git verify-commit b678beffeacd745fe438e78d323357dc6f86af95
git verify-commit 1b1bf23b3c48947d988c9231870f9827f88cc5a6
git verify-commit e21f9a6621df5527544be1de4d0dc60382539c60
git diff --name-only 222d75cfafa9153db1eb44492bf557f11b1a9091..edf16ca9059158452798dbe5667f5e64ef444e39
git diff --name-only 222d75cfafa9153db1eb44492bf557f11b1a9091..e21f9a6621df5527544be1de4d0dc60382539c60
git merge-tree --write-tree edf16ca9059158452798dbe5667f5e64ef444e39 e21f9a6621df5527544be1de4d0dc60382539c60
```

Results: assigned branch/worktree/base/ref registry exact and clean; all four
candidate commits Good SSH; merge-base `222d75cf...`; current-main side 56
paths, candidate side 24, normalized overlap `0`; precomputed tree
`a08a66bb9edc858f7b87e72bff160c5cd8310186`; every candidate path is within
the accepted R1 TIP slice.

## Signed branch-only merge

```powershell
git merge --no-ff -S e21f9a6621df5527544be1de4d0dc60382539c60 -m 'Merge accepted R1 TIP fidelity into R1B integration QA'
git show -s --format='%H%n%P%n%T%n%G?%n%GS%n%GK' HEAD
git verify-commit HEAD
```

Result: signed merge `564d83835258ab48b9ea2ebcc867ba41e185822f`,
ordered parents `edf16ca9059158452798dbe5667f5e64ef444e39` then
`e21f9a6621df5527544be1de4d0dc60382539c60`, tree
`a08a66bb9edc858f7b87e72bff160c5cd8310186`, Good SSH signer
`jaystar524@gmail.com`, key
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

## Build and combined native gates

```powershell
$env:TECMO_SKIP_SHORTCUT = '1'
./build.ps1
./tools/Run-GameplayPreTipTests.ps1 -Build -RomPath $Rom
./tools/Run-GameplaySceneTests.ps1 -Build -RomPath $Rom
./tools/Run-IntroSequenceTests.ps1 -Build -RomPath $Rom
./tools/Run-SeasonTests.ps1 -RomPath $Rom
./tools/Run-GameplayAudioTests.ps1 -Build -RomPath $Rom
./tools/Run-GameplayCpuSteeringTests.ps1 -Build -RomPath $Rom
```

Personal results:

| Gate | UTC interval | Result |
|---|---|---|
| Full build | `2026-08-03T23:52:48.699Z..23:52:57.523Z` | PASS, exit 0, warning lines 0 |
| Focused TIP/TPTI-2 | `23:53:05.436Z..23:53:15.776Z` | PASS |
| Broad gameplay scene | `23:53:25.336Z..23:53:58.132Z` | PASS |
| Intro/current-main smoke | `2026-08-04T00:03:58.769Z..00:04:39.215Z` | PASS; one documented Bucks reference skip |
| Season/current-main smoke | `00:04:45.601Z..00:04:56.453Z` | PASS |
| Gameplay audio/current-main smoke | `00:05:02.901Z..00:05:19.711Z` | PASS |
| CPU steering/current-main smoke | `00:05:26.436Z..00:05:35.749Z` | PASS |
| Focused TIP/TPTI-2 after guidance correction | `00:39:58.162Z..00:40:08.335Z` | PASS, exit 0 |

The broad gameplay-scene runner created ignored
`build/live-proof-20260803T235325730Z`; it is not part of the tracked result.

## Win32 production launch

The first fresh-worktree launch failed closed because the worktree did not yet
contain the required ignored root `build/tecmo.assetpack`:

```text
Explicit console --root developer flow failed: ROM-derived settings cursor
anchors were not loaded
```

The canonical pack was then built from the fixed Rev1 ROM and the identical
smoke command was rerun:

```powershell
./build/tecmo_port.exe --build-assetpack $Rom ./build/tecmo.assetpack
./tools/Run-Win32LaunchSmokeTest.ps1 -ProjectRoot $ProjectRoot -DecompRoot $DecompRoot
```

Pack result: 86 entries, `1406713` bytes, SHA-256
`27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29`.
The second Win32 run passed
`2026-08-03T23:55:01.325Z..23:55:11.974Z`. The first failure was a
fresh-worktree prerequisite and is preserved rather than hidden as a pass.

## Deterministic TIP proof

```powershell
./tools/New-TipoffVisualProof.ps1 -ProjectRoot $ProjectRoot -RomPath $Rom -OutputRoot (Join-Path $ProjectRoot $InitialProofRoot) -FfmpegPath $Ffmpeg
```

Run interval: `2026-08-03T23:55:24.359Z..23:56:02.414Z`, exit 0.

- Manifest: `proof-manifest.json`, `597738` bytes, SHA-256
  `9207974C6A34A24B1468D531D5E0F22C48F28BE0DB28411083BB5495CC1E6798`.
- Summary: `proof-summary.txt`, `6406` bytes, SHA-256
  `C7F5162ED2298170E9C07A1495279543C54E03A62919FF49F86595227371595D`.
- Executable: `2045952` bytes, SHA-256
  `DD5064C85F41632508B4EFFD03799EA23653A17830AE936E25343222E7EECE01`.
- TPTI-2: `7680` bytes, FNV1a32 `28910BC1`, FNV1a64
  `7EA1596E8DFAC0C1`, SHA-256
  `C453848A33D6B29046D48ACDB44973D9A93234457C13F4150154F35DEA8F27FB`.
- Frames: exactly 65 first-pass and 65 independently rendered second-pass
  640x480 PNGs for logical frames `661..725`; mismatches `0`.
- Logs: 138 nonempty render/probe logs; warning/fatal/exception/failed/failure
  and `error:` scans `0`; no incomplete marker.
- Accepted-R1 comparison: 65 frame mismatches `0`; five media mismatches
  `0`.
- MP4 probe: 640x480, 65 frames, `39375000/655171` fps, duration
  `1.081552` seconds. The MP4 is presentation-only, not acceptance evidence.

## Original ASM and TPTI-2 audit

Read-only commands included:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath $Rom
rg -n '86E1|87F1|0761|0762|0763|0764|8817' $DecompRoot/decomp/lifted/bank04
rg -n '8351|839F|8642|9824|985E|98E1|9C79|9C7F|A214|A25F|A274|A2D2|A2D5' $DecompRoot/decomp/lifted/bank05
rg -n 'E537|E542|E56E|CD96|CDAB' $DecompRoot/decomp/lifted
rg -n 'TPTI-2|TGJS-2|A214|A2D5|E537|E56E|8642|98E1' src/asset_pack src/tecmo_gameplay_pretip.c src/tecmo_gameplay_scene.c tools/Run-GameplayPreTipTests.ps1
```

Direct ROM span extraction recalculated every FNV1a32/FNV1a64 value in
[EVIDENCE.md](EVIDENCE.md). Raw little-endian dispatch pointers independently
decoded as:

```text
selected actor state $1A -> $8642
selected actor state $22 -> $839F
opposing actor state $13 -> $985E
slot 10 state $1A -> $A25F
slot 10 state $1B -> $A274
```

The TPTI-2 source-map row contains 29 ordered roles and six exact same-pack
dependencies. A manual target-row audit covered all ten traceability rows with
issues `0`. The focused runner independently exercises canonical revision,
SHA/FNV32+64, source map, exact TGJS-2, missing/malformed/oversized/cross-pack
dependencies, stale TPTI-1, `$8642` false friend, `$A2D1` non-hook,
`$E537-$E542` ordering, recurring `$E56E` count, overlap, bounds, padding,
source mutation, transactional state, and deterministic rendering.

## Authorized guidance correction

```powershell
git verify-commit 9a3b4623022e3e4dc46142f5370f26c705bd9fe3
git diff --check
git add -- AGENTS.md PORTING.md
git commit -S -m 'docs: align TIP equal-error guidance'
git verify-commit 7ba0066ca1084e971a268d0b1b0176d065fdbd01
git diff-tree --no-commit-id --name-only -r 7ba0066ca1084e971a268d0b1b0176d065fdbd01
```

Result: master checkpoint and guidance commit both Good SSH. The guidance
commit's sole parent is `564d838...`, its tree is
`c8b1f48ef771ca566b5fcadec227b2d2daec6e2b`, and its exact paths are
`AGENTS.md` and `PORTING.md`.

## Live-main reconciliation and affected gates

Before the terminal report commit, local `main`, `origin/main`, and the live
remote main advanced together from `edf16ca...` to accepted R2A terminal
`8a5b9928544a430efa34cbf98a248d6a8cbe7b14`. Work stopped for the required
master checkpoint. After explicit authorization, the branch-only audit and
reconciliation used:

```powershell
git verify-commit 8a5b9928544a430efa34cbf98a248d6a8cbe7b14
git diff --name-only edf16ca9059158452798dbe5667f5e64ef444e39..8a5b9928544a430efa34cbf98a248d6a8cbe7b14
git diff --name-only edf16ca9059158452798dbe5667f5e64ef444e39..7ba0066ca1084e971a268d0b1b0176d065fdbd01
git merge-tree --write-tree 7ba0066ca1084e971a268d0b1b0176d065fdbd01 8a5b9928544a430efa34cbf98a248d6a8cbe7b14
git merge --no-ff -S 8a5b9928544a430efa34cbf98a248d6a8cbe7b14 -m 'Merge accepted R2A main into R1B TIP integration QA'
git show -s --format='%H%n%P%n%T%n%G?%n%GS%n%GK' HEAD
git verify-commit HEAD
```

Results: the accepted-main delta changed 24 paths, normalized overlap with the
R1B lineage was `0`, and the predicted tree was
`fb2e4cd08e5c20dfb5f4167853bd49ade6095780`. Good-SSH-signed reconciliation
`3aa7dfb523d6fee51785f845d023e7ea8a990074` has required ordered parents
`7ba0066ca1084e971a268d0b1b0176d065fdbd01` then
`8a5b9928544a430efa34cbf98a248d6a8cbe7b14` and exactly that tree. There was
no conflict resolution or manual product edit.

The affected regression commands included:

```powershell
$env:TECMO_SKIP_SHORTCUT = '1'
./build.ps1
./build/tecmo_port.exe --gameplay-state-test
./build/tecmo_port.exe --assetpack-test
./tools/Run-AssetPackTests.ps1 -Build -RomPath $Rom -AssetPackPath 'build/r1b-integration-qa-recon-3aa7dfb/tecmo.assetpack' -ReportPath 'build/r1b-integration-qa-recon-3aa7dfb/asset-pack-report.json'
./tools/Run-GameplayFreeThrowLineupTests.ps1 -Build -RomPath $Rom
./tools/Run-GameplayFatigueTests.ps1 -Build -RomPath $Rom
./tools/Run-GameplayPreTipTests.ps1 -Build -RomPath $Rom
./tools/Run-GameplaySceneTests.ps1 -Build -RomPath $Rom -ProofRootPath 'build/r1b-integration-qa-recon-3aa7dfb/gameplay-scene-proof'
./tools/Run-NativeFlowTests.ps1 -Build -DecompRoot $DecompRoot -ReportPath 'build/r1b-integration-qa-recon-3aa7dfb/native-flow-report.json'
./build/tecmo_port.exe --build-assetpack $Rom ./build/tecmo.assetpack
./tools/Run-Win32LaunchSmokeTest.ps1 -ProjectRoot $ProjectRoot -DecompRoot $DecompRoot
./tools/Run-SeasonTests.ps1 -RomPath $Rom
```

| Reconciliation gate | UTC interval | Result |
|---|---|---|
| Warning-clean full build | `2026-08-04T01:07:57.962Z..01:08:04.711Z` | PASS, exit 0, warning lines 0 |
| Direct gameplay-state / asset-pack | `01:08:22.363Z..01:08:22.531Z` | PASS; replay `7A204A525C79D21C` |
| Asset-pack suite | `01:08:32.021Z..01:08:39.210Z` | PASS, 55/55 |
| Focused free-throw lineup | `01:09:15.832Z..01:09:16.890Z` | PASS |
| Focused fatigue | `01:09:47.528Z..01:09:47.973Z` | PASS |
| Focused TIP/TPTI-2 | `01:10:01.863Z..01:10:10.051Z` | PASS |
| Broad gameplay scene | `01:10:20.075Z..01:10:40.365Z` | PASS |
| Native flow | `01:10:52.792Z..01:10:53.452Z` | PASS |
| Win32 production launch | `01:11:11.718Z..01:11:13.448Z` | PASS |
| Season flow | `01:11:25.404Z..01:11:27.564Z` | PASS |

The asset-pack suite generated an 86-entry, `1406713`-byte pack with SHA-256
`27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29`;
its `100118`-byte report has SHA-256
`4331027F483A5622D2FBA43807846C82763656639E21AEE39CCD67EE7EFA9EC1`.
The fatigue runner's first outer wrapper inherited exit `1` from an intentional
negative native vector even though the script printed PASS; a `$?`-based
rerun confirmed script success. Native-flow likewise finished PASS while its
last intentional negative child left a stale native exit value. Neither was a
product failure.

The reconciled deterministic proof ran
`2026-08-04T01:12:36.659Z..01:13:08.369Z` at `$ProofRoot`, exit 0. Because the
proof correctly requires a literally clean worktree, the five authorized
untracked report drafts were temporarily moved to
`C:/Users/joshs/Projects/tecmo-r1b-tip-docs-hold-3aa7dfb`, then restored with
five-of-five SHA-256 matches and the empty hold removed. Results:

- manifest `597738` bytes, SHA-256
  `1EAADF9972DB2751F5116A7F382389DEAFF0AB82232D67D215FC9AB9FE584493`;
- summary `6406` bytes, SHA-256
  `1AB2DDECF511EF1E1E17A84BE61C6D11B2902EC5EBF1DCAA3D44A11EFC2B5AB4`;
- executable `2079744` bytes, SHA-256
  `E8061BD573CB275CF419C3AA9BE1AAC6F24E4AB758E7656681844F0897F67CEF`;
- exactly 65 plus 65 deterministic frames, mismatch `0`;
- pre-reconciliation comparison: 65 frame mismatches `0`, five media
  mismatches `0`; and
- full-resolution Sol review of the current-tip contact sheet, frames 687,
  720, 721, facing checkpoint, and all previously reviewed boundaries: PASS.

## Terminal Git gate

The terminal handoff reruns:

```powershell
git ls-remote origin refs/heads/main
git rev-parse HEAD main origin/main codex/round-1b-tip-fidelity-staging
git verify-commit 564d83835258ab48b9ea2ebcc867ba41e185822f
git verify-commit 7ba0066ca1084e971a268d0b1b0176d065fdbd01
git verify-commit 3aa7dfb523d6fee51785f845d023e7ea8a990074
git verify-commit HEAD
git merge-base --is-ancestor 8a5b9928544a430efa34cbf98a248d6a8cbe7b14 HEAD
git merge-base --is-ancestor e21f9a6621df5527544be1de4d0dc60382539c60 HEAD
git merge-base --is-ancestor 3aa7dfb523d6fee51785f845d023e7ea8a990074 HEAD
git diff --check 8a5b9928544a430efa34cbf98a248d6a8cbe7b14..HEAD
git status --porcelain=v2 --untracked-files=all
```

Exact terminal HEAD/ref/signature/cleanliness results are supplied in the
master handoff because the report cannot self-record its own commit SHA.
