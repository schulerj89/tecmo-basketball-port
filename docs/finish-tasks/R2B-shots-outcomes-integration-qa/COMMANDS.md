# R2B commands and results

Private research inputs are represented by variables. They are test inputs only
and are not runtime dependencies or tracked artifacts.

```powershell
$ProjectRoot = '<R2B_WORKTREE>'
$Rom = '<CANONICAL_REV1_ROM>'
$DecompRoot = '<LOCAL_DECOMP_ROOT>'
$env:TECMO_SKIP_SHORTCUT = '1'
```

The canonical Rev1 iNES ROM was 393,232 bytes with SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.

## Clean takeover and immutable candidate

Representative read-only commands:

```powershell
git status --porcelain=v2 --untracked-files=all
git branch --show-current
git rev-parse HEAD main origin/main
git ls-remote origin refs/heads/main
git rev-parse codex/round-2b-shots-outcomes-staging
git merge-base 8a5b9928544a430efa34cbf98a248d6a8cbe7b14 `
  7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
git rev-list --reverse `
  222d75cfafa9153db1eb44492bf557f11b1a9091..7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
git diff --name-only `
  222d75cfafa9153db1eb44492bf557f11b1a9091..7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
git diff --check `
  222d75cfafa9153db1eb44492bf557f11b1a9091..7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
git merge-tree --write-tree `
  8a5b9928544a430efa34cbf98a248d6a8cbe7b14 `
  7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
```

Results: clean exact branch at `8a5b992...`; all three main observations equal;
immutable staging exact `7b9287a...`; candidate base exact `222d75cf...`; three
candidate commits; 17 candidate paths; old-main overlap zero; both diff checks
clean; predicted tree `2d918e8d...`; original exclusion blobs exact.

The first authorized mutation, after the durable takeover checkpoint, was:

```powershell
git merge --no-ff -S `
  7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a `
  -m 'merge: reconcile R2 shots outcomes with current main'
git show -s --format='%H %P %T %G? %GS %GK' HEAD
git verify-commit HEAD
```

Result: Good-signed `26e6aaf19b639972cb9043f29fc55daa1efce835`,
parents `8a5b992... 7b9287a...`, tree `2d918e8d...`.

## New-main reconciliation

After Good-signed control `981aa4...`, read-only preflight used:

```powershell
git status --porcelain=v2 --untracked-files=all
git rev-parse HEAD main origin/main
git ls-remote origin refs/heads/main
git verify-commit 981aa4e3b0aece8569b0be247d0e27ef88fa02c7
git verify-commit 0ef11cf247e3110b6064e79a4c496be6346f3e13
git merge-base 6eaaa535fa69a12e0f63012470dcc052583351b5 `
  0ef11cf247e3110b6064e79a4c496be6346f3e13
git diff --name-only 8a5b9928544a430efa34cbf98a248d6a8cbe7b14..6eaaa535fa69a12e0f63012470dcc052583351b5
git diff --name-only 8a5b9928544a430efa34cbf98a248d6a8cbe7b14..0ef11cf247e3110b6064e79a4c496be6346f3e13
git diff --check 8a5b9928544a430efa34cbf98a248d6a8cbe7b14..6eaaa535fa69a12e0f63012470dcc052583351b5
git diff --check 8a5b9928544a430efa34cbf98a248d6a8cbe7b14..0ef11cf247e3110b6064e79a4c496be6346f3e13
git merge-tree --write-tree --messages `
  6eaaa535fa69a12e0f63012470dcc052583351b5 `
  0ef11cf247e3110b6064e79a4c496be6346f3e13
```

Results: local/tracking/live main exact `0ef11cf...`; merge base `8a5b992...`;
R2B side 23 paths; new-main side 31 paths; normalized overlap zero; both diff
checks clean; conflict-free predicted tree `37bbb3868ee1b2b35fbaec1f7801213d648d7fb0`.

Accepted new main alone advanced the two former exclusion paths to blobs
`504d3d0459780779f47a533ce8bb548208a4195d` and
`40417d8544ce9ffaca7b7110f341fa82bd4b486f`. Read-only diff/source-map/name
checks confirmed TIP/TPTI-2-only changes and preserved R2 classifications.

After the durable pre-merge report to master:

```powershell
git merge --no-ff -S `
  -m 'merge: reconcile R2B shots outcomes with accepted TIP main' `
  0ef11cf247e3110b6064e79a4c496be6346f3e13
git show -s --format='%H%n%T%n%P%n%G?%n%GS%n%GK' HEAD
git verify-commit HEAD
git diff-tree --check HEAD^1 HEAD
```

Result: Good-signed `d3f1980d1d9147c47bd6a3bd555708ad6bfcb0f9`,
ordered parents `6eaaa535... 0ef11cf...`, exact predicted tree `37bbb386...`,
clean branch, both ancestors present.

## Reconciled build and focused gates

```powershell
$env:TECMO_SKIP_SHORTCUT = '1'
.\build.ps1
.\tools\Run-GameplayCloseShotTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayShotResolutionTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayPreTipTests.ps1 -ProjectRoot . -RomPath $Rom
```

Results:

- full `/W4` build exit 0, both executables, warning/error scan count zero;
- TGCS-1 canonical/provenance/reload, 208 poses, negative cases, 43 mutations;
- TGSR-3 four primary plus four lookup spans, point 1/2/3, polarity, raw
  routes, rattle 1..4, claimant settlement;
- TPTI-2 canonical/source-map/dependency/mutation plus input, abort/freeze,
  toss, jump, and live-render matrix.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/tecmo_port.exe` | 2,174,464 | `A03CB118A982AB6F7B35A68DB666F55D22D1839AAFF05EC3A864AFE272A92DA2` |
| `build/tecmo_port_game.exe` | 2,174,464 | `C2ECAED97ED4917095F75D00B34BF4F3A6048414163A572B66335FF44F7BD2AB` |

## Complete gameplay scene and direct state gates

```powershell
$SceneRoot = 'build\r2b-sol-recon-scene-20260804T021505Z'
.\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath $Rom `
  -ProofRootPath $SceneRoot
.\build\tecmo_port.exe --root $ProjectRoot --gameplay-scene-test `
  "$SceneRoot\asset-pack\gameplay-proof.assetpack"
.\build\tecmo_port.exe --gameplay-state-test
.\tools\Run-GameplayFreeThrowLineupTests.ps1 -ProjectRoot . -RomPath $Rom
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\Run-GameplayFatigueTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayCpuSteeringTests.ps1 -ProjectRoot . -RomPath $Rom
```

Results: broad scene PASS, direct scene PASS, gameplay-state PASS replay
`7A204A525C79D21C`, TGFL-1 PASS, TGFT-1 PASS, and TGAI-1 PASS.

Scene manifest SHA-256 is
`C6ABD02253659272F9DBFCF8FD76289D6C13E9B4F5C375BC22E2279ABF0FC54B`.
A separate inventory loop independently checked all 254 declared absolute
paths for root containment, uniqueness, existence, exact byte count, and
SHA-256. Result: zero failures; 255 physical files including the manifest.

The manifest's `DRAFT`, generic task, false build/pass flags, and
`PENDING_CLEAN_COMMIT` final SHA are preserved as an explicit caveat.

## Reconciled pack, flow, Win32, and cross-domain smokes

```powershell
$ReconRoot = 'build\r2b-sol-recon-20260804T021505Z'
.\tools\Run-AssetPackTests.ps1 -ProjectRoot . -RomPath $Rom `
  -AssetPackPath "$ReconRoot\tecmo.assetpack" `
  -ReportPath "$ReconRoot\asset-pack-report.json"
$env:TECMO_ASSETPACK = (Resolve-Path "$ReconRoot\tecmo.assetpack").Path
.\build\tecmo_port.exe --assetpack-test
.\build\tecmo_port.exe --root $DecompRoot --flow-test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\Run-NativeFlowTests.ps1 -ProjectRoot . `
  -DecompRoot $DecompRoot `
  -ReportPath "$ReconRoot\native-flow-report-isolated-bound.json"
.\tools\Run-Win32LaunchSmokeTest.ps1 -ProjectRoot . -DecompRoot $DecompRoot
Remove-Item Env:TECMO_ASSETPACK

.\tools\Run-IntroSequenceTests.ps1 -ProjectRoot . -RomPath $Rom `
  -AssetPackPath "$ReconRoot\intro.assetpack" `
  -ReportPath "$ReconRoot\intro-report.json"
.\tools\Run-MusicTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-FrontendAudioTests.ps1 -ProjectRoot . `
  -RomPath $Rom -DecompRoot $DecompRoot
.\tools\Run-GameplayAudioTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-SeasonTests.ps1 -ProjectRoot . -RomPath $Rom `
  -DecompRoot $DecompRoot -SkipBuild
.\tools\Run-TeamDataTests.ps1 -ProjectRoot . -RomPath $Rom `
  -DecompRoot $DecompRoot -SkipBuild
.\tools\Run-TeamManagementTests.ps1 -ProjectRoot . -RomPath $Rom `
  -DecompRoot $DecompRoot -SkipBuild
```

Results: 55/55 asset tests, direct asset-pack, bound direct flow, isolated
bound native flow exit 0, bound Win32, intro 29/29 with one accurately flagged
skip, music, frontend audio, gameplay audio, season, team data, and team
management all PASS.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `tecmo.assetpack` | 1,406,713 | `27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29` |
| `asset-pack-report.json` | 100,112 | `D6E1155A1CCC69B651B06580B2C1B39BDF5081AAEB3B1A78A3538D57B224F331` |
| `intro-report.json` | 272,374 | `CB681A508929422144A12D42C5A2A4AA06C8938146B73C9655B762DA97DCF757` |
| `native-flow-report-isolated-bound.json` | 897 | `932334D5AAB9CBA662A63218EB160C1B0A46C637D41B3090744595A362CD96B9` |

## Fresh deterministic shot proof

The ignored proof harness was parameterized with the reconciled pack and run:

```powershell
$ShotRoot = 'build\r2b-sol-recon-native-shots-20260804T022200Z'
.\build\r2b-sol-generate-native-shot-proof.ps1 `
  -OutputRoot $ShotRoot `
  -AssetPackPath 'build\r2b-sol-recon-20260804T021505Z\tecmo.assetpack'
```

Scenario map: make 11 frames, miss 15, rattle 16, dunk 9, rendered twice.
Result: all accepted frame aggregates exact and both fresh passes equal.

| Scenario | Aggregate SHA-256 | Fresh FFmpeg 8.1 MP4 SHA-256 |
| --- | --- | --- |
| jump make | `47D5B332BCFEFEA472C5CA4FDFCDC9A646FC5CE9E3FD208C6686807B2F74BB99` | `65CE0BD648DB1E458275B61E64A7E8D52F0472D842182B58BF50FB219A93E0B3` |
| jump miss | `B1E87ECF18121FE57348C46326C1C54A5657FB6633362A36AEAB852E030B9AEF` | `DF3000FDD81BDFE68620B2773EE4C5DD6E2E56E0F6C07563385A664533CEB7AC` |
| jump rattle | `9447A2693D285FE702B3C7D67CF7D554C4962CB3B7122F0845FE633C8230AF5A` | `7F87E6C5E4200C8C966D4ECCF3A7EEBFFC27C3609330EE1FF8C75CD86FBA4426` |
| dunk | `F2C128049E5E28BC9750F6A55011FE2B7065D97D9CFE18B329C43DCEA588CEFD` | `F2B60D386E169B62484B9ED7B15BAA8641D936382D8FEFE6C79593586C4884E3` |

Manifest SHA-256:
`301AEEF5610ACE250C888CEA8B666E6260A0B195C3822AF2B3BCB3B59AB477D3`.

A separate read-only auditor reconstructed all frame aggregates, opened every
PNG for dimensions, checked every declared SHA, compared both passes, verified
manifest head/tree and bound-pack hash, and compared complete physical and
declared inventories. Result: 119/119 files, zero errors.

Sol opened four pass-1 contact sheets and the intentional dunk black frame at
original detail. Visual result: P0=0/P1=0/P2=0.

## Harness and retry record

The complete non-product diagnostic ledger is in `FAULT-LEDGER.md`. Reconciled
runtime-facing retries were all pack-binding or wrapper-state issues:

1. native-flow printed every boundary PASS, but the caller read stale
   `$LASTEXITCODE=1` from an intentional negative vector;
2. the first isolated rerun omitted `TECMO_ASSETPACK` and correctly stopped at
   numeric rendering;
3. the first Win32 attempt removed the pack binding before its developer-flow
   subcheck;
4. an isolated bound native-flow run returned process exit 0 and the bound
   Win32 rerun passed every subsystem check.

No retry exposed a product regression.

## Signed-document and terminal guards

```powershell
git diff --check
git status --short --branch
git add -- docs/finish-tasks/R2B-shots-outcomes-integration-qa
git diff --cached --check
git diff --cached --name-only
git commit -S -m 'docs: reconcile R2B shots outcomes QA with TIP main'
git verify-commit HEAD
git merge-base --is-ancestor `
  0ef11cf247e3110b6064e79a4c496be6346f3e13 HEAD
git merge-base --is-ancestor `
  7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a HEAD
git merge-base --is-ancestor `
  d3f1980d1d9147c47bd6a3bd555708ad6bfcb0f9 HEAD
git diff --check 0ef11cf247e3110b6064e79a4c496be6346f3e13..HEAD
git status --porcelain=v2 --untracked-files=all
git rev-parse main origin/main
git ls-remote origin refs/heads/main
```

Good-signed candidate `fcc520998c206c2f244fab8d75b69fe8ac96bf64`, tree
`445b1615dea553b328c2dbf79bb88ee342943862`, changed exactly the six scoped
documents and was independently accepted by the same pinned Luna with
P0=0/P1=0/P2=0. Its only requested terminal addition was to record that result.
The final docs-only signed descendant is returned to the same Luna for a narrow
scope/wording verification. The terminal SHA and guarded fast-forward handoff
are sent to master because a commit cannot contain its own object ID.
