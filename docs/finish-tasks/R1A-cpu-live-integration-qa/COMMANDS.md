# Personal commands and results

## Fixed inputs

```powershell
$ProjectRoot = 'C:\Users\joshs\Projects\tecmo-basketball-port-r1a-cpu-live-integration-qa-sol'
$Rom = 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
$DecompRoot = 'C:\Users\joshs\Projects\disassem\tecmo-basketball-decompilation'
$OriginalManifest = 'C:\Users\joshs\Projects\tecmo-basketball-port-r1-cpu-play-lifecycle-luna\temp-videos\gameplay-lab\cpu-lifecycle\20260803-053244\proof-manifest.json'
$ProofRoot = 'build\r1a-integration-qa-final-20260803-f'
Set-Location -LiteralPath $ProjectRoot
```

The ROM is `393232` bytes, SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
The CPU manifest is `128484` bytes, SHA-256
`E7C9E6C9210D398DADC82715779A1389DF881643D109A0FDB091EBAFA523254A`.

## Temporal Git state and reconciliation

The frozen proof completed at `2026-08-03T18:21:09.9305271Z`, while `main`
and `origin/main` still resolved to
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`. Master later advanced both to
accepted R3A commit `dd096cb23a5fe7d755615fcaaadc0aa1d9b1509d` (authored
`2026-08-03T18:42:19Z`). That later ref movement does not rewrite the earlier
proof-time observation.

The exact reconciliation checks were:

```powershell
git branch --show-current
git rev-parse HEAD main origin/main codex/round-1-gameplay-foundation-staging
git merge-base HEAD origin/main
git status --porcelain=v2 --untracked-files=all
git diff --cached --name-status
git diff --name-only 6d8f9c7..222d75cf
git diff --name-only 6d8f9c7..dd096cb
git merge-tree 6d8f9c7 222d75cf dd096cb
git diff --check 6d8f9c7..222d75cf
git diff --check 6d8f9c7..dd096cb
```

Results: branch and frozen HEAD were exact; staging had the same SHA/tree;
merge-base was `6d8f9c7...`; R1A changed 43 paths, R3A changed 9 paths,
intersection 0; the 2,685-line merge-tree output contained zero conflict
indicator; index was clean; the only nonignored untracked paths were the five
authorized R1A draft reports.

Master authorized a branch-only merge. It was executed as:

```powershell
git merge --no-ff origin/main -m "Merge accepted R3A main into R1A integration QA"
git show -s --format=%P HEAD
git merge-base --is-ancestor 222d75cfafa9153db1eb44492bf557f11b1a9091 HEAD
git merge-base --is-ancestor dd096cb23a5fe7d755615fcaaadc0aa1d9b1509d HEAD
```

Intermediate merge commit: `f98fea320bf2340e0c6c9b226cfe6caa63196dd7`.
Parents, in order:
`222d75cfafa9153db1eb44492bf557f11b1a9091` and
`dd096cb23a5fe7d755615fcaaadc0aa1d9b1509d`. Both ancestry commands returned
0. At that observation `main` and `origin/main` remained `dd096cb...`.

Accepted R4A subsequently advanced both refs to
`bcacd5b6963f4db1a92c8db9b9770413505a0e98`. Master explicitly authorized a
second required reconciliation. The exact checks and merge were:

```powershell
git rev-parse HEAD main origin/main
git merge-base HEAD origin/main
git diff --name-only dd096cb..f98fea3
git diff --name-only dd096cb..bcacd5b
git merge-tree --write-tree --messages f98fea3 bcacd5b
git merge --no-ff origin/main -m "Merge accepted R4A main into R1A integration QA"
git show -s --format=%P HEAD
```

The R1A side changed 43 paths and R4A 18, with exact overlap 0. Git's native
merge-tree returned exit 0 and predicted tree
`2bc6641c19abad1266fd7a8b1f3d2ea1b28922ee`. Terminal merge
`351f446dddc96c34c838c5a9642a0be9d7f1411e` has ordered parents
`f98fea320bf2340e0c6c9b226cfe6caa63196dd7` and
`bcacd5b6963f4db1a92c8db9b9770413505a0e98`, and its tree exactly matches the
prediction. R1A/R3A/R4A ancestry commands all returned 0; `main` and
`origin/main` remained `bcacd5b...`.

## Warning-clean build and focused gates

```powershell
$env:TECMO_SKIP_SHORTCUT = '1'
.\build.ps1
.\tools\Run-GameplayCpuSteeringTests.ps1 -ProjectRoot '.' -RomPath $Rom -Build
.\tools\Run-GameplayMovementTests.ps1 -ProjectRoot '.' -RomPath $Rom
```

All exited 0. The build emitted no compiler warning and produced both console
and GUI executables. The CPU result was exactly 680 aligned commands, 24
handlers, and 17 ROM mutation rejections (7 lifecycle anchor/table). Movement
passed exact TGMO spans, normal/fast/slow/diagonal/boundary/Y-gate vectors,
transactional rejection, live integration, and 7 ROM mutations. Each fixed
scratch root was proven absent/contained before execution and absent afterward.

## Canonical merged pack and production flow

```powershell
.\build\tecmo_port.exe --build-assetpack $Rom 'build\r1a-final-flow.assetpack'
$env:TECMO_ASSETPACK = (Resolve-Path -LiteralPath 'build\r1a-final-flow.assetpack').Path
.\build\tecmo_port.exe --root $DecompRoot --flow-test
.\tools\Run-NativeFlowTests.ps1 -ProjectRoot '.' -DecompRoot $DecompRoot -Build `
  -ReportPath 'build\native_flow_r1a_final_report.json'
.\tools\Run-SeasonTests.ps1 -ProjectRoot '.' -RomPath $Rom `
  -DecompRoot $DecompRoot -AssetPackPath 'build\r1a-final-flow.assetpack' `
  -SkipBuild
```

The 86-entry pack is 1,401,618 bytes, SHA-256
`695EEB2D0101C5422B01790BD8D6B2A7607E758F396F429C2D2424AC6A26DE07`.
Direct flow returned
`FLOW TEST PASS: menu play-intro title start-game-menu preseason season quit`.
The native-flow wrapper passed the normal flow and every CLI boundary; its
897-byte sanitized report has SHA-256
`932334D5AAB9CBA662A63218EB160C1B0A46C637D41B3090744595A362CD96B9`.
Season returned the strict TSNS/TSAV/native-handoff/malformed-pack PASS and all
13 pixel checkpoints. The pack hash was unchanged afterward.

The accepted R4A gates were also rerun at the terminal combined tip:

```powershell
.\tools\Run-MusicTests.ps1 -ProjectRoot $ProjectRoot -RomPath $Rom
.\tools\Run-FrontendAudioTests.ps1 -ProjectRoot $ProjectRoot -RomPath $Rom `
  -DecompRoot $DecompRoot
.\tools\Run-GameplayAudioTests.ps1 -ProjectRoot $ProjectRoot -RomPath $Rom
```

They returned the exact `MUSIC TEST PASS`, `FRONTEND AUDIO TEST PASS`, and
`GAMEPLAY AUDIO TEST PASS` summaries for TMUS-1, TFSX-1, TSFX-1, and TDMC-1.

## Fresh merged-tip RequirePass proof

The five untracked draft reports were moved temporarily to the task's explicit
`work\R1A-cpu-live-integration-qa-draft-before-final-proof` directory so RequirePass
could observe a truly clean checkout. All five hashes were checked before the
move and after restoration; they were identical. The destination was required
absent, both resolved paths were containment-checked, and no tracked file was
moved.

The committed runner remained unchanged at SHA-256
`2CA1B98E2B49D803C7C7B0F34F84318C633727866A56CDCC00AC6BB8A34930B0`.
Its one LIVE-feature branch literal was adapted only in memory after an
exact-one-match check:

```powershell
$ScriptPath = '.\tools\Run-GameplaySceneTests.ps1'
$Source = Get-Content -Raw -LiteralPath $ScriptPath
$From = '$ExpectedBranch = "codex/r1-live-foundation-luna"'
$To = '$ExpectedBranch = "codex/r1a-cpu-live-integration-qa-sol"'
if ([regex]::Matches($Source, [regex]::Escape($From)).Count -ne 1) {
  throw 'Expected exactly one proof branch literal.'
}
$Adapted = $Source.Replace($From, $To)
& ([scriptblock]::Create($Adapted)) -ProjectRoot $ProjectRoot -RomPath $Rom `
  -Build -RequirePass -ProofRootPath $ProofRoot `
  -OriginalReferenceManifestPath $OriginalManifest
```

The root and fixed scene scratch were absent before execution. Git was clean.
Exit 0: all four scene suites passed, followed by `LIVE PROOF PASS`. Git was
still clean before the drafts were restored, the scene scratch was absent, and
the tracked runner hash was unchanged.

The terminal manifest is 575510 bytes, SHA-256
`F72DC259DFFD6B95B560088D591A059DE9F572ED30D2FA70E889A32977273F43`.
It records current/final `351f446...` and proof time
`2026-08-03T19:31:37.2029425Z` through
`2026-08-03T19:32:09.3616972Z`.

## Personal proof and media audit

PowerShell `ConvertFrom-Json`, canonical path containment, duplicate detection,
`Get-Item`, and `Get-FileHash -Algorithm SHA256` revalidated all 254 inventory
entries and 189 required logs. A recursive tree comparison proved exactly 255
files (inventory plus manifest) with no unlisted file. `System.Drawing`
rechecked 640x480 native frames and 1920x1440 contacts. Independent `ffprobe`
and `ffmpeg -f framemd5 -` reruns revalidated both videos' cadence, count, time
base, and all seven decoded hashes. Repeat state JSON and event invariants were
compared directly. Exact results are in [EVIDENCE.md](EVIDENCE.md).

Both contact sheets and every repeat-1 numbered frame were then opened at
original detail using the repository screenshot-QA checklist. Repeat 2 is
byte-identical.

## Win32 production launch smoke

```powershell
$env:TECMO_SKIP_SHORTCUT = '1'
.\tools\Run-Win32LaunchSmokeTest.ps1 -ProjectRoot '.' `
  -DecompRoot $DecompRoot -Build
```

Exit 0. GUI/console subsystem identity, shortcut arguments, working directory,
icon, roster-independent startup, window lifetime, explicit console developer
flow, and clean shutdown passed. The temporary ignored shortcut was removed.

## Terminal report verification

The report commit is made with `git commit -s`. After commit, the Sol runs
`git diff --cached --check`, `git diff --check`, exact-parent and both-parent
ancestry checks, path-scope/prohibited-artifact checks, and a clean
`git status --porcelain=v2 --untracked-files=all`. The exact terminal commit,
report SHA, current refs, and fast-forward instruction are supplied in the
master handoff rather than through a self-referential tracked edit.
